#include "doors.hpp"

#include "entity_cache.hpp"
#include "tickbase.hpp"

ATLAS_NO_DESTROY doors g_doors{ };

constexpr i32 DOOR_FRAME_LAG{ 1 };

constexpr f32 DOOR_END_TOLERANCE{ 2.0f / 32.0f };

ATLAS_FORCE_INLINE f32 read_float( string_view &text )
{
    usize at{ };
    while ( at < text.size( ) && str::is_space( text[ at ] ) )
    {
        ++at;
    }

    f64 sign{ 1.0 };
    if ( at < text.size( ) && ( text[ at ] == '-' || text[ at ] == '+' ) )
    {
        if ( text[ at ] == '-' )
        {
            sign = -1.0;
        }
        ++at;
    }

    f64 value{ };
    while ( at < text.size( ) && str::is_digit( text[ at ] ) )
    {
        value = value * 10.0 + static_cast< f64 >( text[ at++ ] - '0' );
    }

    if ( at < text.size( ) && text[ at ] == '.' )
    {
        f64 scale{ 0.1 };
        for ( ++at; at < text.size( ) && str::is_digit( text[ at ] ); ++at )
        {
            value += static_cast< f64 >( text[ at ] - '0' ) * scale;
            scale *= 0.1;
        }
    }

    text.remove_prefix( at );
    return static_cast< f32 >( sign * value );
}

ATLAS_FORCE_INLINE f32 parse_float( string_view text )
{
    return read_float( text );
}

ATLAS_FORCE_INLINE i32 parse_int( string_view text )
{
    return static_cast< i32 >( read_float( text ) );
}

ATLAS_FORCE_INLINE vec3 parse_vector( string_view text )
{
    const f32 x{ read_float( text ) };
    const f32 y{ read_float( text ) };
    const f32 z{ read_float( text ) };
    return vec3{ x, y, z };
}

ATLAS_FORCE_INLINE hash_t hash_view( string_view text )
{
    return hash::make_hash( text.data( ), text.size( ), true );
}

ATLAS_FORCE_INLINE i32 brush_model( string_view model )
{
    if ( model.size( ) < 2 || model[ 0 ] != '*' )
    {
        return -1;
    }

    model.remove_prefix( 1 );
    return parse_int( model );
}

ATLAS_FORCE_INLINE bool model_bounds( i32 model, vec3 &mins, vec3 &maxs )
{
    char name[ 16 ]{ };
    atl::format( name, sizeof( name ), "*%d", model );

    const i32 index{ g_tf2.m_model_info->GetModelIndex( name ) };
    if ( index < 0 )
    {
        return false;
    }

    const model_t *brush{ g_tf2.m_model_info->GetModel( index ) };
    if ( !brush )
    {
        return false;
    }

    g_tf2.m_model_info->GetModelBounds( brush, mins, maxs );
    return true;
}

ATLAS_FORCE_INLINE bool split_output( string_view value, string_view *fields, usize count )
{
    for ( usize index{ }; index < count; ++index )
    {
        usize end{ };
        while ( end < value.size( ) && value[ end ] != ',' && value[ end ] != '\x1b' )
        {
            ++end;
        }

        fields[ index ] = value.substr( 0, end );
        if ( end >= value.size( ) )
        {
            return index + 1 == count;
        }
        value.remove_prefix( end + 1 );
    }
    return true;
}

ATLAS_FORCE_INLINE bool names_match( string_view target, string_view name )
{
    if ( name.empty( ) || target.empty( ) )
    {
        return false;
    }

    if ( target.back( ) != '*' )
    {
        return target.equals_ignore_case( name );
    }

    target.remove_suffix( 1 );
    return name.size( ) >= target.size( ) && name.substr( 0, target.size( ) ).equals_ignore_case( target );
}

ATLAS_FORCE_INLINE bool is_gating_input( string_view input )
{
    switch ( hash_view( input ) )
    {
        case HASH_CT( "Enable" ):
        case HASH_CT( "Disable" ):
        case HASH_CT( "DisableAndEndTouch" ):
        case HASH_CT( "Toggle" ):
        case HASH_CT( "Kill" ):
        case HASH_CT( "KillHierarchy" ): return true;
        default: break;
    }
    return false;
}

