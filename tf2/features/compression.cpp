#include "compression.hpp"

ATLAS_NO_DESTROY netvar_compression g_compression{ };

const char *scope_name( prop_scope scope )
{
    switch ( scope )
    {
        case prop_scope::local:
            return "local";
        case prop_scope::non_local:
            return "remote";
        default:
            return "all";
    }
}

// narrows as the walk descends: once inside a local only table nothing below it can widen again
prop_scope narrow_scope( prop_scope current, const char *table_prop_name )
{
    if ( current != prop_scope::any )
    {
        return current;
    }

    if ( str::equals( table_prop_name, ATLAS_XORSTR( "localdata" ).get( ) ) ||
         str::equals( table_prop_name, ATLAS_XORSTR( "tflocaldata" ).get( ) ) )
    {
        return prop_scope::local;
    }

    if ( str::equals( table_prop_name, ATLAS_XORSTR( "nonlocaldata" ).get( ) ) ||
         str::equals( table_prop_name, ATLAS_XORSTR( "tfnonlocaldata" ).get( ) ) )
    {
        return prop_scope::non_local;
    }

    return prop_scope::any;
}

void gather_excludes( SendTable *send, exclude_list_t &out, i32 depth )
{
    if ( !send || !send->m_pProps || depth >= 16 )
    {
        return;
    }

    for ( i32 index{ }; index < send->m_nProps; ++index )
    {
        SendProp &prop{ send->m_pProps[ index ] };

        if ( prop.m_Flags & SPROP_EXCLUDE )
        {
            if ( !prop.m_pExcludeDTName || !prop.m_pVarName )
            {
                continue;
            }

            if ( out.m_count >= MAX_PROP_EXCLUDES )
            {
                DBGOUT( "compression: exclude list full at %u, the model will keep dead props",
                        static_cast< u32 >( MAX_PROP_EXCLUDES ) );
                return;
            }

            out.m_items[ out.m_count ].m_table = prop.m_pExcludeDTName;
            out.m_items[ out.m_count ].m_prop  = prop.m_pVarName;
            ++out.m_count;
            continue;
        }

        if ( prop.m_Type == DPT_DataTable && prop.m_pDataTable )
        {
            gather_excludes( prop.m_pDataTable, out, depth + 1 );
        }
    }
}

const char *encoding_name( prop_encoding encoding )
{
    switch ( encoding )
    {
        case prop_encoding::fixed_point:
            return "coord";
        case prop_encoding::normal:
            return "normal";
        case prop_encoding::scaled:
            return "scaled";
        default:
            return "none";
    }
}

float encoding_step( const quantiser_t &quantiser )
{
    switch ( quantiser.m_encoding )
    {
        case prop_encoding::fixed_point:
            return 1.0f / static_cast< float >( quantiser.m_denominator );
        case prop_encoding::normal:
            return 1.0f / static_cast< float >( NORMAL_DENOMINATOR );
        case prop_encoding::scaled:
            return 1.0f / quantiser.m_scale;
        default:
            return 0.0f;
    }
}

i32 round_to_code( float value )
{
    return static_cast< i32 >( value < 0.0f ? value - 0.5f : value + 0.5f );
}

usize component_count( SendPropType type )
{
    switch ( type )
    {
        case DPT_Vector:
            return 3;
        case DPT_VectorXY:
            return 2;
        default:
            return 1;
    }
}

float assign_range_multiplier( i32 bits, float range )
{
    const u32   high_value{ ( bits == 32 ) ? 0xFFFFFFFEu : ( ( 1u << static_cast< u32 >( bits ) ) - 1u ) };
    const float top{ static_cast< float >( high_value ) };

    float multiplier{ top / range };

    if ( static_cast< u32 >( multiplier * range ) <= high_value &&
         static_cast< double >( multiplier * range ) <= static_cast< double >( high_value ) )
    {
        return multiplier;
    }

    constexpr float squeeze[]{ 0.9999f, 0.99f, 0.9f, 0.8f, 0.7f };
    for ( const float factor : squeeze )
    {
        multiplier = ( top / range ) * factor;

        if ( static_cast< u32 >( multiplier * range ) <= high_value &&
             static_cast< double >( multiplier * range ) <= static_cast< double >( high_value ) )
        {
            return multiplier;
        }
    }

    // the server asserts here and gives up on the prop; we drop it instead of guessing
    return 0.0f;
}

