#include "nospread.hpp"

#include "gui/gui.hpp"
#include "gui/indicator.hpp"
#include "render/render.hpp"
#include "util/format.hpp"

#include "crits.hpp"
#include "holdaim_mgr.hpp"
#include "networking.hpp"
#include "proj_sim.hpp"
#include "ragebot.hpp"

ATLAS_NO_DESTROY no_spread g_nospread{ };

// @ distance between neighbouring float32 values, in whatever unit the argument carries; Used for
// two different quantities here: on the millisecond clock it is one step of the seed, and on the
// raw m_flTime seconds it is the grid the server's own printout is snapped to
static float float32_ulp( float value )
{
    if ( !( value > 0.0f ) )
    {
        return 0.0f;
    }

    union
    {
        float f;
        u32   u;
    } bits{ value };

    bits.u = ( bits.u & 0x7F800000u ) - ( 23u << 23 );
    return bits.f;
}

void no_spread::clear( )
{
    this->m_window_count     = 0;
    this->m_window_next      = 0;
    this->m_offset           = 0.0;
    this->m_sigma            = 0.0;
    this->m_grid             = 0.0;
    this->m_request_time     = 0.0;
    this->m_newest_sample    = 0.0;
    this->m_response_pending = false;
    this->m_moved_count      = 0;
    this->m_rejected         = 0;
    this->m_sample_time      = 0.0;
    this->m_step             = 0.0f;
    this->m_request_command  = 0;
    this->m_ack_count        = 0;
    this->m_ack_next         = 0;
    this->m_ack_command      = -1;
    this->m_last_ack         = -1;
    this->m_last_tick        = -1;
    this->m_tick_base        = -1;
    this->m_pending          = false;
    this->m_phase            = seed_phase_t::empty;
    this->m_seed             = 0;
}

void no_spread::pre_predict( )
{
    if ( !g_cfg.m_exploits.m_seed_prediction && this->m_phase != seed_phase_t::empty )
    {
        this->clear( );
    }
    this->m_pre_last_fire = g_cl.m_weapon ? g_cl.m_weapon->m_flLastFireTime( ) : 0.0f;
}

void no_spread::request( )
{
    if ( !g_tf2.m_cl->m_NetChannel )
    {
        return;
    }

    if ( !g_cfg.m_exploits.m_seed_prediction || !this->uses_clock_seed( ) )
    {
        return;
    }

    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };

    // a round cadence divides evenly into every ULP m_flTime can take (0.5s is exactly 4*125ms),
    // which parks every sample on the same side of the quantisation step and defeats the averaging;
    //
    // walking the period with the command number keeps the samples sweeping across it instead!
    // Once locked, and while the last sample agreed, the loop only has to ride out clock
    // drift (~0.2ms/s), so the cadence relaxes and halves the reliable traffic plus the
    // playerperf trail on the server!
    // the sweep has to cover more than one grid cell (62.5-125ms) or the sampling can phase-lock
    // to the server's float grid; the old 70ms sweep did exactly that on a 62.5ms server, variance
    // collapsed below the quantisation floor (sigma 0.00, offset pinned) and the estimate carried
    // an invisible phase bias of up to half a cell
    const double base{ this->synced( ) && this->m_rejected == 0 ? 0.79 : 0.37 };
    const double period{ this->m_pending
                             ? 2.0
                             : base + static_cast< double >( g_networking.sequence_number( ) % 16 ) * 0.011 };

    if ( now - this->m_request_time < period )
    {
        return;
    }

    if ( !g_player_perf.request( perf_owner_t::nospread, g_tf2.m_engine->GetLocalPlayer( ),
                                 SAMPLE_RECORDS ) )
    {
        return;
    }

    this->m_request_time    = now;
    this->m_pending         = true;
    this->m_request_command = g_networking.last_sent_command( );
}

void no_spread::on_perf( const perf_dump_t &dump )
{
    if ( !this->m_pending || dump.m_packets[ 0 ].m_time < this->m_newest_sample )
    {
        return;
    }

    // @::TextMsg runs inside ProcessPacket, before its hook records this packet's acknowledgements;
    this->m_response_count = math::smaller( dump.m_packet_count, SAMPLE_RECORDS );
    for ( i32 index{ }; index < this->m_response_count; ++index )
    {
        this->m_responses[ index ] = dump.m_packets[ index ];
    }
    this->m_response_pending = true;
}