ATLAS_FORCE_INLINE void store_key( door_lump_entity_t &entity, string_view key, string_view value )
{
    if ( key.size( ) > 2 && str::to_lower( key[ 0 ] ) == 'o' && str::to_lower( key[ 1 ] ) == 'n' )
    {
        if ( entity.m_output_count < MAX_LUMP_OUTPUTS )
        {
            entity.m_output_names[ entity.m_output_count ] = key;
            entity.m_outputs[ entity.m_output_count ]      = value;
            ++entity.m_output_count;
        }
        return;
    }

    switch ( hash_view( key ) )
    {
        case HASH_CT( "classname" ): entity.m_classname = value; break;
        case HASH_CT( "targetname" ): entity.m_targetname = value; break;
        case HASH_CT( "parentname" ): entity.m_parentname = value; break;
        case HASH_CT( "filtername" ): entity.m_filtername = value; break;
        case HASH_CT( "model" ): entity.m_model = value; break;
        case HASH_CT( "origin" ): entity.m_origin = value; break;
        case HASH_CT( "movedir" ): entity.m_movedir = value; break;
        case HASH_CT( "speed" ): entity.m_speed = value; break;
        case HASH_CT( "lip" ): entity.m_lip = value; break;
        case HASH_CT( "wait" ): entity.m_wait = value; break;
        case HASH_CT( "spawnflags" ): entity.m_spawnflags = value; break;
        case HASH_CT( "forceclosed" ): entity.m_force_closed = value; break;
        case HASH_CT( "TeamNum" ): entity.m_team = value; break;
        case HASH_CT( "Negated" ): entity.m_negated = value; break;
        case HASH_CT( "controlpoint" ): entity.m_control_point = value; break;
        case HASH_CT( "StartDisabled" ): entity.m_start_disabled = value; break;
        default: break;
    }
}

ATLAS_FORCE_INLINE bool read_quoted( string_view &cursor, string_view &out )
{
    const usize open{ cursor.find( '"' ) };
    if ( open == str::npos )
    {
        return false;
    }
    cursor.remove_prefix( open + 1 );

    const usize close{ cursor.find( '"' ) };
    if ( close == str::npos )
    {
        return false;
    }

    out = cursor.substr( 0, close );
    cursor.remove_prefix( close + 1 );
    return true;
}

ATLAS_FORCE_INLINE bool next_entity( string_view &cursor, door_lump_entity_t &entity )
{
    const usize open{ cursor.find( '{' ) };
    if ( open == str::npos )
    {
        return false;
    }

    cursor.remove_prefix( open + 1 );
    entity = door_lump_entity_t{ };

    for ( ;; )
    {
        const usize next{ cursor.find_first_of( string_view{ "\"}" } ) };
        if ( next == str::npos )
        {
            return false;
        }

        if ( cursor[ next ] == '}' )
        {
            cursor.remove_prefix( next + 1 );
            return true;
        }

        string_view key{ }, value{ };
        if ( !read_quoted( cursor, key ) || !read_quoted( cursor, value ) )
        {
            return false;
        }
        store_key( entity, key, value );
    }
}

ATLAS_FORCE_INLINE void read_filter( const door_lump_entity_t &entity, door_lump_names_t &names )
{
    if ( names.m_filter_count >= MAX_DOOR_FILTERS ||
         entity.m_targetname.empty( ) ||
         parse_int( entity.m_negated ) != 0 ||
         !entity.m_control_point.empty( ) )
    {
        return;
    }

    names.m_filters[ names.m_filter_count ]      = entity.m_targetname;
    names.m_filter_teams[ names.m_filter_count ] = parse_int( entity.m_team );
    ++names.m_filter_count;
}