quantiser_t build_quantiser( const SendProp *prop, SendPropType type )
{
    quantiser_t out{ };

    const i32 flags{ prop->m_Flags };

    // SendPropFloat zeroes m_nBits as soon as any of these is set, so the flag decides the encoding
    // on its own and the bit count below is only meaningful once none of them matched
    if ( flags & SPROP_COORD )
    {
        // A DPT_Vector goes through WriteBitVec3Coord
        out.m_encoding    = prop_encoding::fixed_point;
        out.m_denominator = COORD_DENOMINATOR;
        return out;
    }

    if ( flags & SPROP_COORD_MP )
    {
        out.m_encoding    = prop_encoding::fixed_point;
        out.m_denominator = COORD_DENOMINATOR;
        return out;
    }

    if ( flags & SPROP_COORD_MP_LOWPRECISION )
    {
        out.m_encoding    = prop_encoding::fixed_point;
        out.m_denominator = COORD_DENOMINATOR_LOWPRECISION;
        return out;
    }

    if ( flags & SPROP_COORD_MP_INTEGRAL )
    {
        out.m_encoding    = prop_encoding::fixed_point;
        out.m_denominator = 1;
        return out;
    }

    // full float on the wire, nothing was lost
    if ( flags & SPROP_NOSCALE )
    {
        return out;
    }

    if ( flags & SPROP_NORMAL )
    {
        // @ WriteBitVec3Normal only sends x and y plus a sign bit for z, which the reader rebuilds from
        // the other two; Per component quantisation is simply not what happened to z, so vectors are
        // left alone and only scalar normals are modelled;
        if ( type != DPT_Float )
        {
            return out;
        }

        out.m_encoding = prop_encoding::normal;
        return out;
    }

    if ( prop->m_nBits <= 0 || prop->m_nBits >= 32 )
    {
        return out;
    }

    const float range{ prop->m_fHighValue - prop->m_fLowValue };
    if ( range <= 0.0f )
    {
        return out;
    }

    const float scale{ assign_range_multiplier( prop->m_nBits, range ) };
    if ( scale <= 0.0f )
    {
        return out;
    }

    out.m_encoding     = prop_encoding::scaled;
    out.m_low          = prop->m_fLowValue;
    out.m_high         = prop->m_fHighValue;
    out.m_scale        = scale;
    out.m_decode_scale = static_cast< float >( ( 1u << static_cast< u32 >( prop->m_nBits ) ) - 1u ) / range;
    out.m_wraps        = ( prop->m_fLowValue == 0.0f ) && ( prop->m_fHighValue == 360.0f );

    return out;
}

SendProp *matching_send_prop( SendTable *send, const char *name )
{
    for ( i32 index{ }; index < send->m_nProps; ++index )
    {
        SendProp &prop{ send->m_pProps[ index ] };

        if ( prop.m_pVarName && str::equals( prop.m_pVarName, name ) )
        {
            return &prop;
        }
    }

    return nullptr;
}

