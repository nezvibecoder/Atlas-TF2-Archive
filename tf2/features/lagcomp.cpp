#include "lagcomp.hpp"
#include "extrapolate.hpp"
#include "networking.hpp"
#include "tickbase.hpp"

#include "math/math.hpp"
#include "render/render.hpp"
#include "runtime/memory.hpp"

ATLAS_NO_DESTROY lag_compensation g_lagcomp{ };

constexpr f32     NOISE_FLOOR_UNITS{ 6.0f }; // matches recover_wish_input's NOISE_FLOOR
constexpr color_t NEWEST_COLOR{ 255, 122, 122, 255 };
constexpr color_t RECORD_COLOR{ 196, 199, 208, 120 };

constexpr f32 MOVING_SPEED{ 60.0f };
constexpr f32 GROUND_SLACK{ 0.5f };
constexpr f32 GROUND_NOISE{ 2.0f };
constexpr f32 AIR_SLACK{ 2.0f };
constexpr f32 AIR_NOISE{ 4.0f };
constexpr f32 STALL_SHARE{ 0.5f };
constexpr f32 KEPT_HEADING{ 0.7f };

static vec3 bone_origin( const matrix3x4 &bone )
{
    return vec3{ bone[ 0 ][ 3 ], bone[ 1 ][ 3 ], bone[ 2 ][ 3 ] };
}

static void draw_skeleton( const studiohdr_t *header, const matrix3x4 *bones, color_t value )
{
    for ( i32 index{ }; index < header->numbones; ++index )
    {
        const mstudiobone_t *bone{ header->pBone( index ) };

        if ( bone->parent < 0 || !( bone->flags & BONE_USED_BY_HITBOX ) ||
             !( header->pBone( bone->parent )->flags & BONE_USED_BY_HITBOX ) )
        {
            continue;
        }

        vec2 from{ };
        vec2 to{ };

        if ( !g_cl.world_to_screen( bone_origin( bones[ index ] ), from ) ||
             !g_cl.world_to_screen( bone_origin( bones[ bone->parent ] ), to ) )
        {
            continue;
        }

        g_draw.line( point_t{ static_cast< i32 >( from.x ), static_cast< i32 >( from.y ) },
                     point_t{ static_cast< i32 >( to.x ), static_cast< i32 >( to.y ) }, value );
    }
}

ATLAS_FORCE_INLINE i32 server_rhythm( const i32 *gaps )
{
    i32 sorted[ RHYTHM_GAPS ]{ };
    i32 known{ };

    for ( i32 at{ }; at < RHYTHM_GAPS; ++at )
    {
        if ( gaps[ at ] > 0 )
        {
            sorted[ known++ ] = gaps[ at ];
        }
    }

    if ( known == 0 )
    {
        return 1;
    }

    for ( i32 a{ }; a < known - 1; ++a )
    {
        for ( i32 b{ a + 1 }; b < known; ++b )
        {
            if ( sorted[ b ] < sorted[ a ] )
            {
                const i32 swap{ sorted[ a ] };
                sorted[ a ] = sorted[ b ];
                sorted[ b ] = swap;
            }
        }
    }

    return sorted[ known / 2 ];
}

C_TFPlayer *lag_compensation::trackable( i32 index, bool tracked )
{
    IClientEntity *client_entity{ g_tf2.m_entity_list->GetClientEntity( index ) };
    if ( !client_entity )
    {
        if ( tracked )
        {
            DBGOUT( "lagcomp[%d]: entity gone", index );
        }

        return nullptr;
    }

    C_BaseEntity *base_entity{ client_entity->GetBaseEntity( ) };
    if ( !base_entity || base_entity->IsDormant( ) )
    {
        if ( tracked )
        {
            DBGOUT( "lagcomp[%d]: dormant", index );
        }

        return nullptr;
    }

    ClientClass *client_class{ base_entity->GetClientClass( ) };
    if ( !client_class || client_class->GetClassID( ) != ETFClassID::CTFPlayer )
    {
        return nullptr;
    }

    C_TFPlayer *player{ base_entity->As< C_TFPlayer * >( ) };

    if ( !player->IsAlive( ) )
    {
        if ( tracked )
        {
            DBGOUT( "lagcomp[%d]: had a painful death", index );
        }

        return nullptr;
    }

    if ( player->m_nSequence( ) < 0 || !player->m_PlayerAnimState( ) )
    {
        if ( tracked )
        {
            DBGOUT( "lagcomp[%d]: pose not replayable", index );
        }

        return nullptr;
    }

    return player;
}