ATLAS_FORCE_INLINE void read_gates( const door_lump_entity_t &entity, door_lump_names_t &names )
{
    for ( usize index{ }; index < entity.m_output_count; ++index )
    {
        string_view fields[ 5 ]{ };
        if ( !split_output( entity.m_outputs[ index ], fields, 5 ) || !is_gating_input( fields[ 1 ] ) ||
             names.m_gated_count >= MAX_DOOR_GATED )
        {
            continue;
        }

        names.m_gated[ names.m_gated_count++ ] = fields[ 0 ];
    }
}

ATLAS_FORCE_INLINE bool trigger_usable( const door_lump_entity_t &entity, const door_lump_names_t &names, i32 &team )
{
    team = 0;

    if ( !( parse_int( entity.m_spawnflags ) & SF_TRIGGER_ALLOW_CLIENTS ) ||
         parse_int( entity.m_start_disabled ) != 0 )
    {
        return false;
    }

    for ( usize index{ }; index < names.m_gated_count; ++index )
    {
        if ( names_match( names.m_gated[ index ], entity.m_targetname ) )
        {
            return false;
        }
    }

    if ( entity.m_filtername.empty( ) )
    {
        return true;
    }

    for ( usize index{ }; index < names.m_filter_count; ++index )
    {
        if ( names.m_filters[ index ].equals_ignore_case( entity.m_filtername ) )
        {
            team = names.m_filter_teams[ index ];
            return true;
        }
    }

    return false;
}

ATLAS_FORCE_INLINE bool add_link( door_t &door, u8 trigger, bool all, bool opens, bool closes )
{
    usize link{ };
    while ( link < door.m_link_count && door.m_links[ link ].m_trigger != trigger )
    {
        ++link;
    }

    if ( link >= MAX_DOOR_LINKS )
    {
        return false;
    }

    if ( link == door.m_link_count )
    {
        door.m_links[ door.m_link_count++ ] = door_link_t{ trigger };
    }

    door.m_links[ link ].m_all    = door.m_links[ link ].m_all || all;
    door.m_links[ link ].m_opens  = door.m_links[ link ].m_opens || opens;
    door.m_links[ link ].m_closes = door.m_links[ link ].m_closes || closes;
    return true;
}

ATLAS_FORCE_INLINE bool near_end( const vec3 &a, const vec3 &b )
{
    return math::abs( a.x - b.x ) <= DOOR_END_TOLERANCE && math::abs( a.y - b.y ) <= DOOR_END_TOLERANCE &&
           math::abs( a.z - b.z ) <= DOOR_END_TOLERANCE;
}

ATLAS_FORCE_INLINE bool boxes_overlap( const vec3 &mins_a, const vec3 &maxs_a, const vec3 &mins_b, const vec3 &maxs_b )
{
    return maxs_a.x > mins_b.x && mins_a.x < maxs_b.x && maxs_a.y > mins_b.y && mins_a.y < maxs_b.y && maxs_a.z > mins_b.z &&
           mins_a.z < maxs_b.z;
}

ATLAS_FORCE_INLINE bool any_member( const door_members_t &members )
{
    for ( const u64 bits : members.m_bits )
    {
        if ( bits )
        {
            return true;
        }
    }
    return false;
}

ATLAS_FORCE_INLINE void add_member( door_members_t &members, i32 index )
{
    members.m_bits[ index / 64 ] |= u64{ 1 } << ( index % 64 );
}

ATLAS_FORCE_INLINE door_state_t classify( const door_t &door, const vec3 &received, const vec3 &previous )
{
    if ( near_end( received, door.m_pos1 ) )
    {
        return door_state_t::closed;
    }

    if ( near_end( received, door.m_pos2 ) )
    {
        return door_state_t::open;
    }

    if ( received == previous )
    {
        return door_state_t::unknown;
    }

    if ( ( received - previous ).dot( door.m_dir ) > 0.0f )
    {
        return door_state_t::opening;
    }
    return door_state_t::closing;
}