void no_spread::sample( const perf_record_t *records, i32 count )
{
    this->m_newest_sample = records[ 0 ].m_time;
    this->m_step          = float32_ulp( static_cast< float >( records[ 0 ].m_time * 1000.0 ) );
    this->m_pending       = false;
    this->m_grid          = static_cast< double >( float32_ulp( static_cast< float >( records[ 0 ].m_time ) ) );

    double    raws[ SAMPLE_RECORDS ]{ };
    const i32 paired{ this->pair( records, count, this->m_request_command, raws ) };

    if ( !paired )
    {
        return;
    }

    // a sample buckets away from the estimate is a hitch or a mispair, not the clock: drift moves
    // ~0.2ms a second; Let one in and it sits in the variance for a whole window, holding
    // tracking down for the ~25s it takes to age out; The bound is half the grid the sample is
    // snapped to plus half a tick: the tick dating a sample is the first snapshot that acks its
    // move, and a skipped, choked or lost snapshot dates it whole ticks late whenever no newer move
    // takes the ack (every fakelagged one), so past half a tick it sits nearer the neighbouring
    // tick than this one; on a 4ms grid that tick spans almost four seeds; Until there is an
    // estimate, the records of one dump agree with each other unless a tick misdated one, so their
    // median stands in for it; One rejected dump keeps the phase and the quick cadence confirms it;
    // a run of them means the offset genuinely moved, so the window reseeds and reconverges
    // instead of rejecting forever...
    double sorted[ SAMPLE_RECORDS ]{ };
    for ( i32 index{ }; index < paired; ++index )
    {
        i32 slot{ index };
        while ( slot > 0 && sorted[ slot - 1 ] > raws[ index ] )
        {
            sorted[ slot ] = sorted[ slot - 1 ];
            --slot;
        }
        sorted[ slot ] = raws[ index ];
    }

    const bool   estimated{ this->m_phase == seed_phase_t::tracking || this->m_phase == seed_phase_t::uncertain };
    const double bound{ ( this->m_grid + static_cast< double >( TICK_INTERVAL ) ) * 0.5 };
    const double median{ sorted[ paired / 2 ] };
    const double reference{ estimated ? this->m_offset : median };

    bool moved{ estimated && median - sorted[ 0 ] <= bound && sorted[ paired - 1 ] - median <= bound &&
                ( sorted[ 0 ] > this->m_offset || sorted[ paired - 1 ] < this->m_offset ) };

    for ( i32 slip{ 1 }; moved && slip <= this->m_ack_command - this->m_request_command; ++slip )
    {
        double    repaired[ SAMPLE_RECORDS ]{ };
        const i32 repaired_count{ this->pair( records, count, this->m_request_command + slip, repaired ) };

        double low{ repaired[ 0 ] };
        double high{ repaired[ 0 ] };
        for ( i32 index{ 1 }; index < repaired_count; ++index )
        {
            low  = math::smaller( low, repaired[ index ] );
            high = math::bigger( high, repaired[ index ] );
        }

        if ( repaired_count && low <= this->m_offset && high >= this->m_offset )
        {
            DBGOUT( "no_spread::sample: dump off the estimate by %.2fms re-pairs onto it %d moves later",
                    ( median - this->m_offset ) * 1000.0, slip );
            moved = false;
        }
    }

    const double before{ this->m_moved[ this->m_moved_count / 2 ] };
    const bool   confirmed{ moved && this->m_moved_count && ( before - this->m_offset ) * ( median - this->m_offset ) > 0.0 &&
                            before - median <= bound && median - before <= bound };

    if ( confirmed )
    {
        this->m_window_count = 0;
        this->m_window_next  = 0;

        for ( i32 index{ }; index < this->m_moved_count; ++index )
        {
            this->push_offset( this->m_moved[ index ] );
        }

        for ( i32 index{ }; index < paired; ++index )
        {
            this->push_offset( sorted[ index ] );
        }

        DBGOUT( "no_spread::sample: offset stepped %.2fms, reseeded from %d+%d records", ( median - this->m_offset ) * 1000.0,
                this->m_moved_count, paired );

        this->m_moved_count = 0;
        this->m_rejected    = 0;
        this->m_sample_time = g_tf2.m_engine->Plat_FloatTime( );
        this->estimate( );
        return;
    }

    this->m_moved_count = moved ? paired : 0;
    for ( i32 index{ }; index < this->m_moved_count; ++index )
    {
        this->m_moved[ index ] = sorted[ index ];
    }

    i32 admitted{ };
    for ( i32 index{ }; index < paired; ++index )
    {
        if ( raws[ index ] - reference > bound || raws[ index ] - reference < -bound )
        {
            continue;
        }

        this->push_offset( raws[ index ] );
        ++admitted;
    }

    DBGOUT( "no_spread::sample: paired=%d admitted=%d residual=%.2fms bound=%.2fms rejected=%d sigma=%.2fms", paired, admitted,
            ( raws[ 0 ] - reference ) * 1000.0, bound * 1000.0, this->m_rejected, this->m_sigma );

    if ( !admitted )
    {
        if ( estimated && ++this->m_rejected >= 6 )
        {
            this->m_window_count = 0;
            this->m_window_next  = 0;
            this->m_rejected     = 0;
            this->m_phase        = seed_phase_t::acquiring;
        }

        return;
    }

    this->m_rejected    = 0;
    this->m_sample_time = g_tf2.m_engine->Plat_FloatTime( );
    this->estimate( );
}

// reliable stringcmds precede the movement stream; pair the previous actual send's
// transport index, not command_number (which crit steering can independently remap).
i32 no_spread::pair( const perf_record_t *records, i32 count, i32 newest, double *raws ) const
{
    i32 paired{ };
    i32 command{ newest };

    for ( i32 index{ }; index < count && records[ index ].m_num_cmds >= 1; ++index )
    {
        if ( const acked_packet_t *entry{ this->acked( command ) } )
        {
            raws[ paired++ ] = records[ index ].m_time - this->tick_seconds( entry->m_tick );
        }

        command -= records[ index ].m_num_cmds;
    }

    return paired;
}

void no_spread::push_offset( double offset )
{
    this->m_window[ this->m_window_next ] = offset;
    this->m_window_next                   = ( this->m_window_next + 1 ) % WINDOW_SIZE;

    if ( this->m_window_count < WINDOW_SIZE )
    {
        ++this->m_window_count;
    }
}