float lag_compensation::lerp_time( )
{
    if ( !g_tf2.cl_interpolate->GetInt( ) )
    {
        return 0.0f;
    }

    const i32 updaterate{ math::clamp( g_tf2.cl_updaterate->GetInt( ),
                                       g_tf2.sv_minupdaterate->GetInt( ),
                                       g_tf2.sv_maxupdaterate->GetInt( ) ) };
    float ratio{ g_tf2.cl_interp_ratio->GetFloat( ) };
    const float min_ratio{ g_tf2.sv_client_min_interp_ratio->GetFloat( ) };

    if ( ratio == 0.0f )
    {
        ratio = 1.0f;
    }

    if ( min_ratio != -1.0f )
    {
        ratio = math::clamp( ratio, min_ratio, g_tf2.sv_client_max_interp_ratio->GetFloat( ) );
    }

    return math::bigger( g_tf2.cl_interp->GetFloat( ),
                         ratio / static_cast< float >( math::bigger( updaterate, 1 ) ) );
}

float lag_compensation::max_unlag( ) const
{
    return g_tf2.sv_maxunlag->GetFloat( );
}

float lag_compensation::teleport_distance_sqr( ) const
{
    const float dist{ g_tf2.sv_lagcompensation_teleport_dist->GetFloat( ) };
    return dist * dist;
}

i32 lag_compensation::command_bank( )
{
    const i32 bank{ g_tf2.sv_maxusrcmdprocessticks->GetInt( ) };
    return bank > 0 ? bank : MAX_PATH_TICKS;
}

void lag_compensation::invalidate( i32 index )
{
    if ( index < 0 || index >= MAX_TRACKED_PLAYERS )
    {
        return;
    }

    g_extrapolate.forget( index ); // as if it never existed.

    track_t &track{ this->m_tracks[ index ] };
    track.m_head       = 0;
    track.m_count      = 0;
    track.m_last_tick  = 0;
    track.m_gap_rotor  = 0;
    track.m_break_tick = 0;

    for ( i32 &gap : track.m_gaps )
    {
        gap = 0;
    }

    for ( i32 &gap : track.m_server_gaps )
    {
        gap = 0;
    }

    track.m_server_rotor = 0;
    track.m_charge       = command_bank( );
    track.m_over_clock   = 0;
    track.m_loose        = loose_state_t{ };
}

void lag_compensation::clear( )
{
    for ( i32 index{ }; index < MAX_TRACKED_PLAYERS; ++index )
    {
        this->invalidate( index );
    }
}

// the rhythm the records arrive to
i32 lag_compensation::cadence( i32 index ) const
{
    if ( index < 1 || index >= MAX_TRACKED_PLAYERS )
    {
        return 0;
    }

    i32 sorted[ 4 ]{ };
    i32 known{ };

    for ( const i32 gap : this->m_tracks[ index ].m_gaps )
    {
        if ( gap > 0 )
        {
            sorted[ known++ ] = gap;
        }
    }

    if ( known < 4 )
    {
        return 0;
    }

    for ( i32 a{ }; a < known - 1; ++a )
    {
        for ( i32 b{ a + 1 }; b < known; ++b )
        {
            if ( sorted[ b ] < sorted[ a ] )
            {
                const i32 swap{ sorted[ a ] };
                sorted[ a ] = sorted[ b ];
                sorted[ b ] = swap;
            }
        }
    }

    return sorted[ known / 2 ];
}

i32 lag_compensation::count( i32 index ) const
{
    if ( index < 1 || index >= MAX_TRACKED_PLAYERS )
    {
        return 0;
    }

    return this->m_tracks[ index ].m_count;
}