void doors::on_post_data_update( )
{
    if ( !g_cl.m_local )
    {
        return;
    }

    if ( !this->m_loaded )
    {
        this->m_loaded = this->load_map( );
        if ( !this->m_loaded )
        {
            return;
        }
    }

    const i32 tick{ g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick };
    if ( tick == this->m_snapshot_tick )
    {
        return;
    }

    this->m_previous_tick = this->m_snapshot_tick;
    this->m_snapshot_tick = tick;
    this->m_local_index   = g_tf2.m_engine->GetLocalPlayer( );
    this->m_local_team    = g_cl.m_local->m_iTeamNum( );
    this->m_team_win      = TFGameRules( )->m_iRoundState( ) == GR_STATE_TEAM_WIN;

    this->bind( );
    this->observe( tick );
    this->observe_players( tick );
}

void doors::pose( i32 command )
{
    const i32 frame{ g_tickbase.command_frame( command ) };
    if ( !this->m_loaded || frame < 0 )
    {
        return;
    }

    for ( usize index{ }; index < this->m_door_count; ++index )
    {
        door_t       &door{ this->m_doors[ index ] };
        C_BaseEntity *entity{ g_entity_cache.resolve( door.m_handle ) };
        if ( !entity || entity->IsDormant( ) || door.m_snapshot_tick < 0 )
        {
            continue;
        }

        const vec3  pose{ this->pose_at( door, frame - DOOR_FRAME_LAG ) };
        const vec3 &current{ entity->GetAbsOrigin( ) };
        if ( pose == current )
        {
            continue;
        }

        Assert( !door.m_posed );
        door.m_saved = current;
        door.m_posed = true;
        g_tf2.m_CBaseEntity_SetAbsOrigin.call_as< void, C_BaseEntity *, const vec3 * >( entity, &pose );
    }
}

void doors::on_command_predicted( i32 command, C_BasePlayer *player )
{
    const i32 frame{ g_tickbase.command_frame( command ) };
    if ( !this->m_loaded || frame < 0 )
    {
        return;
    }

    this->m_local[ static_cast< usize >( frame ) % DOOR_FRAME_RING ] =
        door_local_t{ player->m_vecOrigin( ), player->m_vecMins( ), player->m_vecMaxs( ), frame };
}

void doors::restore( )
{
    for ( usize index{ }; index < this->m_door_count; ++index )
    {
        door_t &door{ this->m_doors[ index ] };
        if ( !door.m_posed )
        {
            continue;
        }

        door.m_posed = false;
        if ( C_BaseEntity *entity{ g_entity_cache.resolve( door.m_handle ) } )
        {
            g_tf2.m_CBaseEntity_SetAbsOrigin.call_as< void, C_BaseEntity *, const vec3 * >( entity, &door.m_saved );
        }
    }
}

void doors::clear( )
{
    *this = doors{ };
}

bool doors::load_map( )
{
    const char *text{ g_tf2.m_engine->GetMapEntitiesString( ) };
    if ( !text )
    {
        return false;
    }

    door_lump_names_t  names{ };
    door_lump_entity_t entity{ };

    for ( string_view cursor{ text }; next_entity( cursor, entity ); )
    {
        read_gates( entity, names );

        switch ( hash_view( entity.m_classname ) )
        {
            case HASH_CT( "func_door" ): this->read_door( entity, names ); break;
            case HASH_CT( "filter_activator_tfteam" ): read_filter( entity, names ); break;
            default: break;
        }
    }

    for ( string_view cursor{ text }; next_entity( cursor, entity ); )
    {
        if ( hash_view( entity.m_classname ) == HASH_CT( "trigger_multiple" ) )
        {
            this->read_trigger( entity, names );
            continue;
        }

        this->unpredict_targets( entity, names );
    }

    return true;
}