void no_spread::estimate( )
{
    this->m_phase = seed_phase_t::uncertain;

    const usize count{ this->m_window_count };

    if ( count < WINDOW_MINIMUM )
    {
        this->m_phase = seed_phase_t::acquiring;
        return;
    }

    double sorted[ WINDOW_SIZE ]{ };

    for ( usize index{ }; index < count; ++index )
    {
        sorted[ index ] = this->m_window[ index ];
    }

    for ( usize index{ 1 }; index < count; ++index )
    {
        const double key{ sorted[ index ] };
        usize        slot{ index };

        while ( slot > 0 && sorted[ slot - 1 ] > key )
        {
            sorted[ slot ] = sorted[ slot - 1 ];
            --slot;
        }

        sorted[ slot ] = key;
    }

    // trim first, then average; a partial window trims the same share
    const usize trim{ count * WINDOW_TRIM / WINDOW_SIZE };
    const usize kept{ count - trim * 2 };

    double total{ };

    for ( usize index{ trim }; index < count - trim; ++index )
    {
        total += sorted[ index ];
    }

    this->m_offset = total / static_cast< double >( kept );

    // how far a prediction misses the moment the server actually stamps the seed; The samples carry
    // two independent errors: the server snapping m_flTime onto its float32 grid, and the frame
    // that read the packet running late; The second has a tail: a late frame is late by tens of ms
    // and a shot read by one is lost to any cell, so letting it widen the kernel only waters down
    // the shots that were fine; The scatter comes from the same trimmed set as the centre, and the
    // grid term is taken off analytically: the middle fraction f of a uniform spans f of it, so its
    // variance is f squared times the full one
    double variance{ };

    for ( usize index{ trim }; index < count - trim; ++index )
    {
        const double deviation{ ( sorted[ index ] - this->m_offset ) * 1000.0 };
        variance += deviation * deviation;
    }

    variance /= static_cast< double >( kept );

    const double fraction{ static_cast< double >( kept ) / static_cast< double >( count ) };

    const double grid{ this->m_grid * 1000.0 };
    const double quantisation{ grid * grid / 12.0 };

    this->m_sigma = math::sqrt( static_cast< float >(
        math::bigger( variance / ( fraction * fraction ) - quantisation, 0.0 ) ) );

    // averaging quantized timestamps does not make a zero-scatter window exact..
    const double mean_error{ grid / math::sqrt( static_cast< double >( 12 * kept ) ) };
    this->m_sigma = math::sqrt( this->m_sigma * this->m_sigma + mean_error * mean_error +
                                0.25 / static_cast< double >( kept ) );
    this->m_phase = this->m_step > 0.0f && this->m_ack_count > 0 &&
                            this->m_sigma * KERNEL_WIDTH < static_cast< double >( this->m_step ) * 2.0
                        ? seed_phase_t::tracking : seed_phase_t::uncertain;
}

double no_spread::tick_seconds( i32 tick ) const
{
    return static_cast< double >( TICK_INTERVAL ) * static_cast< double >( tick - this->m_tick_base );
}

double no_spread::stamp_for( i32 tick ) const
{
    // NET_Tick supplies one quantized frametime, not elapsed wall time across missing snapshots;
    // integrating it against the tick gap introduces drift; playerperf anchors the clock directly.
    return this->m_offset + this->tick_seconds( tick );
}

const acked_packet_t *no_spread::acked( i32 command ) const
{
    for ( usize index{ }; index < this->m_ack_count; ++index )
    {
        if ( this->m_acks[ index ].m_command == command )
        {
            return &this->m_acks[ index ];
        }
    }

    return nullptr;
}

usize no_spread::arrival_votes( i32 delay, i32 *ticks, float *votes ) const
{
    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    const usize recent{ math::smaller( this->m_ack_count, ACK_RECENT ) };
    usize distinct{ };
    usize used{ };
    for ( usize back{ 1 }; back <= recent; ++back )
    {
        const auto &entry{ this->m_acks[ ( this->m_ack_next + ACK_RING - back ) % ACK_RING ] };
        const double age{ now - entry.m_time };
        if ( age < 0.0 || age > 1.0 )
        {
            continue;
        }
        const i32 tick{ entry.m_tick + TIME_TO_TICKS( age ) + delay };
        usize slot{ };
        while ( slot < distinct && ticks[ slot ] != tick ) { ++slot; }
        if ( slot == distinct ) { ticks[ distinct++ ] = tick; }
        votes[ slot ] += 1.0f;
        ++used;
    }
    for ( usize slot{ }; slot < distinct; ++slot ) { votes[ slot ] /= static_cast< float >( used ); }
    return distinct;
}


void no_spread::on_packet( CNetChannel *channel )
{
    if ( !g_cfg.m_exploits.m_seed_prediction )
    {
        if ( this->m_last_ack >= 0 || this->m_pending ) { this->clear( ); }
        return;
    }
    this->update_acks( channel );

    if ( this->m_response_pending )
    {
        this->m_response_pending = false;
        this->sample( this->m_responses, this->m_response_count );
    }
}

void no_spread::update_acks( CNetChannel *channel )
{
    const i32 tick{ g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick };
    const i32 ack{ channel->m_nOutSequenceNrAck };
    if ( tick < this->m_last_tick || ack < this->m_last_ack )
    {
        this->clear( );
    }
    if ( this->m_last_ack < 0 )
    {
        this->m_last_ack = ack;
        this->m_last_tick = tick;
        return;
    }
    if ( ack <= this->m_last_ack ) { return; }

    const sent_packet_t *newest{ g_networking.sent( ack ) };
    this->m_last_ack = ack;
    this->m_last_tick = tick;
    if ( !newest || !newest->m_movement || newest->m_command <= this->m_ack_command || newest->m_time <= 0.0 ) { return; }
    if ( this->m_tick_base < 0 ) { this->m_tick_base = tick; }

    this->remember( newest->m_command, tick, newest->m_time );
    this->m_ack_command = newest->m_command;
}

void no_spread::remember( i32 command, i32 tick, double time )
{
    this->m_acks[ this->m_ack_next ] = { command, tick, time };
    this->m_ack_next                 = ( this->m_ack_next + 1 ) % ACK_RING;

    if ( this->m_ack_count < ACK_RING )
    {
        ++this->m_ack_count;
    }
}