const lag_record_t *lag_compensation::record( i32 index, i32 age ) const
{
    if ( index < 1 || index >= MAX_TRACKED_PLAYERS || age < 0 )
    {
        return nullptr;
    }

    const track_t &track{ this->m_tracks[ index ] };

    if ( !track.m_records || age >= track.m_count )
    {
        return nullptr;
    }

    return &track.m_records[ ( track.m_head - age + MAX_RECORDS ) % MAX_RECORDS ];
}

warp_read_t lag_compensation::read_link( const track_t &track, const lag_record_t &newest, const vec3 &delta,
                                         i32 sim_gap, i32 server_gap, float maxspeed, bool on_ground ) const
{
    warp_read_t out{ };

    const f32  dt{ TICK_INTERVAL };
    const f32  speed{ newest.m_velocity.length2d( ) };
    const f32  ground{ delta.length2d( ) };
    const i32  clock{ math::bigger( math::bigger( sim_gap, server_gap ), 1 ) };
    const bool grounded{ newest.m_on_ground && on_ground };

    const f32 clock_pace{ newest.m_gap > 0 ? speed * static_cast< f32 >( newest.m_commands ) / static_cast< f32 >( newest.m_gap )
                                           : 0.0f };

    out.m_commands = sim_gap > 0 ? sim_gap : clock;
    out.m_link     = clock > 1 ? warp_link_t::burst : warp_link_t::steady;

    const f32 reach{ math::bigger( math::bigger( speed, clock_pace ), math::bigger( newest.m_maxspeed, maxspeed ) ) };
    const f32 allowed{ grounded ? ( static_cast< f32 >( clock ) + GROUND_SLACK ) * reach * dt + GROUND_NOISE
                                : ( static_cast< f32 >( clock ) + AIR_SLACK ) * reach * dt + AIR_NOISE };

    const f32  along{ speed >= MOVING_SPEED && ground > GROUND_NOISE
                          ? ( delta.x * newest.m_velocity.x + delta.y * newest.m_velocity.y ) / ( ground * speed )
                          : 1.0f };
    const bool kept{ along >= KEPT_HEADING };

    if ( reach > 0.0f && ground > allowed && ( grounded || ( track.m_charge >= 2 && kept ) ) )
    {
        const f32 counted_pace{ speed >= MOVING_SPEED ? math::smaller( speed, reach ) : reach };
        const i32 counted{ static_cast< i32 >( ground / ( counted_pace * dt ) + 0.5f ) };

        out.m_commands = math::clamp( counted, clock + 1, clock + command_bank( ) );
        out.m_link     = warp_link_t::warp;
    } else if ( clock > 1 && speed >= MOVING_SPEED && kept &&
                ground < STALL_SHARE * static_cast< f32 >( clock ) * speed * dt )
    {
        const i32 counted{ static_cast< i32 >( ground / ( speed * dt ) + 0.5f ) };
        out.m_commands = math::clamp( counted, 1, clock - 1 );
        out.m_link     = warp_link_t::recharge;
    }

    out.m_velocity = delta / ( static_cast< f32 >( out.m_commands ) * dt );
    out.m_banked   = server_gap - out.m_commands;

    if ( out.m_link == warp_link_t::burst && speed < MOVING_SPEED )
    {
        out.m_banked = math::bigger( out.m_banked, server_gap - 1 );
    }

    return out;
}

void lag_compensation::loosen( i32 index, C_TFPlayer *player, i32 tick, i32 server_tick )
{
    track_t            &track{ this->m_tracks[ index ] };
    const lag_record_t *newest{ this->record( index, 0 ) };
    const vec3          origin{ player->m_vecOrigin( ) };
    const bool          on_ground{ player->IsOnGround( ) };
    const i32           server_gap{ server_tick > newest->m_server_tick ? server_tick - newest->m_server_tick : 1 };
    const warp_read_t   link{ this->read_link( track, *newest, origin - newest->m_pose.m_origin, tick - newest->m_tick,
                                               server_gap, player->m_flMaxspeed( ), on_ground ) };

    if ( track.m_loose.m_server_tick < 0 )
    {
        DBGOUT( "lagcomp[%d]: clock pulled back %d ticks under a %d unit move, %d commands", index, newest->m_tick - tick,
                static_cast< i32 >( ( origin - newest->m_pose.m_origin ).length2d( ) ), link.m_commands );
    }

    track.m_loose.m_origin      = origin;
    track.m_loose.m_velocity    = link.m_velocity;
    track.m_loose.m_server_tick = server_tick;
    track.m_loose.m_commands    = link.m_commands;
    track.m_loose.m_on_ground   = on_ground;
}