SendTable *send_table_for( RecvTable *recv, bool verbose )
{
    if ( !recv || !recv->m_pNetTableName )
    {
        return nullptr;
    }

    if ( !recv->m_pDecoder )
    {
        if ( verbose )
        {
            DBGOUT( "compression: %s has no decoder, the signon tables never linked",
                    recv->m_pNetTableName );
        }

        return nullptr;
    }

    void **decoder{ reinterpret_cast< void ** >( recv->m_pDecoder ) };

    for ( usize slot{ }; slot < 8; ++slot )
    {
        if ( decoder[ slot ] != recv )
        {
            continue;
        }

        SendTable *send{ static_cast< SendTable * >( decoder[ slot + 1 ] ) };

        if ( !send || ( reinterpret_cast< uptr >( send ) & ( sizeof( void * ) - 1 ) ) ||
             reinterpret_cast< uptr >( send ) < 0x10000 )
        {
            if ( verbose )
            {
                DBGOUT( "compression: %s anchored at slot %u but slot %u holds 0x%llx, not a pointer",
                        recv->m_pNetTableName, static_cast< u32 >( slot ),
                        static_cast< u32 >( slot + 1 ),
                        static_cast< unsigned long long >( reinterpret_cast< uptr >( send ) ) );
            }

            return nullptr;
        }

        if ( !send->m_pProps || send->m_nProps <= 0 || send->m_nProps > 4096 )
        {
            if ( verbose )
            {
                DBGOUT( "compression: %s send table candidate claims %d props, rejected",
                        recv->m_pNetTableName, send->m_nProps );
            }

            return nullptr;
        }

        if ( !send->m_pNetTableName || !str::equals( send->m_pNetTableName, recv->m_pNetTableName ) )
        {
            if ( verbose )
            {
                DBGOUT( "compression: %s send table candidate names %s, CRecvDecoder layout changed",
                        recv->m_pNetTableName,
                        send->m_pNetTableName ? send->m_pNetTableName : "(null)" );
            }

            return nullptr;
        }

        DBGOUT( "compression: %s send table at decoder slot %u, %d props", recv->m_pNetTableName,
                static_cast< u32 >( slot + 1 ), send->m_nProps );

        return send;
    }

    if ( verbose )
    {
        DBGOUT( "compression: %s decoder never points back at its recv table in %u slots",
                recv->m_pNetTableName, static_cast< u32 >( 8 ) );
    }

    return nullptr;
}

bool is_compressed_class( const char *name )
{
    return str::equals( name, ATLAS_XORSTR( "CTFPlayer" ).get( ) );
}

bool exclude_list_t::contains( const char *table, const char *prop ) const
{
    if ( !table || !prop )
    {
        return false;
    }

    for ( usize index{ }; index < this->m_count; ++index )
    {
        if ( str::equals( this->m_items[ index ].m_table, table ) &&
             str::equals( this->m_items[ index ].m_prop, prop ) )
        {
            return true;
        }
    }

    return false;
}

i32 quantiser_t::encode( float value ) const
{
    switch ( this->m_encoding )
    {
        case prop_encoding::fixed_point:
        {
            const i32 magnitude{ static_cast< i32 >( math::abs( value ) *
                                                     static_cast< float >( this->m_denominator ) ) };
            return value < 0.0f ? -magnitude : magnitude;
        }
        case prop_encoding::normal:
        {
            const i32 magnitude{ math::smaller(
                static_cast< i32 >( math::abs( value ) * static_cast< float >( NORMAL_DENOMINATOR ) ),
                NORMAL_DENOMINATOR ) };
            return value < 0.0f ? -magnitude : magnitude;
        }
        case prop_encoding::scaled:
        {
            float ranged{ value };

            // only values the encoder could never have seen get wrapped; 360 itself is in range and
            // is a real decoder output, because the top code of a [0,360] prop reconstructs to
            // exactly 360; wrapping it to 0 would claim the top and bottom buckets are the same one
            if ( this->m_wraps && ( ranged < this->m_low || ranged > this->m_high ) )
            {
                const i32 turns{ static_cast< i32 >( ranged / 360.0f ) };
                ranged -= 360.0f * static_cast< float >( turns );

                if ( ranged < 0.0f )
                {
                    ranged += 360.0f;
                }
            }

            const float clamped{ math::clamp( ranged, this->m_low, this->m_high ) };
            return static_cast< i32 >( ( clamped - this->m_low ) * this->m_scale + 0.5f );
        }
        default:
            return 0;
    }
}