bool no_spread::uses_clock_seed( ) const
{
    return g_tf2.sv_usercmd_custom_random_seed->GetBool( );
}

// some game modes simply overwrite the cvar as far as im aware of.
bool no_spread::fixed_spread( ) const
{
    C_TFGameRules *rules{ TFGameRules( ) };
    bool           fixed{ };

    switch ( rules ? rules->m_nMatchGroupType( ) : k_eTFMatchGroup_Invalid )
    {
        case k_eTFMatchGroup_Ladder_6v6:
        case k_eTFMatchGroup_Event_Placeholder: fixed = true; break;
        case k_eTFMatchGroup_MvM_Practice:
        case k_eTFMatchGroup_MvM_MannUp:
        case k_eTFMatchGroup_Casual_12v12: fixed = false; break;
        default: fixed = g_tf2.tf_use_fixed_weaponspreads->GetBool( ); break;
    }

    return fixed || g_cl.attrib_hook( g_cl.m_weapon, 0.0f, ATLAS_XORSTR( "fixed_shot_pattern" ).get( ) ) != 0.0f;
}

static spread_table_t s_spread{ };
const spread_table_t &no_spread::spread_table( ) const
{
    if ( !s_spread.m_filled )
    {
        for ( i32 seed{ }; seed < SPREAD_SEEDS + MAX_BULLETS; ++seed )
        {
            g_tf2.m_random.SetSeed( seed );

            vec2 &offset{ s_spread.m_offset[ seed ] };
            offset.x = g_tf2.m_random.RandomFloat( -0.5f, 0.5f ) + g_tf2.m_random.RandomFloat( -0.5f, 0.5f );
            offset.y = g_tf2.m_random.RandomFloat( -0.5f, 0.5f ) + g_tf2.m_random.RandomFloat( -0.5f, 0.5f );
        }

        s_spread.m_filled = true;
    }

    return s_spread;
}

bool no_spread::should_run( ) const
{
    if ( !g_cl.m_local || !g_cl.m_weapon || !g_cl.m_cmd )
    {
        return false;
    }

    if ( !( g_cl.m_cmd->buttons & IN_ATTACK ) || g_holdaim_mgr.held( ) )
    {
        return false;
    }

    return this->synced( ) || !this->uses_clock_seed( );
}

usize no_spread::candidates( i32 delay, seed_candidate_t *out ) const
{
    if ( !this->uses_clock_seed( ) )
    {
        out[ 0 ] = { g_cl.m_cmd->random_seed & 255, 1.0f };
        return 1;
    }

    if ( !this->synced( ) ) { return 0; }

    i32   ticks[ ACK_RECENT ]{ };
    float votes[ ACK_RECENT ]{ };
    const usize distinct{ this->arrival_votes( delay, ticks, votes ) };

    if ( distinct == 0 )
    {
        return 0;
    }

    const double age{ g_tf2.m_engine->Plat_FloatTime( ) - this->m_sample_time };
    const double half{ this->m_sigma * KERNEL_WIDTH * 0.5 + math::bigger( age, 0.0 ) * 0.2 };

    constexpr i32   reach{ static_cast< i32 >( MAX_CANDIDATES / 2 ) };
    constexpr usize capacity{ MAX_CANDIDATES * 2 };

    usize count{ };

    for ( usize slot{ }; slot < distinct; ++slot )
    {
        // the server rounds its millisecond clock to the nearest float32, so each seed owns the cell
        // between the midpoints to the neighbouring floats; stepping the bit pattern reaches those
        // neighbours exactly, exponent boundaries included
        const double centre{ this->stamp_for( ticks[ slot ] ) * 1000.0 };

        union
        {
            float f;
            i32   i;
        } cell{ static_cast< float >( centre ) };

        for ( i32 index{ -reach }; index <= reach; ++index )
        {
            union
            {
                float f;
                i32   i;
            } below{ }, at{ }, above{ };

            below.i = cell.i + index - 1;
            at.i    = cell.i + index;
            above.i = cell.i + index + 1;

            const double low{ ( static_cast< double >( below.f ) + static_cast< double >( at.f ) ) * 0.5 };
            const double high{ ( static_cast< double >( at.f ) + static_cast< double >( above.f ) ) * 0.5 };

            float share{ };

            if ( half > 0.0 )
            {
                const double overlap{ math::smaller( high, centre + half ) -
                                      math::bigger( low, centre - half ) };

                if ( overlap <= 0.0 )
                {
                    continue;
                }

                share = static_cast< float >( overlap / ( half * 2.0 ) );
            } else if ( centre >= low && centre < high )
            {
                share = 1.0f;
            } else
            {
                continue;
            }

            // a tick apart the spacings mostly land in the same cell, so the cell collects both votes
            const i32 seed{ at.i & 255 };
            usize     existing{ };

            while ( existing < count && out[ existing ].m_seed != seed )
            {
                ++existing;
            }

            if ( existing < count )
            {
                out[ existing ].m_weight += share * votes[ slot ];
            } else if ( count < capacity )
            {
                out[ count++ ] = { seed, share * votes[ slot ] };
            } else
            {
                return 0;
            }
        }
    }

    float mass{ };
    for ( usize index{ }; index < count; ++index ) { mass += out[ index ].m_weight; }
    return mass >= 0.999f ? count : 0;
}