void doors::read_door( const door_lump_entity_t &entity, door_lump_names_t &names )
{
    const i32 flags{ parse_int( entity.m_spawnflags ) };
    const i32 model{ brush_model( entity.m_model ) };
    if ( this->m_door_count >= MAX_DOORS ||
         ( flags & ( SF_DOOR_NONSOLID_TO_PLAYER | SF_DOOR_PASSABLE ) ) ||
         !entity.m_parentname.empty( ) ||
         model < 0 )
    {
        return;
    }

    vec3 mins{ }, maxs{ };
    if ( !model_bounds( model, mins, maxs ) )
    {
        return;
    }

    door_t &door{ this->m_doors[ this->m_door_count ] };
    door         = door_t{ };
    door.m_model = model;
    door.m_pos1  = parse_vector( entity.m_origin );
    door.m_mins  = mins + vec3{ 1.0f, 1.0f, 1.0f };
    door.m_maxs  = maxs - vec3{ 1.0f, 1.0f, 1.0f };
    math::angle_vectors( parse_vector( entity.m_movedir ), &door.m_dir );

    const vec3 size{ maxs - mins - vec3{ 2.0f, 2.0f, 2.0f } };
    const f32  travel{ math::abs( door.m_dir.x * size.x ) + math::abs( door.m_dir.y * size.y ) + math::abs( door.m_dir.z * size.z ) -
                      parse_float( entity.m_lip ) };
    door.m_pos2 = door.m_pos1 + door.m_dir * travel;

    f32 speed{ parse_float( entity.m_speed ) };
    if ( speed == 0.0f )
    {
        speed = 100.0f;
    }
    door.m_step = speed * TICK_INTERVAL;

    const f32  wait{ parse_float( entity.m_wait ) };
    const bool auto_returns{ wait >= 0.0f && !( flags & SF_DOOR_NO_AUTO_RETURN ) };
    door.m_reverses  = wait >= 0.0f;
    door.m_predicted = !( flags & ( SF_DOOR_PTOUCH | SF_DOOR_LOCKED ) ) && parse_int( entity.m_force_closed ) == 0 && !auto_returns;

    names.m_doors[ this->m_door_count ] = entity.m_targetname;
    ++this->m_door_count;
}

void doors::read_trigger( const door_lump_entity_t &entity, const door_lump_names_t &names )
{
    i32        team{ };
    const bool usable{ trigger_usable( entity, names, team ) };

    vec3       mins{ }, maxs{ };
    const i32  model{ brush_model( entity.m_model ) };
    bool       bounded{ false };
    if ( usable && model >= 0 )
    {
        bounded = model_bounds( model, mins, maxs );
    }

    const vec3 origin{ parse_vector( entity.m_origin ) };
    const u8   trigger{ static_cast< u8 >( this->m_trigger_count ) };

    for ( usize index{ }; index < entity.m_output_count; ++index )
    {
        string_view fields[ 5 ]{ };
        if ( !split_output( entity.m_outputs[ index ], fields, 5 ) )
        {
            continue;
        }

        const hash_t output{ hash_view( entity.m_output_names[ index ] ) };
        const hash_t input{ hash_view( fields[ 1 ] ) };
        const bool   all{ output == HASH_CT( "OnStartTouchAll" ) };
        const bool   opens{ input == HASH_CT( "Open" ) && ( all || output == HASH_CT( "OnStartTouch" ) ) };
        const bool   closes{ input == HASH_CT( "Close" ) && output == HASH_CT( "OnEndTouchAll" ) };
        const f32    delay{ parse_float( fields[ 3 ] ) };

        for ( usize at{ }; at < this->m_door_count; ++at )
        {
            door_t &door{ this->m_doors[ at ] };
            if ( !names_match( fields[ 0 ], names.m_doors[ at ] ) )
            {
                continue;
            }

            if ( !bounded ||
                 ( !opens && !closes ) ||
                 delay != 0.0f ||
                 this->m_trigger_count >= MAX_DOOR_TRIGGERS )
            {
                door.m_predicted = false;
                continue;
            }

            if ( !add_link( door, trigger, all, opens, closes ) )
            {
                door.m_predicted = false;
                continue;
            }

            if ( this->m_trigger_count == trigger )
            {
                this->m_triggers[ trigger ] = door_trigger_t{ origin + mins + vec3{ 1.0f, 1.0f, 1.0f },
                                                              origin + maxs - vec3{ 1.0f, 1.0f, 1.0f }, team };
                ++this->m_trigger_count;
            }
        }
    }
}