i32 quantiser_t::invert( float decoded ) const
{
    switch ( this->m_encoding )
    {
        case prop_encoding::fixed_point:
        {
            return round_to_code( decoded * static_cast< float >( this->m_denominator ) );
        }
        case prop_encoding::normal:
        {
            // 1/2047 is not exactly representable, so a decoded lattice point sits a few ulps off
            // fract * (1/2047) and truncation would land on fract - 1 half the time
            return round_to_code( decoded * static_cast< float >( NORMAL_DENOMINATOR ) );
        }
        case prop_encoding::scaled:
        {
            // DecodeFloat is low + range * code / (2^bits - 1); the encoder's m_scale never appears
            // in it, so this inversion is the only mapping guaranteed to give the code back
            return round_to_code( ( decoded - this->m_low ) * this->m_decode_scale );
        }
        default:
            return 0;
    }
}

bool quantiser_t::holds( float candidate, float decoded ) const
{
    if ( this->m_encoding == prop_encoding::none || candidate == decoded )
    {
        return false;
    }

    if ( this->m_encoding == prop_encoding::scaled && !this->m_wraps &&
         ( candidate < this->m_low || candidate > this->m_high ) )
    {
        return false;
    }

    return this->encode( candidate ) == this->invert( decoded );
}

const compressed_class_t *netvar_compression::model_for( const ClientClass *client_class ) const
{
    if ( !client_class || !client_class->m_pNetworkName )
    {
        return nullptr;
    }

    for ( usize index{ }; index < this->m_class_count; ++index )
    {
        const compressed_class_t &model{ this->m_classes[ index ] };

        if ( str::equals( model.m_name, client_class->m_pNetworkName ) )
        {
            return &model;
        }
    }

    return nullptr;
}

void netvar_compression::collect( RecvTable *recv, SendTable *send, compressed_class_t &out,
                                  const exclude_list_t &excluded, i32 base_offset, i32 depth,
                                  prop_scope scope )
{
    if ( !recv || !recv->m_pProps || !send || !send->m_pProps || depth >= 16 )
    {
        return;
    }

    for ( i32 index{ }; index < recv->m_nProps; ++index )
    {
        RecvProp &prop{ recv->m_pProps[ index ] };

        const char *name{ prop.m_pVarName };
        if ( !name )
        {
            continue;
        }

        if ( excluded.contains( send->m_pNetTableName, name ) )
        {
            continue;
        }

        // the engine pairs the two trees by name in MatchRecvPropsToSendProps_R, so doing the same
        // keeps us correct even where the two prop lists are ordered differently;
        SendProp *mirror{ matching_send_prop( send, name ) };
        if ( !mirror )
        {
            continue;
        }

        const i32 offset{ base_offset + prop.m_Offset };
        if ( prop.m_RecvType == DPT_DataTable )
        {
            if ( prop.m_pDataTable && mirror->m_pDataTable )
            {
                this->collect( prop.m_pDataTable, mirror->m_pDataTable, out, excluded, offset,
                               depth + 1, narrow_scope( scope, name ) );
            }

            continue;
        }

        if ( prop.m_RecvType != DPT_Float && prop.m_RecvType != DPT_Vector &&
             prop.m_RecvType != DPT_VectorXY )
        {
            continue;
        }

        const quantiser_t quantiser{ build_quantiser( mirror, prop.m_RecvType ) };
        if ( quantiser.m_encoding == prop_encoding::none )
        {
            continue;
        }

        bool seen{ };
        for ( usize existing{ }; existing < out.m_count; ++existing )
        {
            if ( out.m_props[ existing ].m_offset == offset && out.m_props[ existing ].m_scope == scope )
            {
                seen = true;
                break;
            }
        }

        if ( seen )
        {
            continue;
        }

        if ( out.m_count >= MAX_COMPRESSED_PROPS )
        {
            DBGOUT( "compression: %s exceeded %u props, dropping the rest", out.m_name,
                    static_cast< u32 >( MAX_COMPRESSED_PROPS ) );
            return;
        }

        compressed_prop_t &entry{ out.m_props[ out.m_count++ ] };
        entry.m_name      = name;
        entry.m_offset    = offset;
        entry.m_scope     = scope;
        entry.m_type      = prop.m_RecvType;
        entry.m_quantiser = quantiser;
    }
}