float no_spread::weapon_spread( float base ) const
{
    C_TFWeaponBase *weapon{ g_cl.m_weapon };
    float spread{ g_cl.attrib_hook( weapon, base, ATLAS_XORSTR( "mult_spread_scale" ).get( ) ) };
    const bool minigun{ weapon->GetWeaponID( ) == TF_WEAPON_MINIGUN };

    if ( g_cl.m_local->InCond( TF_COND_RUNE_PRECISION ) )
    {
        spread *= minigun ? 0.4f : 0.1f;
    }

    if ( minigun )
    {
        C_TFMinigun *gun{ weapon->As< C_TFMinigun * >( ) };
        const float now{ g_tf2.m_globals->curtime };
        const float firing{ gun->m_flStartedFiringAt( ) >= 0.0f ? now - gun->m_flStartedFiringAt( ) : 0.0f };
        const float windup{ gun->m_flStartedWindUpAt( ) >= 0.0f ? now - gun->m_flStartedWindUpAt( ) : 0.0f };
        const float spin{ math::clamp( math::bigger( windup - 0.75f, firing ), 0.0f, 1.0f ) };
        spread *= 1.5f - spin * 0.5f;
    }

    if ( weapon->GetWeaponID( ) == TF_WEAPON_REVOLVER && weapon->As< C_TFRevolver * >( )->CanHeadshot( ) )
    {
        const float since{ g_tf2.m_globals->curtime - weapon->As< C_TFRevolver * >( )->m_flLastAccuracyCheck( ) };
        spread *= math::clamp( ( 1.0f - since ) * 2.0f, 0.0f, 1.0f );
    }

    return spread;
}

void no_spread::create_move( )
{
    if ( !g_cfg.m_exploits.m_seed_prediction )
    {
        if ( this->m_phase != seed_phase_t::empty || this->m_pending )
        {
            this->clear( );
        }

        return;
    }

    g_crits.run( );
    g_proj_sim.compensate( );

    if ( !this->should_run( ) )
    {
        return;
    }

    CTFWeaponInfo *info{ g_cl.m_weapon->GetWeaponInfo( ) };

    if ( !info )
    {
        return;
    }

    const WeaponData_t &data{ info->GetWeaponData( g_cl.m_weapon->m_iWeaponMode( ) ) };

    if ( data.m_iProjectile != TF_PROJECTILE_BULLET || data.m_flSpread <= 0.0f || data.m_nBulletsPerShot < 1 )
    {
        return;
    }

    // A discarded move can move a held-trigger shot to the next command the server runs,
    // so every command carrying IN_ATTACK needs its own correction.

    // FX_FireBullets folds item attributes in before it draws: pellet count and spread both move,
    // and a weapon carrying fixed_shot_pattern uses the deterministic table no matter what the
    // convar says; raw weapon data misses all three. Health/consecutive-shot attributes
    // remain outside weapon_spread; the minigun ramp uses the current predicted weapon state.
    const i32 bullets{ math::smaller(
        static_cast< i32 >( g_cl.attrib_hook( g_cl.m_weapon, static_cast< float >( data.m_nBulletsPerShot ),
                                              ATLAS_XORSTR( "mult_bullets_per_shot" ).get( ) ) ),
        MAX_BULLETS ) };

    const float spread{ this->weapon_spread( data.m_flSpread ) };

    if ( bullets < 1 || spread <= 0.0f )
    {
        return;
    }

    // a fixed pattern is not seeded at all
    if ( data.m_nBulletsPerShot > 1 && ( g_cl.m_weapon->GetDamageType( ) & DMG_BUCKSHOT ) != 0 &&
         this->fixed_spread( ) )
    {
        return;
    }

    const i32 remaining{ g_networking.send_delay( ).ticks( ) };

    seed_candidate_t candidates[ MAX_CANDIDATES * 2 ]{ };
    const usize      count{ this->candidates( remaining, candidates ) };

    if ( count == 0 )
    {
        return;
    }

    usize top{ };

    for ( usize index{ 1 }; index < count; ++index )
    {
        if ( candidates[ index ].m_weight > candidates[ top ].m_weight )
        {
            top = index;
        }
    }

    this->m_seed = candidates[ top ].m_seed;

    const float since_fire{ g_tf2.m_globals->curtime - this->m_pre_last_fire };
    const bool  bonus{ since_fire > ( bullets > 1 ? 0.25f : 1.25f ) };

    float first_variance{ 0.5f };

    if ( bonus )
    {
        first_variance = g_cl.attrib_hook( g_cl.m_weapon, 0.0f,
                                           ATLAS_XORSTR( "mult_spread_scale_first_shot" ).get( ) );
    }

    if ( first_variance == 0.0f )
    {
        return;
    }

    // the ragebot placed the whole pattern from these candidates already; shifting it again would
    // correct the same seed twice..
    if ( g_ragebot.evaluated_this_command( ) )
    {
        return;
    }

    this->apply( spread, bullets, first_variance, candidates, count );
}

f32 no_spread::axis_roll( f32 xx, f32 yy, f32 xy, f32 target )
{
    // a pattern on one point has no axis
    if ( xx + yy <= epsilon )
    {
        return 0.0f;
    }

    const f32 roll{ RAD2DEG( 0.5f * math::atan2( 2.0f * xy, xx - yy ) - target ) };
    return roll <= -90.0f ? roll + 180.0f : roll;
}

void no_spread::free_rolls( const vec3 &sent, f32 &low, f32 &high )
{
    low  = -90.0f;
    high = 90.0f;

    vec3 view{ };
    g_tf2.m_engine->GetViewAngles( view );

    const f32 view_yaw{ DEG2RAD( view.y ) };
    const f32 wish_x{ g_cl.m_cmd->forwardmove * math::cos( view_yaw ) + g_cl.m_cmd->sidemove * math::sin( view_yaw ) };
    const f32 wish_y{ g_cl.m_cmd->forwardmove * math::sin( view_yaw ) - g_cl.m_cmd->sidemove * math::cos( view_yaw ) };

    const f32 yaw{ DEG2RAD( sent.y ) };
    const f32 along{ wish_x * math::cos( yaw ) + wish_y * math::sin( yaw ) };
    const f32 across{ wish_x * math::sin( yaw ) - wish_y * math::cos( yaw ) };
    const f32 lean{ across * math::sin( DEG2RAD( sent.x ) ) };

    if ( lean == 0.0f )
    {
        return;
    }

    const f32 edge{ RAD2DEG( math::atan( -along / lean ) ) };
    low  = math::smaller( edge, 0.0f );
    high = math::bigger( edge, 0.0f );
}