void doors::unpredict_targets( const door_lump_entity_t &entity, const door_lump_names_t &names )
{
    for ( usize index{ }; index < entity.m_output_count; ++index )
    {
        string_view fields[ 5 ]{ };
        if ( !split_output( entity.m_outputs[ index ], fields, 5 ) )
        {
            continue;
        }

        for ( usize at{ }; at < this->m_door_count; ++at )
        {
            if ( names_match( fields[ 0 ], names.m_doors[ at ] ) )
            {
                this->m_doors[ at ].m_predicted = false;
            }
        }
    }
}

void doors::bind( )
{
    for ( const entity_cache::entry_t *entry : g_entity_cache.doors( ) )
    {
        const model_t *model{ entry->m_entity->GetModel( ) };
        if ( !model )
        {
            continue;
        }

        const i32 number{ brush_model( string_view{ g_tf2.m_model_info->GetModelName( model ) } ) };
        for ( usize index{ }; index < this->m_door_count; ++index )
        {
            door_t &door{ this->m_doors[ index ] };
            if ( door.m_model != number || door.m_handle == entry->m_handle )
            {
                continue;
            }

            door.m_handle        = entry->m_handle;
            door.m_snapshot_tick = -1;
            door.m_state         = door_state_t::unknown;
        }
    }
}

void doors::observe( i32 tick )
{
    for ( usize index{ }; index < this->m_door_count; ++index )
    {
        door_t       &door{ this->m_doors[ index ] };
        C_BaseEntity *entity{ g_entity_cache.resolve( door.m_handle ) };
        if ( !entity || entity->IsDormant( ) )
        {
            continue;
        }

        const f32 simtime{ entity->m_flSimulationTime( ) };
        if ( door.m_snapshot_tick >= 0 && simtime == door.m_simtime )
        {
            door.m_snapshot_tick = tick;
            continue;
        }

        const vec3 received{ entity->m_vecOrigin( ) };
        vec3       previous{ received };
        if ( door.m_snapshot_tick >= 0 )
        {
            previous = door.m_received;
        }

        door.m_received      = received;
        door.m_simtime       = simtime;
        door.m_snapshot_tick = tick;
        door.m_state         = classify( door, received, previous );
    }
}

void doors::observe_players( i32 tick )
{
    this->m_player_count = 0;
    for ( const entity_cache::entry_t *entry : g_entity_cache.players( ) )
    {
        auto *player{ entry->m_entity->As< C_TFPlayer * >( ) };
        if ( player == g_cl.m_local ||
             entry->m_index <= 0 ||
             entry->m_index >= static_cast< i32 >( MAX_DOOR_PLAYERS ) ||
             player->IsDormant( ) ||
             !player->IsAlive( ) )
        {
            continue;
        }

        door_player_t &record{ this->m_players[ entry->m_index ] };
        const vec3     origin{ player->m_vecOrigin( ) };
        const f32      simtime{ player->m_flSimulationTime( ) };
        if ( record.m_seen != this->m_previous_tick || record.m_tick[ 1 ] < 0 )
        {
            record.m_origin[ 0 ] = origin;
            record.m_origin[ 1 ] = origin;
            record.m_tick[ 0 ]   = tick;
            record.m_tick[ 1 ]   = tick;
        } else if ( simtime != record.m_simtime )
        {
            record.m_origin[ 0 ] = record.m_origin[ 1 ];
            record.m_tick[ 0 ]   = record.m_tick[ 1 ];
            record.m_origin[ 1 ] = origin;
            record.m_tick[ 1 ]   = tick;
        }

        record.m_seen    = tick;
        record.m_simtime = simtime;
        record.m_mins    = player->m_vecMins( );
        record.m_maxs    = player->m_vecMaxs( );
        record.m_team    = player->m_iTeamNum( );

        this->m_player_list[ this->m_player_count++ ] = static_cast< u8 >( entry->m_index );
    }
}