bool netvar_compression::build_class( ClientClass *client_class, compressed_class_t &out, bool verbose )
{
    RecvTable *recv{ client_class->m_pRecvTable };

    SendTable *send{ send_table_for( recv, verbose ) };
    if ( !send )
    {
        return false;
    }

    out.m_name  = client_class->m_pNetworkName;
    out.m_count = 0;

    exclude_list_t excluded{ };
    gather_excludes( send, excluded, 0 );

    this->collect( recv, send, out, excluded, 0, 0, prop_scope::any );

    if ( out.m_count == 0 )
    {
        DBGOUT( "compression: %s resolved but carries no quantised float or vector props", out.m_name );
        return false;
    }

    usize by_encoding[ 4 ]{ };
    for ( usize index{ }; index < out.m_count; ++index )
    {
        ++by_encoding[ static_cast< usize >( out.m_props[ index ].m_quantiser.m_encoding ) ];
    }

    DBGOUT( "compression: %s modelled %u props from %u exclusions (%u coord, %u scaled, %u normal)",
            out.m_name, static_cast< u32 >( out.m_count ), static_cast< u32 >( excluded.m_count ),
            static_cast< u32 >( by_encoding[ static_cast< usize >( prop_encoding::fixed_point ) ] ),
            static_cast< u32 >( by_encoding[ static_cast< usize >( prop_encoding::scaled ) ] ),
            static_cast< u32 >( by_encoding[ static_cast< usize >( prop_encoding::normal ) ] ) );

    for ( usize index{ }; index < out.m_count; ++index )
    {
        const compressed_prop_t &prop{ out.m_props[ index ] };

        DBGOUT( "compression:   +0x%04x %-34s %-6s %-6s step %.6f", prop.m_offset, prop.m_name,
                scope_name( prop.m_scope ), encoding_name( prop.m_quantiser.m_encoding ),
                encoding_step( prop.m_quantiser ) );
    }

    return true;
}

bool netvar_compression::ensure_built( )
{
    if ( this->m_available )
    {
        return true;
    }

    if ( this->m_attempts >= COMPRESSION_BUILD_ATTEMPTS )
    {
        return false;
    }

    ++this->m_attempts;
    const bool verbose{ this->m_attempts == 1 || this->m_attempts == COMPRESSION_BUILD_ATTEMPTS };

    this->m_class_count = 0;

    ClientClass *head{ g_tf2.m_client ? g_tf2.m_client->GetAllClasses( ) : nullptr };

    for ( ClientClass *iter{ head }; iter; iter = iter->m_pNext )
    {
        if ( !iter->m_pNetworkName || !is_compressed_class( iter->m_pNetworkName ) )
        {
            continue;
        }

        if ( this->m_class_count >= MAX_COMPRESSED_CLASSES )
        {
            break;
        }

        if ( this->build_class( iter, this->m_classes[ this->m_class_count ], verbose ) )
        {
            ++this->m_class_count;
        }
    }

    this->m_available = this->m_class_count > 0;

    if ( this->m_available )
    {
        DBGOUT( "compression: online after %u attempts, %u classes",
                static_cast< u32 >( this->m_attempts ), static_cast< u32 >( this->m_class_count ) );
    } else if ( verbose )
    {
        DBGOUT( "compression: gave up after %u attempts, staying off",
                static_cast< u32 >( this->m_attempts ) );
    }

    return this->m_available;
}

void netvar_compression::clear( )
{
    if ( this->m_total_corrections )
    {
        DBGOUT( "compression: %u components restored this level",
                static_cast< u32 >( this->m_total_corrections ) );
    }

    for ( snapshot_t &snapshot : this->m_ring )
    {
        snapshot.m_command = -1;
    }

    this->m_class_count       = 0;
    this->m_attempts          = 0;
    this->m_available         = false;
    this->m_armed             = false;
    this->m_acked             = -1;
    this->m_total_corrections = 0;
    this->m_logged            = 0;
    this->m_logged_mask[ 0 ]  = 0;
    this->m_logged_mask[ 1 ]  = 0;
    this->m_logged_mask[ 2 ]  = 0;
    this->m_window            = 0.0f;
}