ATLAS_FORCE_INLINE f32 nearest_roll( f32 wanted, f32 low, f32 high )
{
    if ( wanted >= low && wanted <= high )
    {
        return wanted;
    }

    const f32 down{ wanted > high ? wanted - high : wanted + 180.0f - high };
    const f32 up{ wanted < low ? low - wanted : low + 180.0f - wanted };
    return down <= up ? high : low;
}

ATLAS_FORCE_INLINE bool launch_rolled( const vec3 &forward, f32 x, f32 y, f32 roll, const vec3 &punch, vec3 &sent )
{
    const f32 c{ math::cos( DEG2RAD( roll ) ) };
    const f32 s{ math::sin( DEG2RAD( roll ) ) };
    if ( !math::launch_angles( forward, 1.0f, x * c + y * s, y * c - x * s, sent ) )
    {
        return false;
    }

    sent -= punch;
    sent.z = roll - punch.z;
    return true;
}

f32 no_spread::nearest_free_roll( f32 wanted, const vec3 &sent )
{
    f32 low{ };
    f32 high{ };
    free_rolls( sent, low, high );
    return nearest_roll( wanted, low, high );
}

bool no_spread::rolls_freely( const vec3 &sent )
{
    f32 low{ };
    f32 high{ };
    free_rolls( sent, low, high );
    return sent.z >= low && sent.z <= high;
}

// no target here, so the pattern's long axis goes up: a standing player's hull (VEC_HULL_MIN/MAX, 48 x 48 x 82) is taller than
// wide on the view plane from all but a near vertical view
f32 no_spread::pattern_roll( float ( *offsets_x )[ MAX_BULLETS ], float ( *offsets_y )[ MAX_BULLETS ],
                             const seed_candidate_t *candidates, usize count, i32 bullets ) const
{
    f32 total{ };
    f32 mean_x{ };
    f32 mean_y{ };

    for ( usize candidate{ }; candidate < count; ++candidate )
    {
        for ( i32 index{ }; index < bullets; ++index )
        {
            total += candidates[ candidate ].m_weight;
            mean_x += candidates[ candidate ].m_weight * offsets_x[ candidate ][ index ];
            mean_y += candidates[ candidate ].m_weight * offsets_y[ candidate ][ index ];
        }
    }

    if ( total <= 0.0f )
    {
        return 0.0f;
    }

    mean_x /= total;
    mean_y /= total;

    f32 xx{ };
    f32 yy{ };
    f32 xy{ };

    for ( usize candidate{ }; candidate < count; ++candidate )
    {
        for ( i32 index{ }; index < bullets; ++index )
        {
            const f32 dx{ offsets_x[ candidate ][ index ] - mean_x };
            const f32 dy{ offsets_y[ candidate ][ index ] - mean_y };
            xx += candidates[ candidate ].m_weight * dx * dx;
            yy += candidates[ candidate ].m_weight * dy * dy;
            xy += candidates[ candidate ].m_weight * dx * dy;
        }
    }

    return axis_roll( xx, yy, xy, PI * 0.5f );
}