void lag_compensation::store( i32 index, C_TFPlayer *player )
{
    track_t  &track{ this->m_tracks[ index ] };
    const i32 tick{ TIME_TO_TICKS( player->m_flSimulationTime( ) ) };
    const i32 server_tick{ g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick };

    // ::FrameUpdatePostEntityThink
    if ( track.m_count > 0 && tick <= track.m_last_tick )
    {
        if ( ( player->m_vecOrigin( ) - this->record( index, 0 )->m_pose.m_origin ).length2d_sqr( ) >
             this->teleport_distance_sqr( ) )
        {
            track.m_break_tick = track.m_last_tick;
            this->loosen( index, player, tick, server_tick );
        }

        return;
    }

    if ( !track.m_records )
    {
        constexpr usize size{ sizeof( lag_record_t ) * MAX_RECORDS };
        track.m_records = static_cast< lag_record_t * >( g_tf2.m_alloc->Alloc( size ) );

        if ( !track.m_records )
        {
            DBGOUT( "lagcomp[%d]: out of memory for %d records", index, MAX_RECORDS );
            return;
        }

        atl::memset( track.m_records, 0, size );
    }

    const i32     slot{ track.m_count > 0 ? ( track.m_head + 1 ) % MAX_RECORDS : 0 };
    lag_record_t &entry{ track.m_records[ slot ] };

    if ( !g_bone_mgr.capture( player, entry.m_pose ) )
    {
        // a full ring hands out the oldest live record as the next slot, and a half written one
        // must never stay reachable
        DBGOUT( "lagcomp[%d]: capture failed, dropped %d records", index, track.m_count );
        this->invalidate( index );
        return;
    }

    // the server drives a player's abs angles off the animstate feet yaw and leaves pitch and roll
    // at zero, so the pose the bones come out of has to sit in that same frame
    entry.m_pose.m_angles.x = 0.0f;
    entry.m_pose.m_angles.z = 0.0f;

    this->store_bones( player, entry );

    entry.m_mins        = player->m_vecMinsPreScaled( );
    entry.m_maxs        = player->m_vecMaxsPreScaled( );
    entry.m_tick        = tick;
    entry.m_server_tick = server_tick;
    entry.m_maxspeed    = player->m_flMaxspeed( );
    entry.m_eye_yaw     = player->m_angEyeAngles( ).y;
    entry.m_on_ground   = player->IsOnGround( );
    entry.m_ducked      = player->IsDucked( );
    entry.m_swimming    = player->m_nWaterLevel( ) >= 2;
    entry.m_parachuting = player->InCond( ETFCond::TF_COND_PARACHUTE_ACTIVE );

    const lag_record_t *newest{ this->record( index, 0 ) };

    entry.m_gap         = newest ? tick - track.m_last_tick : 0;

    // @ CGameMovement::FinishDuck
    vec3 corrected{ entry.m_pose.m_origin };
    if ( newest && !entry.m_on_ground && !newest->m_on_ground )
    {
        corrected += ( entry.m_mins + entry.m_maxs ) - ( newest->m_mins + newest->m_maxs );
    }

    entry.m_velocity = vec3{ };
    entry.m_commands = 1;
    entry.m_link     = warp_link_t::first;

    if ( newest && entry.m_gap > 0 )
    {
        const i32         server_gap{ server_tick > newest->m_server_tick ? server_tick - newest->m_server_tick : entry.m_gap };
        const warp_read_t link{ this->read_link( track, *newest, corrected - newest->m_pose.m_origin, entry.m_gap, server_gap,
                                                 entry.m_maxspeed, entry.m_on_ground ) };

        entry.m_velocity = link.m_velocity;
        entry.m_commands = link.m_commands;
        entry.m_link     = link.m_link;

        track.m_charge     = math::clamp( track.m_charge + link.m_banked, 0, command_bank( ) );
        track.m_over_clock = link.m_commands > server_gap ? track.m_over_clock + 1 : 0;

        if ( link.m_link != warp_link_t::recharge )
        {
            track.m_server_gaps[ track.m_server_rotor ] = server_gap;
            track.m_server_rotor                        = ( track.m_server_rotor + 1 ) % RHYTHM_GAPS;
        }

        if ( link.m_link == warp_link_t::warp )
        {
            DBGOUT( "lagcomp[%d]: warp, %d commands over %d server ticks (%d of clock), %d banked left", index,
                    link.m_commands, server_gap, entry.m_gap, track.m_charge );
        }
    }

    entry.m_wall = false;

    if ( newest && newest->m_velocity.length2d( ) > 60.0f )
    {
        Ray_t ray{ };
        ray.Init( newest->m_pose.m_origin,
                  newest->m_pose.m_origin + newest->m_velocity * TICK_INTERVAL, entry.m_mins,
                  entry.m_maxs );

        trace_t               trace{ };
        CTraceFilterWorldOnly filter{ };
        g_tf2.m_trace->TraceRay( ray, MASK_PLAYERSOLID_BRUSHONLY, &filter, &trace );

        // @ CGameMovement::ClipVelocity
        // the flag alone traces the full hull a tick ahead, so a corridor sets it on nearly every
        // record; v dot n at overbounce 1 is what the engine actually subtracts, so requiring it
        // past the noise floor keeps only the pairs a clip really corrupted
        entry.m_wall = trace.fraction < 1.0f && trace.plane.normal.z < 0.7f &&
                       newest->m_velocity.dot( trace.plane.normal ) < -NOISE_FLOOR_UNITS;
    }

    if ( entry.m_velocity.length( ) > g_tf2.sv_maxvelocity->GetFloat( ) )
    {
        entry.m_velocity = vec3{ }; // velocity spike
    }

    const float step{ newest ? ( entry.m_pose.m_origin - newest->m_pose.m_origin ).length2d_sqr( )
                             : 0.0f };
    const bool  broke{ newest && step > this->teleport_distance_sqr( ) };

    if ( broke )
    {
        DBGOUT( "lagcomp[%d]: teleport, %d units over %d ticks, chain cut", index,
                static_cast< i32 >( math::sqrt( step ) ), entry.m_tick - newest->m_tick );
        track.m_break_tick = track.m_last_tick;
    } else if ( track.m_count == 0 )
    {
        DBGOUT( "lagcomp[%d]: tracking from tick %d", index, entry.m_tick );
    }

    track.m_head      = slot;
    track.m_count     = math::smaller( track.m_count + 1, MAX_RECORDS );
    track.m_last_tick = tick;
    track.m_loose     = loose_state_t{ };

    if ( entry.m_gap > 0 )
    {
        track.m_gaps[ track.m_gap_rotor ] = entry.m_gap;
        track.m_gap_rotor                 = ( track.m_gap_rotor + 1 ) % 4;
    }

    g_extrapolate.on_record( index, entry.m_tick, entry.m_pose.m_origin ); // WE EXTRAPOLATIN'
}