void netvar_compression::on_run_simulation( C_BasePlayer *player, i32 command )
{
    if ( !player || command < 0 || !this->ensure_built( ) )
    {
        return;
    }

    const compressed_class_t *model{ this->model_for( player->GetClientClass( ) ) };
    if ( !model )
    {
        return;
    }

    snapshot_t &snapshot{ this->m_ring[ command & COMPRESSION_RING_MASK ] };
    snapshot.m_command = command;

    const u8 *base{ reinterpret_cast< const u8 * >( player ) };

    for ( usize prop{ }; prop < model->m_count; ++prop )
    {
        const compressed_prop_t &entry{ model->m_props[ prop ] };

        const float *field{ reinterpret_cast< const float * >( base + entry.m_offset ) };
        const usize  count{ component_count( entry.m_type ) };

        for ( usize component{ }; component < count; ++component )
        {
            snapshot.m_values[ prop ][ component ] = field[ component ];
        }
    }
}

void netvar_compression::on_packet_start( i32 acked_command )
{
    this->m_acked = acked_command;
    this->m_armed = true;
}

void netvar_compression::restore( )
{
    if ( !this->m_available || !this->m_armed )
    {
        return;
    }

    this->m_armed = false;

    C_TFPlayer *local{ g_tf2.m_entity_list->GetClientEntity( g_tf2.m_engine->GetLocalPlayer( ) )
                           ->As< C_TFPlayer * >( ) }; // doing this to avoid crashes..
    if ( !local )
    {
        return;
    }

    const compressed_class_t *model{ this->model_for( local->GetClientClass( ) ) };
    if ( !model )
    {
        return;
    }

    // a miss means this client never predicted the acked command (fresh connect, flushed entity
    // packet, a tickbase shift); there is nothing aligned to compare then
    const snapshot_t &snapshot{ this->m_ring[ this->m_acked & COMPRESSION_RING_MASK ] };
    if ( snapshot.m_command != this->m_acked )
    {
        return;
    }

    usize corrected{ };

    const float now{ g_tf2.m_globals->realtime };
    if ( now - this->m_window >= 2.0f )
    {
        if ( this->m_total_corrections != this->m_window_mark )
        {
            DBGOUT( "compression: %u components restored in the last 2s (%u total)",
                    static_cast< u32 >( this->m_total_corrections - this->m_window_mark ),
                    static_cast< u32 >( this->m_total_corrections ) );
        }

        this->m_window      = now;
        this->m_window_mark = this->m_total_corrections;
        this->m_logged      = 0;

        this->m_logged_mask[ 0 ] = 0;
        this->m_logged_mask[ 1 ] = 0;
        this->m_logged_mask[ 2 ] = 0;
    }

    u8 *base{ reinterpret_cast< u8 * >( local ) };

    for ( usize prop{ }; prop < model->m_count; ++prop )
    {
        const compressed_prop_t &entry{ model->m_props[ prop ] };

        // the local player never receives the non local branch, so its lattice never applied here;
        if ( entry.m_scope == prop_scope::non_local )
        {
            continue;
        }

        float      *field{ reinterpret_cast< float * >( base + entry.m_offset ) };
        const usize count{ component_count( entry.m_type ) };

        for ( usize component{ }; component < count; ++component )
        {
            const float candidate{ snapshot.m_values[ prop ][ component ] };

            if ( !entry.m_quantiser.holds( candidate, field[ component ] ) )
            {
                continue;
            }

            const u64 seen_bit{ 1ull << prop };
            if ( this->m_logged < 8 && !( this->m_logged_mask[ component ] & seen_bit ) )
            {
                this->m_logged_mask[ component ] |= seen_bit;
                ++this->m_logged;

                DBGOUT( "compression: %s.%u %.6f -> %.6f at cmd %d, inside one %.6f step",
                        entry.m_name, static_cast< u32 >( component ), field[ component ], candidate,
                        this->m_acked, encoding_step( entry.m_quantiser ) );
            }

            field[ component ] = candidate;
            ++corrected;
        }
    }

    this->m_total_corrections += corrected;
}