void no_spread::apply( float spread, i32 bullets, float first_variance, const seed_candidate_t *candidates,
                       usize count )
{
    vec3 forward{ };
    const vec3 punch{ g_cl.m_local->m_vecPunchAngle( ) };
    const vec3 firing_angles{ g_cl.m_cmd->viewangles + punch };
    math::angle_vectors( firing_angles, &forward );
    const spread_table_t &table{ this->spread_table( ) };

    float offsets_x[ MAX_CANDIDATES * 2 ][ MAX_BULLETS ]{ };
    float offsets_y[ MAX_CANDIDATES * 2 ][ MAX_BULLETS ]{ };
    usize top{ };

    for ( usize candidate{ }; candidate < count; ++candidate )
    {
        for ( i32 index{ }; index < bullets; ++index )
        {
            const float variance{ index == 0 ? first_variance : 0.5f };
            if ( variance == 0.5f )
            {
                const vec2 &offset{ table.m_offset[ candidates[ candidate ].m_seed + index ] };
                offsets_x[ candidate ][ index ] = offset.x;
                offsets_y[ candidate ][ index ] = offset.y;
                continue;
            }

            g_tf2.m_random.SetSeed( candidates[ candidate ].m_seed + index );

            offsets_x[ candidate ][ index ] = g_tf2.m_random.RandomFloat( -variance, variance ) +
                                              g_tf2.m_random.RandomFloat( -variance, variance );
            offsets_y[ candidate ][ index ] = g_tf2.m_random.RandomFloat( -variance, variance ) +
                                              g_tf2.m_random.RandomFloat( -variance, variance );
        }

        if ( candidates[ candidate ].m_weight > candidates[ top ].m_weight )
        {
            top = candidate;
        }
    }

    const f32 wanted{ g_cfg.m_ragebot.m_hitscan_profiles[ client::get_profile( ) ].m_hitscan_roll_angles
                          ? this->pattern_roll( offsets_x, offsets_y, candidates, count, bullets )
                          : 0.0f };

    float anchor_x{ };
    float anchor_y{ };
    for ( i32 index{ }; index < bullets; ++index )
    {
        anchor_x += offsets_x[ top ][ index ];
        anchor_y += offsets_y[ top ][ index ];
    }

    anchor_x /= static_cast< float >( bullets );
    anchor_y /= static_cast< float >( bullets );

    i32 common_first{ 1 };
    i32 common_last{ 0 };
    if ( count > 1 && bullets > 1 && first_variance == 0.5f )
    {
        common_first = candidates[ 0 ].m_seed;
        common_last  = candidates[ 0 ].m_seed + bullets - 1;
        for ( usize candidate{ 1 }; candidate < count; ++candidate )
        {
            common_first = math::bigger( common_first, candidates[ candidate ].m_seed );
            common_last  = math::smaller( common_last, candidates[ candidate ].m_seed + bullets - 1 );
        }
    }

    for ( i32 round{ }; round < 2; ++round )
    {
        float mean_x{ };
        float mean_y{ };
        float total{ };

        for ( usize candidate{ }; candidate < count; ++candidate )
        {
            i32   chosen{ };
            float closest{ FLT_MAX };

            for ( i32 index{ }; index < bullets; ++index )
            {
                const i32 pellet_seed{ candidates[ candidate ].m_seed + index };
                if ( common_first <= common_last && ( pellet_seed < common_first || pellet_seed > common_last ) ) { continue; }

                const float dx{ offsets_x[ candidate ][ index ] - anchor_x };
                const float dy{ offsets_y[ candidate ][ index ] - anchor_y };
                const float distance{ dx * dx + dy * dy };

                if ( distance < closest )
                {
                    closest = distance;
                    chosen  = index;
                }
            }

            const float weight{ candidates[ candidate ].m_weight };

            mean_x += offsets_x[ candidate ][ chosen ] * weight;
            mean_y += offsets_y[ candidate ][ chosen ] * weight;
            total += weight;
        }

        if ( total <= 0.0f )
        {
            return;
        }

        anchor_x = mean_x / total;
        anchor_y = mean_y / total;
    }

    if ( firing_angles.z != 0.0f )
    {
        return;
    }

    vec3 corrected{ };
    f32  roll{ wanted };
    for ( i32 pass{ }; roll != 0.0f; ++pass )
    {
        if ( pass == 3 || !launch_rolled( forward, anchor_x * spread, anchor_y * spread, roll, punch, corrected ) )
        {
            roll = 0.0f;
            break;
        }

        if ( rolls_freely( corrected ) )
        {
            break;
        }

        roll = nearest_free_roll( wanted - punch.z, corrected ) + punch.z;
    }

    if ( roll == 0.0f && !launch_rolled( forward, anchor_x * spread, anchor_y * spread, 0.0f, punch, corrected ) )
    {
        return;
    }

    if ( corrected.x < -89.0f || corrected.x > 89.0f )
    {
        return;
    }

    const f32 sent_roll{ corrected.z };
    math::clamp_angles( corrected );
    corrected.z            = sent_roll;
    g_cl.m_cmd->viewangles = corrected;
}

constexpr i32 SEED_WIDTH{ 184 };
constexpr i32 SEED_ROW_HEIGHT{ INDICATOR_WELL_HEIGHT };
constexpr i32 SEED_ROW_GAP{ 6 };
constexpr i32 SEED_ROW_STEP{ SEED_ROW_HEIGHT + SEED_ROW_GAP };
constexpr i32 SEED_HEIGHT{ INDICATOR_HEIGHT + SEED_ROW_STEP };

enum class seed_status_t : usize
{
    acquiring,
    synced,
    uncertain,
    count,
};

struct seed_look_t
{
    const char *m_label{ };
    const char *m_glyph{ };
    color_t     m_text{ };
    color_t     m_ink{ };
};

static indicator_card_t s_seed_card{ };
static u8               s_seed_mix[ static_cast< usize >( seed_status_t::count ) ]{ };

static seed_look_t seed_look( seed_status_t status )
{
    switch ( status )
    {
        case seed_status_t::synced: return seed_look_t{ ATLAS_XORSTR_STATIC( "synced" ), ICON_CROSSHAIR, g_theme.m_accent, g_theme.m_accent };
        case seed_status_t::acquiring: return seed_look_t{ ATLAS_XORSTR_STATIC( "acquiring" ), ICON_LOADER, CONTROL_VALUE, MENU_TEXT_DIM };
        default: return seed_look_t{ ATLAS_XORSTR_STATIC( "uncertain" ), ICON_TRIANGLE_ALERT, STATUS_WARNING, STATUS_WARNING };
    }
}

ATLAS_FORCE_INLINE u8 seed_mix( seed_status_t status )
{
    return gui::eased_mix( s_seed_mix[ static_cast< usize >( status ) ] );
}