void lag_compensation::update( )
{
    if ( !g_tf2.m_engine->IsInGame( ) || !g_cl.m_local )
    {
        return;
    }

    const i32 highest{ math::smaller( g_tf2.m_globals->maxClients, MAX_TRACKED_PLAYERS - 1 ) };
    for ( i32 index{ 1 }; index <= highest; ++index )
    {
        if ( index == g_cl.m_local->entindex( ) )
        {
            continue;
        }

        C_TFPlayer *player{ trackable( index, this->m_tracks[ index ].m_count > 0 ) };

        if ( !player )
        {
            this->invalidate( index );
            continue;
        }

        this->store( index, player );
    }
}

bool lag_compensation::broken( i32 index ) const
{
    const lag_record_t *previous{ this->record( index, 1 ) };
    return previous && !this->continuous( index, *previous );
}

bool lag_compensation::continuous( i32 index, const lag_record_t &entry ) const
{
    return index >= 1 && index < MAX_TRACKED_PLAYERS &&
           entry.m_tick > this->m_tracks[ index ].m_break_tick;
}

bool lag_compensation::hull_at( i32 index, i32 tick, vec3 &origin, vec3 &mins, vec3 &maxs ) const
{
    const lag_record_t *newer{ };
    for ( i32 age{ }; age < this->count( index ); ++age )
    {
        const lag_record_t *entry{ this->record( index, age ) };
        if ( !this->continuous( index, *entry ) )
        {
            return false;
        }

        if ( entry->m_tick <= tick || age == this->count( index ) - 1 )
        {
            origin = entry->m_pose.m_origin;
            mins   = entry->m_mins;
            maxs   = entry->m_maxs;
            if ( newer && entry->m_tick < tick )
            {
                const float fraction{ static_cast< float >( tick - entry->m_tick ) /
                                      static_cast< float >( newer->m_tick - entry->m_tick ) };
                origin += ( newer->m_pose.m_origin - origin ) * fraction;
                mins += ( newer->m_mins - mins ) * fraction;
                maxs += ( newer->m_maxs - maxs ) * fraction;
            }
            return true;
        }
        newer = entry;
    }
    return false;
}