vec3 doors::pose_at( const door_t &door, i32 frame ) const
{
    vec3         position{ door.m_received };
    door_state_t state{ door.m_state };
    const bool   predicted{ door.m_predicted && !this->m_team_win };
    const bool   resting{ state == door_state_t::closed || state == door_state_t::open };
    if ( state == door_state_t::unknown || ( !predicted && resting ) )
    {
        return position;
    }

    door_members_t before[ MAX_DOOR_LINKS ]{ };
    if ( predicted )
    {
        this->seed_members( door, state, before );
    }

    for ( i32 at{ door.m_snapshot_tick }; at <= frame; ++at )
    {
        if ( at > door.m_snapshot_tick )
        {
            this->step( door, at, position, state );
        }

        if ( predicted )
        {
            this->fire_links( door, at, before, state );
        }
    }
    return position;
}

void doors::seed_members( const door_t &door, door_state_t state, door_members_t *before ) const
{
    const usize local_word{ static_cast< usize >( this->m_local_index ) / 64 };
    const u64   local_bit{ u64{ 1 } << ( this->m_local_index % 64 ) };

    if ( state == door_state_t::closed || state == door_state_t::closing )
    {
        for ( usize link{ }; link < door.m_link_count; ++link )
        {
            before[ link ] = this->members( this->m_triggers[ door.m_links[ link ].m_trigger ], door.m_snapshot_tick - 1 );
            before[ link ].m_bits[ local_word ] &= ~local_bit;
        }
        return;
    }

    bool anyone{ };
    for ( usize link{ }; link < door.m_link_count; ++link )
    {
        const door_trigger_t &trigger{ this->m_triggers[ door.m_links[ link ].m_trigger ] };
        const door_members_t  earlier{ this->members( trigger, door.m_snapshot_tick - 1 ) };
        const door_members_t  current{ this->members( trigger, door.m_snapshot_tick ) };
        const door_members_t  oldest{ this->members( trigger, door.m_snapshot_tick - 2 ) };
        for ( usize word{ }; word < MAX_DOOR_PLAYERS / 64; ++word )
        {
            before[ link ].m_bits[ word ] = earlier.m_bits[ word ] | current.m_bits[ word ];
        }
        before[ link ].m_bits[ local_word ] |= oldest.m_bits[ local_word ] & local_bit;

        if ( any_member( before[ link ] ) )
        {
            anyone = true;
        }
    }

    if ( state != door_state_t::opening || anyone )
    {
        return;
    }

    for ( usize link{ }; link < door.m_link_count; ++link )
    {
        if ( door.m_links[ link ].m_closes )
        {
            before[ link ].m_bits[ 0 ] |= u64{ 1 };
            return;
        }
    }
}

void doors::step( const door_t &door, i32 frame, vec3 &position, door_state_t &state ) const
{
    if ( state == door_state_t::opening )
    {
        const f32 left{ ( door.m_pos2 - position ).dot( door.m_dir ) };
        if ( left <= door.m_step )
        {
            position = door.m_pos2;
            state    = door_state_t::open;
            return;
        }

        position += door.m_dir * door.m_step;
        return;
    }

    if ( state != door_state_t::closing )
    {
        return;
    }

    const f32  left{ ( position - door.m_pos1 ).dot( door.m_dir ) };
    const bool arrives{ left <= door.m_step };
    vec3       next{ position - door.m_dir * door.m_step };
    if ( arrives )
    {
        next = door.m_pos1;
    }

    if ( this->blocks( door, next, frame ) )
    {
        if ( door.m_reverses )
        {
            state = door_state_t::opening;
        }
        return;
    }

    position = next;
    if ( arrives )
    {
        state = door_state_t::closed;
    }
}