void no_spread::paint( ) const
{
    const bool wanted{ g_cfg.m_exploits.m_seed_prediction && g_cfg.m_exploits.m_seed_indicator &&
                       g_tf2.m_engine->IsInGame( ) && this->uses_clock_seed( ) };

    if ( !wanted && !s_seed_card.m_enter )
    {
        hide_indicator( s_seed_card );

        for ( u8 &mix : s_seed_mix )
        {
            mix = 0;
        }

        return;
    }

    seed_status_t status{ seed_status_t::uncertain };

    if ( this->synced( ) )
    {
        status = seed_status_t::synced;
    } else if ( this->m_phase == seed_phase_t::empty || this->m_phase == seed_phase_t::acquiring )
    {
        status = seed_status_t::acquiring;
    }

    float fill{ };

    if ( status == seed_status_t::acquiring )
    {
        fill = math::smaller( static_cast< float >( this->m_window_count ) / static_cast< float >( WINDOW_MINIMUM ), 1.0f );
    } else if ( this->m_step > 0.0f )
    {
        fill = math::clamp( 1.0f - static_cast< float >( this->m_sigma * KERNEL_WIDTH ) / ( this->m_step * 2.0f ),
                            0.0f, 1.0f );
    }

    s_seed_card.m_at = g_cfg.m_misc.m_seed_indicator_position;
    const u32 delta{ step_indicator( s_seed_card, fill, g_cl.m_screen_width, g_cl.m_screen_height, SEED_WIDTH,
                                     SEED_HEIGHT, wanted ) };
    g_cfg.m_misc.m_seed_indicator_position = s_seed_card.m_at;

    for ( usize index{ }; index < static_cast< usize >( seed_status_t::count ); ++index )
    {
        gui::step_mix( s_seed_mix[ index ], index == static_cast< usize >( status ), delta,
                       WIDGET_TOGGLE_DURATION );
    }

    const u8 synced{ seed_mix( seed_status_t::synced ) };
    const u8 uncertain{ seed_mix( seed_status_t::uncertain ) };

    const color_t edge{ gui::mix_color( gui::mix_color( g_theme.m_input_focus_border, g_theme.m_accent, synced ),
                                        STATUS_WARNING, uncertain ) };
    const color_t wash{ gui::mix_color( g_theme.m_selected_background, g_theme.m_row_selected_background,
                                        synced ) };

    const indicator_frame_t frame{ draw_indicator_frame( s_seed_card, ATLAS_XORSTR_STATIC( "seed pred" ),
                                                         CARD_TITLE_TEXT ) };
    const u8                enter{ frame.m_enter };

    const rect_t first{ frame.m_content.left, frame.m_content.top, frame.m_content.right,
                        frame.m_content.top + SEED_ROW_HEIGHT };
    const rect_t second{ first.left, first.top + SEED_ROW_STEP, first.right, first.bottom + SEED_ROW_STEP };

    draw_indicator_well( first, enter );

    const rect_t inner{ first.left + 1, first.top + 1, first.right - 1, first.bottom - 1 };
    const i32    washed{ static_cast< i32 >( s_seed_card.m_fill * static_cast< float >( inner.right - inner.left ) +
                                             0.5f ) };
    const i32    fill_edge{ math::smaller( inner.left + washed, inner.right - 1 ) };

    g_render.box( rect_t{ inner.left, inner.top, inner.left + washed, inner.bottom },
                  gui::with_alpha( wash, enter ), true );
    g_render.box( rect_t{ fill_edge, inner.top, fill_edge + 1, inner.bottom }, gui::with_alpha( edge, enter ),
                  true );

    draw_indicator_stripe( first, enter, edge );
    draw_indicator_well( second, enter );
    draw_indicator_stripe( second, enter, edge );

    for ( usize index{ }; index < static_cast< usize >( seed_status_t::count ); ++index )
    {
        const seed_status_t layer{ static_cast< seed_status_t >( index ) };
        const u8            mix{ seed_mix( layer ) };

        if ( !mix )
        {
            continue;
        }

        const seed_look_t look{ seed_look( layer ) };
        const bool        filling{ layer == seed_status_t::acquiring };
        const u8          pulse{ filling ? indicator_glyph_alpha( 255 ) : u8{ 255 } };

        char value[ 16 ]{ };

        if ( filling )
        {
            str::format_to( value, sizeof( value ), ATLAS_XORSTR_STATIC( "{}/{}" ),
                            static_cast< i32 >( this->m_window_count ), static_cast< i32 >( WINDOW_MINIMUM ) );
        } else
        {
            str::format_to( value, sizeof( value ), ATLAS_XORSTR_STATIC( "{:.1f} ms" ),
                            static_cast< float >( this->m_sigma ) );
        }

        g_render.text( MENU_FONT_TINY( ), frame.m_state, gui::with_alpha( gui::with_alpha( look.m_text, mix ), enter ),
                       look.m_label, false, render::text_align::right );
        gui::draw_icon_at( look.m_glyph, point_t{ first.left + 6, first.top + 2 },
                           gui::with_alpha( gui::with_alpha( look.m_ink, gui::mix_channel( 0, pulse, mix ) ), enter ) );
        g_render.text( MENU_FONT_TINY( ), point_t{ first.left + 26, indicator_text_y( first ) },
                       gui::with_alpha( gui::with_alpha( CONTROL_VALUE, mix ), enter ),
                       filling ? ATLAS_XORSTR_STATIC( "window" ) : ATLAS_XORSTR_STATIC( "sigma" ) );
        g_render.text( MENU_FONT_TINY( ), point_t{ first.right - 5, indicator_text_y( first ) },
                       gui::with_alpha( gui::with_alpha( CONTROL_LABEL, mix ), enter ), value, false,
                       render::text_align::right );
    }

    char step[ 16 ]{ };

    if ( this->m_step > 0.0f )
    {
        str::format_to( step, sizeof( step ),
                        this->m_step >= 1.0f ? ATLAS_XORSTR_STATIC( "{:.0f} ms" ) : ATLAS_XORSTR_STATIC( "{:.3f} ms" ),
                        this->m_step );
    } else
    {
        str::format_to( step, sizeof( step ), ATLAS_XORSTR_STATIC( "-" ) );
    }

    gui::draw_icon_at( ICON_RULER, point_t{ second.left + 6, second.top + 2 }, gui::with_alpha( MENU_TEXT_DIM, enter ) );
    g_render.text( MENU_FONT_TINY( ), point_t{ second.left + 26, indicator_text_y( second ) },
                   gui::with_alpha( CONTROL_VALUE, enter ), ATLAS_XORSTR_STATIC( "step" ) );
    g_render.text( MENU_FONT_TINY( ), point_t{ second.right - 5, indicator_text_y( second ) },
                   gui::with_alpha( CONTROL_LABEL, enter ), step, false, render::text_align::right );
}