bool lag_compensation::reachable( i32 index, const lag_record_t &entry, i32 ticks_ahead, i32 hold ) const
{
    return this->continuous( index, entry ) && this->rewind_window( ticks_ahead, hold ).accepts( entry.m_tick );
}

const loose_state_t *lag_compensation::loose( i32 index ) const
{
    if ( index < 1 || index >= MAX_TRACKED_PLAYERS || this->m_tracks[ index ].m_count <= 0 )
    {
        return nullptr;
    }

    const loose_state_t &loose{ this->m_tracks[ index ].m_loose };
    return loose.m_server_tick >= 0 ? &loose : nullptr;
}

warp_flow_t lag_compensation::flow( i32 index ) const
{
    warp_flow_t         out{ };
    const lag_record_t *newest{ this->record( index, 0 ) };

    if ( !newest )
    {
        return out;
    }

    const track_t       &track{ this->m_tracks[ index ] };
    const loose_state_t *loose{ this->loose( index ) };
    const lag_record_t  *previous{ this->record( index, 1 ) };

    out.m_newest_server = loose ? loose->m_server_tick : newest->m_server_tick;
    out.m_latest_server = g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick;
    out.m_rhythm        = loose ? 1 : server_rhythm( track.m_server_gaps );
    out.m_burst         = newest->m_link == warp_link_t::burst ? newest->m_commands : out.m_rhythm;
    out.m_charge        = track.m_charge;
    out.m_speed         = ( loose ? loose->m_velocity : newest->m_velocity ).length2d( );
    out.m_loose         = loose != nullptr;

    if ( track.m_over_clock >= 2 && previous && newest->m_server_tick > previous->m_server_tick )
    {
        out.m_rate = static_cast< float >( newest->m_commands ) /
                     static_cast< float >( newest->m_server_tick - previous->m_server_tick );
    }

    return out;
}

rewind_window_t lag_compensation::rewind_window( i32 ticks_ahead, i32 hold ) const
{
    rewind_window_t window{ };
    CNetChannel    *net_channel{ g_tf2.m_engine->GetNetChannelInfo( ) };
    const i32       frame{ g_tickbase.send_frame( ) };
    if ( !net_channel || frame < 0 )
    {
        return window;
    }

    window.m_server_tick = frame + g_networking.send_delay( hold ).ticks( ) + ticks_ahead;
    window.m_interval    = TICK_INTERVAL;
    window.m_unlag       = this->max_unlag( );
    window.m_correct     = math::clamp( net_channel->GetLatency( FLOW_OUTGOING ) +
                                            net_channel->GetLatency( FLOW_INCOMING ) +
                                            TICKS_TO_TIME( TIME_TO_TICKS( lerp_time( ) ) ),
                                        0.0f, window.m_unlag );
    return window;
}

i32 lag_compensation::command_tick( i32 tick ) const
{
    return tick + TIME_TO_TICKS( lerp_time( ) );
}

i32 lag_compensation::command_tick( const lag_record_t &entry ) const
{
    // cmd->tick_count - lerpTicks
    return this->command_tick( entry.m_tick );
}