void doors::fire_links( const door_t &door, i32 frame, door_members_t *before, door_state_t &state ) const
{
    for ( usize link{ }; link < door.m_link_count; ++link )
    {
        const door_link_t   &info{ door.m_links[ link ] };
        const door_members_t now{ this->members( this->m_triggers[ info.m_trigger ], frame ) };

        door_members_t entrants{ };
        for ( usize word{ }; word < MAX_DOOR_PLAYERS / 64; ++word )
        {
            entrants.m_bits[ word ] = now.m_bits[ word ] & ~before[ link ].m_bits[ word ];
        }

        const bool was_occupied{ any_member( before[ link ] ) };
        const bool is_occupied{ any_member( now ) };
        before[ link ] = now;

        bool entered{ any_member( entrants ) };
        if ( info.m_all )
        {
            entered = !was_occupied && is_occupied;
        }

        if ( info.m_opens && entered && ( state == door_state_t::closed || state == door_state_t::closing ) )
        {
            state = door_state_t::opening;
            continue;
        }

        if ( info.m_closes && was_occupied && !is_occupied && ( state == door_state_t::open || state == door_state_t::opening ) )
        {
            state = door_state_t::closing;
        }
    }
}

door_members_t doors::members( const door_trigger_t &trigger, i32 frame ) const
{
    door_members_t inside{ };

    door_local_t local{ };
    if ( ( !trigger.m_team || trigger.m_team == this->m_local_team ) && g_cl.m_local->IsAlive( ) && this->local_at( frame, local ) &&
         boxes_overlap( local.m_origin + local.m_mins, local.m_origin + local.m_maxs, trigger.m_mins, trigger.m_maxs ) )
    {
        add_member( inside, this->m_local_index );
    }

    for ( usize index{ }; index < this->m_player_count; ++index )
    {
        const u8             number{ this->m_player_list[ index ] };
        const door_player_t &player{ this->m_players[ number ] };
        if ( trigger.m_team && trigger.m_team != player.m_team )
        {
            continue;
        }

        const vec3 origin{ this->player_at( player, frame ) };
        if ( boxes_overlap( origin + player.m_mins, origin + player.m_maxs, trigger.m_mins, trigger.m_maxs ) )
        {
            add_member( inside, number );
        }
    }
    return inside;
}

bool doors::blocks( const door_t &door, const vec3 &position, i32 frame ) const
{
    const vec3 mins{ position + door.m_mins };
    const vec3 maxs{ position + door.m_maxs };

    door_local_t local{ };
    if ( g_cl.m_local->IsAlive( ) && this->local_at( frame, local ) &&
         boxes_overlap( local.m_origin + local.m_mins, local.m_origin + local.m_maxs, mins, maxs ) )
    {
        return true;
    }

    for ( usize index{ }; index < this->m_player_count; ++index )
    {
        const door_player_t &player{ this->m_players[ this->m_player_list[ index ] ] };
        const vec3           origin{ this->player_at( player, frame ) };
        if ( boxes_overlap( origin + player.m_mins, origin + player.m_maxs, mins, maxs ) )
        {
            return true;
        }
    }
    return false;
}

bool doors::local_at( i32 frame, door_local_t &out ) const
{
    for ( i32 at{ frame }; at > frame - MAX_SHIFT; --at )
    {
        const door_local_t &slot{ this->m_local[ static_cast< usize >( at ) % DOOR_FRAME_RING ] };
        if ( slot.m_frame == at )
        {
            out = slot;
            return true;
        }

        if ( slot.m_frame > at )
        {
            return false;
        }
    }
    return false;
}

vec3 doors::player_at( const door_player_t &player, i32 frame ) const
{
    const i32 span{ player.m_tick[ 1 ] - player.m_tick[ 0 ] };
    if ( span <= 0 )
    {
        return player.m_origin[ 1 ];
    }

    const vec3 velocity{ ( player.m_origin[ 1 ] - player.m_origin[ 0 ] ) / static_cast< f32 >( span ) };
    if ( velocity.length( ) > g_tf2.sv_maxvelocity->GetFloat( ) * TICK_INTERVAL )
    {
        return player.m_origin[ 1 ];
    }

    return player.m_origin[ 1 ] + velocity * static_cast< f32 >( frame - player.m_tick[ 1 ] );
}