void lag_compensation::store_bones( C_TFPlayer *player, lag_record_t &entry ) const
{
    entry.m_bone_count = 0;
    entry.m_header     = nullptr;

    CStudioHdr *studio{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< CStudioHdr * >( player ) };

    if ( !studio || !studio->m_pStudioHdr || studio->m_pStudioHdr->numbones > MAX_STUDIO_BONES )
    {
        return;
    }

    alignas( 16 ) vec3       pos[ MAX_STUDIO_BONES ];
    alignas( 16 ) quaternion q[ MAX_STUDIO_BONES ];

    g_tf2.m_mdl_cache->BeginLock( );

    if ( g_bone_mgr.build( studio, entry.m_pose, BONE_USED_BY_ANYTHING, entry.m_bones, pos, q ) )
    {
        entry.m_bone_count = studio->m_pStudioHdr->numbones;
        entry.m_header     = studio->m_pStudioHdr;
    }

    g_tf2.m_mdl_cache->EndLock( );
}

bool lag_compensation::build_bones( C_TFPlayer *player, const lag_record_t *entry, i32 bone_mask,
                                    matrix3x4 *out, i32 max_bones ) const
{
    if ( !player || !out )
    {
        return false;
    }

    if ( entry->m_bone_count > 0 && max_bones >= entry->m_bone_count )
    {
        atl::memcpy( out, entry->m_bones,
                     sizeof( matrix3x4 ) * static_cast< usize >( entry->m_bone_count ) );
        return true;
    }

    // an extrapolated record has a pose the stored bones never saw, so it still has to build
    CStudioHdr *studio{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< CStudioHdr * >( player ) };

    if ( !studio || !studio->m_pStudioHdr || max_bones < studio->m_pStudioHdr->numbones )
    {
        return false;
    }

    alignas( 16 ) vec3       pos[ MAX_STUDIO_BONES ];
    alignas( 16 ) quaternion q[ MAX_STUDIO_BONES ];

    // MDLCACHE_CRITICAL_SECTION
    g_tf2.m_mdl_cache->BeginLock( );
    const bool built{ g_bone_mgr.build( studio, entry->m_pose, bone_mask, out, pos, q ) };
    g_tf2.m_mdl_cache->EndLock( );

    return built;
}

void lag_compensation::draw_records( C_TFPlayer *player, i32 index ) const
{
    CStudioHdr *studio{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< CStudioHdr * >( player ) };

    if ( !studio || !studio->m_pStudioHdr )
    {
        return;
    }

    const studiohdr_t *header{ studio->m_pStudioHdr };

    for ( i32 age{ }; age < this->m_tracks[ index ].m_count; ++age )
    {
        const lag_record_t *entry{ this->record( index, age ) };

        if ( !entry || !this->reachable( index, *entry ) )
        {
            continue;
        }

        // cheap reject before the pose accumulation runs, since a record behind the camera cannot
        // produce a single visible segment anyway
        vec2 anchor{ };
        if ( !g_cl.world_to_screen( entry->m_pose.m_origin, anchor ) )
        {
            continue;
        }

        if ( entry->m_bone_count > 0 )
        {
            draw_skeleton( header, entry->m_bones, age == 0 ? NEWEST_COLOR : RECORD_COLOR );
        }
    }
}

// this will remain here for now for debugging
void lag_compensation::paint( ) const
{
    if ( !g_cfg.m_lagcomp.m_draw_records || !g_tf2.m_engine->IsInGame( ) || !g_cl.m_local )
    {
        return;
    }

    const i32 local_index{ g_cl.m_local->entindex( ) };

    for ( i32 index{ 1 }; index < MAX_TRACKED_PLAYERS; ++index )
    {
        if ( index == local_index || this->m_tracks[ index ].m_count <= 0 )
        {
            continue;
        }

        IClientEntity *client_entity{ g_tf2.m_entity_list->GetClientEntity( index ) };
        if ( !client_entity )
        {
            continue;
        }

        C_BaseEntity *base_entity{ client_entity->GetBaseEntity( ) };
        if ( !base_entity || base_entity->IsDormant( ) )
        {
            continue;
        }

        this->draw_records( base_entity->As< C_TFPlayer * >( ), index );
    }
}
