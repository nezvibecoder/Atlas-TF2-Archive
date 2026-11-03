#include "anims.hpp"
#include "bone_setup.hpp"
#include "entity_cache.hpp"
#include "prediction.hpp"
#include "resolver.hpp"

#include "math/math.hpp"

ATLAS_NO_DESTROY animations g_anims{ };

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/baseanimating.cpp#L437
constexpr float MAX_ANIMTIME_INTERVAL{ 0.2f };

void animations::on_entity_deleted( u32 handle )
{
    const u32 index{ handle & ENT_ENTRY_MASK };
    if ( index > 0 && index < MAX_TRACKED_PLAYERS )
    {
        this->m_records[ index ] = remote_anim_record_t{ };
    }
}

void animations::clear_local( )
{
    this->m_last_command     = 0;
    this->m_last_run_command = 0;
    this->m_rewound          = false;
    this->m_shown.m_command  = -1;
    for ( local_anim_record_t &record : this->m_local_records )
    {
        record.m_command       = -1;
        record.m_event_command = -1;
    }

    for ( local_anim_record_t &record : this->m_copy_records )
    {
        record.m_command = -1;
    }
}

void animations::clear( )
{
    this->clear_local( );
    this->m_snapshot_spawn   = -1;
    this->m_life_command     = 0;
    this->m_replay_player    = nullptr;
    this->m_receiving_packet = false;
    this->m_update_pending   = false;
    this->m_queue_events     = false;
    for ( i32 index{ }; index < MAX_TRACKED_PLAYERS; ++index )
    {
        this->m_prev_anim_times[ index ] = 0.0f;
        this->m_event_counts[ index ]    = 0;
        this->m_records[ index ]         = remote_anim_record_t{ };
        for ( server_layer_t &layer : this->m_server_layers[ index ] )
        {
            layer = server_layer_t{ };
        }
    }
}

const vec3 *animations::replay_velocity( const C_BaseEntity *entity ) const
{
    return entity == this->m_replay_player ? &this->m_replay_velocity : nullptr;
}

void animations::begin_events( bool replay_follows )
{
    this->m_queue_events = replay_follows;
}

bool animations::queue_event( C_BaseEntity *player, i32 event, i32 data )
{
    const i32 index{ player->entindex( ) };
    if ( !this->m_queue_events || index >= MAX_TRACKED_PLAYERS || this->m_event_counts[ index ] >= MAX_QUEUED_EVENTS )
    {
        return false;
    }

    this->m_events[ index ][ this->m_event_counts[ index ]++ ] = anim_event_t{ event, data };
    return true;
}

// The players the replay skipped (no new simulation tick, dormant, dead) get their events now, on their snapshot's tick.
void animations::end_events( )
{
    this->m_queue_events = false;
    for ( i32 index{ }; index < MAX_TRACKED_PLAYERS; ++index )
    {
        if ( this->m_event_counts[ index ] == 0 )
        {
            continue;
        }

        IClientEntity *client_entity{ g_tf2.m_entity_list->GetClientEntity( index ) };
        C_BaseEntity  *player{ client_entity ? client_entity->GetBaseEntity( ) : nullptr };
        if ( player && player->As< C_TFPlayer * >( )->m_PlayerAnimState( ) )
        {
            for ( i32 event{ }; event < this->m_event_counts[ index ]; ++event )
            {
                const anim_event_t &queued{ this->m_events[ index ][ event ] };
                this->play_event( player->As< C_TFPlayer * >( )->m_PlayerAnimState( ), player, queued.m_event, queued.m_data );
            }
        }

        this->m_event_counts[ index ] = 0;
    }
}

// Jump/custom events can call RestartMainSequence; Latching render curtime
// there leaves FrameAdvance waiting for a clock ahead of our remote replay!
void animations::play_event( CTFPlayerAnimState *anim_state, C_BaseEntity *player, i32 event, i32 data ) const
{
    globals_backup_t backup{ };
    backup.store( );
    g_tf2.m_globals->curtime   = player->m_flSimulationTime( );
    g_tf2.m_globals->tickcount = TIME_TO_TICKS( player->m_flSimulationTime( ) );
    g_hooks.m_anim_state_do_animation_event.thiscall< void >( anim_state, event, data );
    // A packet can carry an event without a new player snapshot; Preserve the
    // restart/pose changes even when no remote replay will follow this event.
    player->OnLatchInterpolatedVariables( LATCH_ANIMATION_VAR );
    backup.restore( );
}

void animations::begin_packet( )
{
    this->m_receiving_packet = true;
    this->m_update_pending   = false;
}

bool animations::defer_update( )
{
    this->m_update_pending = this->m_receiving_packet;
    return this->m_receiving_packet;
}

bool animations::end_packet( )
{
    const bool pending{ this->m_update_pending };
    this->m_receiving_packet = false;
    this->m_update_pending   = false;
    return pending;
}

struct anim_state_block_t
{
    usize m_offset;
    usize m_size;
};

constexpr anim_state_block_t LOCAL_ANIM_STATE_BLOCKS[]{ { 8, 8 }, { 56, 12 }, { 100, 100 }, { 292, 12 }, { 312, 40 } };

void animations::save_local( C_TFPlayer *local, i32 command, bool restarted )
{
    local_anim_record_t &record{ this->m_local_records[ static_cast< usize >( command ) % MULTIPLAYER_BACKUP ] };
    this->store_local( local, record );
    record.m_command   = command;
    record.m_tick      = TIME_TO_TICKS( g_tf2.m_globals->curtime );
    record.m_restarted = restarted;
}

bool animations::restore_local( C_TFPlayer *local, i32 command )
{
    const local_anim_record_t &record{ this->m_local_records[ static_cast< usize >( command ) % MULTIPLAYER_BACKUP ] };
    if ( record.m_command != command )
    {
        return false;
    }

    const i32 server_tick{ g_prediction.network_tickbase( ) - 1 };
    float     shift{ };
    if ( command == g_prediction.network_command( ) && ( record.m_tick > server_tick || record.m_restarted ) )
    {
        shift = TICKS_TO_TIME( server_tick - record.m_tick );
    }

    this->load_local( local, record, shift );
    return true;
}

void animations::store_local( C_TFPlayer *local, local_anim_record_t &record )
{
    const u8              *anim_state{ reinterpret_cast< const u8 * >( local->m_PlayerAnimState( ) ) };
    const gesture_slots_t &slots{ local->m_PlayerAnimState( )->m_aGestureSlots( ) };
    const anim_overlay_t  &overlays{ local->m_AnimOverlay( ) };
    const i32              index{ local->entindex( ) };

    for ( const anim_state_block_t &block : LOCAL_ANIM_STATE_BLOCKS )
    {
        atl::memcpy( record.m_anim_state + block.m_offset, anim_state + block.m_offset, block.m_size );
    }

    record.m_gesture_slot_count = math::smaller( slots.m_count, MAX_OVERLAYS );
    record.m_layer_count        = math::smaller( overlays.count( ), MAX_OVERLAYS );
    atl::memcpy( record.m_gesture_slots, slots.m_slots, sizeof( gesture_slot_t ) * record.m_gesture_slot_count );
    atl::memcpy( record.m_layers, overlays.m_layers, sizeof( C_AnimationLayer ) * record.m_layer_count );
    atl::memcpy( record.m_server_layers, this->m_server_layers[ index ], sizeof( record.m_server_layers ) );
    atl::memcpy( record.m_pose_parameters, local->m_flPoseParameter( ), sizeof( record.m_pose_parameters ) );

    record.m_sequence       = local->m_nSequence( );
    record.m_sequence_loops = local->m_bSequenceLoops( );
    record.m_cycle          = local->m_flCycle( );
    record.m_playback_rate  = local->m_flPlaybackRate( );
    record.m_anim_time      = local->m_flAnimTime( );
    record.m_prev_anim_time = this->m_prev_anim_times[ index ];
}

void animations::load_local( C_TFPlayer *local, const local_anim_record_t &record, float shift )
{
    CTFPlayerAnimState *anim_state_object{ local->m_PlayerAnimState( ) };
    u8                 *anim_state{ reinterpret_cast< u8 * >( anim_state_object ) };
    gesture_slots_t    &slots{ anim_state_object->m_aGestureSlots( ) };
    anim_overlay_t     &overlays{ local->m_AnimOverlay( ) };
    const i32           index{ local->entindex( ) };

    for ( const anim_state_block_t &block : LOCAL_ANIM_STATE_BLOCKS )
    {
        atl::memcpy( anim_state + block.m_offset, record.m_anim_state + block.m_offset, block.m_size );
    }

    anim_state_object->m_flJumpStartTime( ) += shift;
    anim_state_object->m_flHoldDeployedPoseUntilTime( ) += shift;

    atl::memcpy( slots.m_slots, record.m_gesture_slots,
                 sizeof( gesture_slot_t ) * math::smaller( record.m_gesture_slot_count, slots.m_count ) );
    atl::memcpy( overlays.m_layers, record.m_layers,
                 sizeof( C_AnimationLayer ) * math::smaller( record.m_layer_count, overlays.count( ) ) );
    atl::memcpy( this->m_server_layers[ index ], record.m_server_layers, sizeof( record.m_server_layers ) );
    atl::memcpy( local->m_flPoseParameter( ), record.m_pose_parameters, sizeof( record.m_pose_parameters ) );

    local->m_nSequence( )            = record.m_sequence;
    local->m_bSequenceLoops( )       = record.m_sequence_loops;
    local->m_flCycle( )              = record.m_cycle;
    local->m_flPlaybackRate( )       = record.m_playback_rate;
    local->m_flAnimTime( )           = record.m_anim_time + shift;
    this->m_prev_anim_times[ index ] = record.m_prev_anim_time + shift;
}

void animations::step_shown( C_TFPlayer *local, i32 command, const anim_clock_t &clock )
{
    const usize                slot{ static_cast< usize >( command ) % MULTIPLAYER_BACKUP };
    const local_anim_record_t &own{ this->m_local_records[ slot ] };
    const local_anim_record_t &current{ this->m_in_copy ? this->m_copy_records[ slot ] : own };
    const vec3                 angles{ local->GetAbsAngles( ) };
    if ( this->m_shown.m_command <= 0 )
    {
        this->m_shown = current;
    }

    this->load_local( local, this->m_shown, 0.0f );

    // the server's copies of our last command each fire their own events, at the copy's step
    const i32 event_count{ own.m_event_command == command ? own.m_event_count : 0 };
    for ( i32 event{ }; event < event_count; ++event )
    {
        const anim_event_t &recorded{ own.m_events[ event ] };
        if ( recorded.m_tick == this->m_copy_steps )
        {
            g_hooks.m_anim_state_do_animation_event.thiscall< void >( local->m_PlayerAnimState( ), recorded.m_event, recorded.m_data );
        }
    }

    local->m_flAnimTime( )                                        = clock.m_anim_time;
    this->m_prev_anim_times[ local->entindex( ) ]                 = clock.m_prev_anim_time;
    local->m_PlayerAnimState( )->m_flJumpStartTime( )             = clock.m_jump_start_time;
    local->m_PlayerAnimState( )->m_flHoldDeployedPoseUntilTime( ) = clock.m_hold_deployed_until;

    // SendPropFloat( m_angEyeAngles[ 0 ], 8, SPROP_CHANGES_OFTEN, -90, 90 )
    const i32 sent{ g_tf2.m_cl->lastoutgoingcommand };
    vec3      eye{ g_tf2.m_input->m_pCommands[ static_cast< usize >( math::smaller( command, sent ) ) % MULTIPLAYER_BACKUP ].viewangles };
    eye.x = math::clamp( eye.x, -90.0f, 90.0f );
    this->step( local, eye );
    this->store_local( local, this->m_shown );
    this->m_shown.m_command = command;

    if ( command <= sent )
    {
        g_bone_mgr.store_shown_pose( local );
    }

    // hkComputeAimYaw turned the entity to the shown state's feet, and prediction only sets its angles again once per pass
    this->load_local( local, current, 0.0f );
    g_tf2.m_CBaseEntity_SetAbsAngles.call_as< void, C_BaseEntity *, const vec3 & >( local, angles );
}

anim_clock_t animations::local_clock( C_TFPlayer *local ) const
{
    CTFPlayerAnimState *anim_state{ local->m_PlayerAnimState( ) };
    return anim_clock_t{ local->m_flAnimTime( ), this->m_prev_anim_times[ local->entindex( ) ], anim_state->m_flJumpStartTime( ),
                         anim_state->m_flHoldDeployedPoseUntilTime( ) };
}

void animations::replay_life_start( C_TFPlayer *local, i32 command )
{
    const float curtime{ g_tf2.m_globals->curtime };
    const i32   first{ math::bigger( this->m_life_command, command - static_cast< i32 >( MULTIPLAYER_BACKUP ) + 1 ) };

    // A respawn keeps the old life's cycle and cached loop flag, a dead one included; Spawn's SetModel only sets sequence 0,
    // whose single frame leaves the cycle alone in the life's first StudioFrameAdvance, and the life's sequence then comes
    // through ResetSequence. The rate the death cam stopped runs again (the server restores it a frame later).
    // https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/baseanimating.cpp#L2684
    if ( this->m_life_base.m_command == first - 1 )
    {
        local->m_nSequence( )                         = 0;
        local->m_bSequenceLoops( )                    = this->m_life_base.m_sequence_loops;
        local->m_flCycle( )                           = this->m_life_base.m_cycle;
        local->m_flPlaybackRate( )                    = 1.0f;
        local->m_flAnimTime( )                        = this->m_life_base.m_anim_time;
        this->m_prev_anim_times[ local->entindex( ) ] = this->m_life_base.m_prev_anim_time;
    }

    for ( i32 missed{ first }; missed < command; ++missed )
    {
        g_tf2.m_globals->curtime = curtime - TICKS_TO_TIME( command - missed );

        const anim_clock_t clock{ this->local_clock( local ) };

        const bool restarted{ this->step( local, g_tf2.m_input->m_pCommands[ static_cast< usize >( missed ) % MULTIPLAYER_BACKUP ].viewangles ) };
        this->save_local( local, missed, restarted );
        this->step_shown( local, missed, clock );
    }

    g_tf2.m_globals->curtime = curtime;
    this->m_life_command     = 0;
}

CTFPlayerAnimState *animations::sent_anim_state( )
{
    const i32            command{ g_tf2.m_cl->lastoutgoingcommand };
    local_anim_record_t &record{ this->m_local_records[ static_cast< usize >( command ) % MULTIPLAYER_BACKUP ] };
    return record.m_command == command ? reinterpret_cast< CTFPlayerAnimState * >( record.m_anim_state ) : nullptr;
}

void animations::record_local_event( C_TFPlayer *local, i32 event, i32 data )
{
    if ( !local->m_pCurrentCommand( ) || !g_tf2.m_prediction->m_bFirstTimePredicted ||
         ( event >= PLAYERANIMEVENT_STUN_BEGIN && event <= PLAYERANIMEVENT_CYOAPDA_END ) )
    {
        return;
    }

    const i32            command{ this->m_last_run_command };
    local_anim_record_t &record{ this->m_local_records[ static_cast< usize >( command ) % MULTIPLAYER_BACKUP ] };
    if ( record.m_event_command != command )
    {
        record.m_event_command = command;
        record.m_event_count   = 0;
    }

    if ( record.m_event_count < MAX_QUEUED_EVENTS )
    {
        record.m_events[ record.m_event_count++ ] = anim_event_t{ event, data, this->m_copy_steps };
    }
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/player.cpp#L3392-L3415
void animations::begin_copy( )
{
    this->m_in_copy = true;
}

void animations::end_copy( )
{
    this->m_in_copy = false;
    ++this->m_copy_steps;
}

// The game rules respawn us before the frame's commands run, so the snapshot that brings a new spawn counter acknowledges
// commands the server ran on the cleared animation state from the one after the previous snapshots!
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/gameinterface.cpp#L1234-L1251
void animations::on_post_data_update( )
{
    if ( !g_cl.m_local )
    {
        return;
    }

    const i32 spawn{ g_cl.m_local->As< C_TFPlayer * >( )->m_iSpawnCounter( ) };
    if ( spawn != this->m_snapshot_spawn )
    {
        this->m_life_command   = this->m_snapshot_spawn < 0 ? 0 : this->m_snapshot_ack + 1;
        this->m_snapshot_spawn = spawn;
    }

    this->m_snapshot_ack = g_tf2.m_cl->command_ack;
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/player.cpp#L4723
ATLAS_FORCE_INLINE bool is_animated( C_TFPlayer *local )
{
    return local->m_nSequence( ) >= 0 && local->m_PlayerAnimState( ) && !local->InCond( ETFCond::TF_COND_TAUNTING );
}

void animations::begin_local_command( C_TFPlayer *local, i32 command, i32 seen_copies )
{
    // A respawn clears the animation state (C_TFPlayer::CleanUpAnimationOnSpawn, and OnNewModel on a class change), and
    // the server ran every command after it on the cleared state: none of the records before it can be rewound to.
    if ( local->m_iSpawnCounter( ) != this->m_local_spawn )
    {
        this->m_local_spawn = local->m_iSpawnCounter( );
        this->m_life_base   = this->m_local_records[ static_cast< usize >( this->m_life_command - 1 ) % MULTIPLAYER_BACKUP ];
        this->clear_local( );
    }

    const bool re_run{ command <= this->m_last_command };
    const bool pass_start{ command != this->m_last_run_command + 1 };
    this->m_last_run_command = command;
    this->m_copy_steps       = 0;
    if ( !is_animated( local ) )
    {
        return;
    }

    if ( !( re_run && pass_start ) && command - 1 != g_prediction.network_command( ) )
    {
        return;
    }

    // the networked state we predict from already holds the server's copies before this command, which prediction then
    // doesn't run again;
    const local_anim_record_t &copied{ this->m_copy_records[ static_cast< usize >( command ) % MULTIPLAYER_BACKUP ] };
    if ( seen_copies > 0 && copied.m_command == command && copied.m_copies == seen_copies )
    {
        this->load_local( local, copied, 0.0f );
        this->m_copy_steps = seen_copies;
        this->m_rewound    = true;
        return;
    }

    this->m_rewound = this->restore_local( local, command - 1 );
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/tf/tf_player.cpp#L2689
void animations::update_local( C_TFPlayer *local )
{
    // crit steering commands might fuck this up, so we use the input slot that runsimulation ran
    const i32  command{ this->m_last_run_command };
    const bool re_run{ command <= this->m_last_command };
    if ( !is_animated( local ) )
    {
        return;
    }

    if ( this->m_last_command == 0 && this->m_life_command > 0 )
    {
        this->replay_life_start( local, command );
    }

    if ( re_run )
    {
        if ( !this->m_rewound )
        {
            return;
        }

        const local_anim_record_t &record{ this->m_local_records[ static_cast< usize >( command ) % MULTIPLAYER_BACKUP ] };
        const i32                  event_count{ record.m_event_command == command ? record.m_event_count : 0 };
        // a copy replays what it fired itself
        for ( i32 event{ }; event < event_count; ++event )
        {
            const anim_event_t &recorded{ record.m_events[ event ] };
            if ( recorded.m_tick == this->m_copy_steps || ( !this->m_in_copy && recorded.m_tick > this->m_copy_steps ) )
            {
                g_hooks.m_anim_state_do_animation_event.thiscall< void >( local->m_PlayerAnimState( ), recorded.m_event, recorded.m_data );
            }
        }
    } else if ( !this->m_in_copy )
    {
        this->m_last_command = command;
    }

    // NOTE: NvK; This used to queue the commands and replay the batch on the send, feeding every command the
    // real yaw while fake angles were on. The server runs the sent fake last in every batch, so its head ends at
    // feet + clamp( fake - feet, -45, 45 ): standing with rage real 180 / fake 0 the real-only pose was 19.4 units off the
    // server's head (p95) with the feet 17 degrees off (p50) in the test server; this way the head is within 0.3.

    // NOTE: Hoku; Yea sure, it would be more accurate to the server simulation of the animations to do what you mentioned previously.
    // However: This causes 2 major issues.
    // 1: The yaw update causes a body lean when the final command gets animated to when the first command of the new batch starts.
    // This causes multiple animation bugs, namely your feet will walk forwards when moving backwards. It also breaks the animation state
    // calculations we use when dictating the ideal auto yaw in anti-aim itself.
    // 2: It causes fake angle chams to operate in a way they really shouldn't be operating.
    // The old fake angle cham code relied off the additional pose/bone update to show the fake angle cham correctly at the very tail end of the batch.
    // It seems fine on paper, until we tweak the animations like this. Then, it causes issues.
    // Realistically: The best solution for fake angles would be to build a new matrix, pose params, anim state, etc. and then build/back them up when we go to draw
    // the matrix.
    // I'm leaving the code like this for now since it technically fixes the problem, but it should be improved upon later down the line.

    // https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/tf/tf_player.cpp#L14011-L14020
    if ( local->m_nPlayerState( ) == TF_STATE_OBSERVER )
    {
        local->m_flPlaybackRate( ) = 0.0f;
    }

    const anim_clock_t clock{ this->local_clock( local ) };

    const vec3 &eye{ local->m_pCurrentCommand( )->viewangles };
    const bool  restarted{ this->step( local, eye ) };
    if ( this->m_in_copy )
    {
        local_anim_record_t &copied{ this->m_copy_records[ static_cast< usize >( command ) % MULTIPLAYER_BACKUP ] };
        this->store_local( local, copied );
        copied.m_command = command;
        copied.m_copies  = this->m_copy_steps + 1;
        if ( !re_run )
        {
            this->step_shown( local, command, clock );
        }

        return;
    }

    this->save_local( local, command, restarted );

    if ( command <= g_tf2.m_cl->lastoutgoingcommand )
    {
        g_bone_mgr.store_pose( local );
    }

    if ( !re_run )
    {
        this->step_shown( local, command, clock );
    }
}

// AddToGestureSlot gives a starting gesture its slot as the order.
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/Multiplayer/multiplayer_animstate.cpp#L663
ATLAS_FORCE_INLINE bool gesture_started( const C_AnimationLayer &layer, bool was_weighted, i32 was_sequence )
{
    return layer.m_flWeight > 0.0f && ( !was_weighted || layer.m_nSequence != was_sequence );
}

// CBaseAnimatingOverlay::FastRemoveLayer moves every active layer above the removed one an order down.
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/BaseAnimatingOverlay.cpp#L1119
ATLAS_FORCE_INLINE void remove_server_layer( server_layer_t *server, i32 count, i32 removed )
{
    for ( i32 index{ }; index < count; ++index )
    {
        server_layer_t &layer{ server[ index ] };
        if ( index != removed && layer.m_order < MAX_OVERLAYS && layer.m_order > server[ removed ].m_order )
        {
            --layer.m_order;
        }
    }

    server[ removed ].m_order    = MAX_OVERLAYS;
    server[ removed ].m_finished = false;
}

// One server animation step: CBasePlayer::PostThink's StudioFrameAdvance, then CTFPlayer::PostThink's animstate update,
// with the gestures moved the way StudioFrameAdvance moves them and ordered the way the server orders them. Returns
// whether the step threw the time since the last advance away: the update restarted the main sequence
// (RestartMainSequence stamps the anim time with curtime and zeroes the cycle), or the clock was new (FrameAdvance
// advances nothing from an anim time of 0, the first command of a life, and only stamps it).
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/client/c_baseanimating.cpp#L5487
bool animations::step( C_TFPlayer *player, const vec3 &angles )
{
    anim_overlay_t &overlays{ player->m_AnimOverlay( ) };
    const i32       count{ math::smaller( overlays.count( ), MAX_OVERLAYS ) };
    server_layer_t *server{ this->m_server_layers[ player->entindex( ) ] };

    // The command's own events (the shot in ItemPostFrame, the jump in the movement) run before the server's
    // StudioFrameAdvance, which then removes the autokill gestures that finished on the last command.
    // https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/BaseAnimatingOverlay.cpp#L266
    for ( i32 index{ }; index < count; ++index )
    {
        server_layer_t         &layer{ server[ index ] };
        const C_AnimationLayer &now{ overlays[ index ] };
        if ( gesture_started( now, layer.m_weighted, layer.m_sequence ) )
        {
            layer.m_order    = index;
            layer.m_finished = false;
        } else if ( now.m_flWeight <= 0.0f && layer.m_weighted )
        {
            layer.m_order    = MAX_OVERLAYS;
            layer.m_finished = false;
        }
    }

    for ( i32 index{ }; index < count; ++index )
    {
        if ( server[ index ].m_finished )
        {
            remove_server_layer( server, count, index );
        }
    }

    const bool  fresh_clock{ player->m_flAnimTime( ) == 0.0f };
    const float layer_interval{ this->advance_main_sequence( player ) };
    const float advanced_cycle{ player->m_flCycle( ) };
    const float advanced_time{ player->m_flAnimTime( ) };

    C_AnimationLayer before[ MAX_OVERLAYS ];
    for ( i32 index{ }; index < count; ++index )
    {
        before[ index ] = overlays[ index ];
    }

    player->m_PlayerAnimState( )->Update( angles.y, angles.x );

    this->match_gesture_advance( player, before, count, layer_interval );

    // The update's own gestures (the landing in HandleJumping) start after the advance, and the autokill ones it ends
    // ended in the server's advance as well, to be removed on the next command.
    for ( i32 index{ }; index < count; ++index )
    {
        server_layer_t         &layer{ server[ index ] };
        C_AnimationLayer       &now{ overlays[ index ] };
        const C_AnimationLayer &was{ before[ index ] };
        if ( gesture_started( now, was.m_flWeight > 0.0f, was.m_nSequence ) )
        {
            layer.m_order    = index;
            layer.m_finished = false;
        } else if ( now.m_flWeight <= 0.0f && was.m_flWeight > 0.0f )
        {
            layer.m_finished = true;
        }

        layer.m_sequence = now.m_nSequence;
        layer.m_weighted = now.m_flWeight > 0.0f;
        if ( layer.m_weighted )
        {
            now.m_nOrder = layer.m_order;
        }
    }

    return fresh_clock || player->m_flCycle( ) < advanced_cycle || player->m_flAnimTime( ) != advanced_time;
}

// The server's CBaseAnimatingOverlay::StudioFrameAdvance. Returns the interval it moves the layers by, GetAnimTimeInterval( )
// read before the main sequence's own advance: the time since the last advance, or the last advance's own interval once
// something restarted the main sequence during this command (the jump event's RestartMainSequence stamps m_flAnimTime with
// curtime in the movement).
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/baseanimating.cpp#L443
float animations::advance_main_sequence( C_TFPlayer *player )
{
    float      &prev_anim_time{ this->m_prev_anim_times[ player->entindex( ) ] };
    const float curtime{ g_tf2.m_globals->curtime };
    const float anim_time{ player->m_flAnimTime( ) };
    const float layer_interval{ anim_time < curtime ? math::smaller( curtime - anim_time, MAX_ANIMTIME_INTERVAL )
                                                    : math::clamp( anim_time - prev_anim_time, 0.0f, MAX_ANIMTIME_INTERVAL ) };

    // CBaseAnimating::StudioFrameAdvance clamps the time since the last advance and skips under a millisecond.
    // https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/baseanimating.cpp#L528
    if ( prev_anim_time == 0.0f )
    {
        prev_anim_time = anim_time;
    }

    const float interval{ math::smaller( curtime - anim_time, MAX_ANIMTIME_INTERVAL ) };
    if ( interval > 0.001f )
    {
        prev_anim_time = anim_time;
        player->FrameAdvance( interval );
    }

    return layer_interval;
}

void animations::match_gesture_advance( C_TFPlayer *player, const C_AnimationLayer *before, i32 count, float layer_interval ) const
{
    anim_overlay_t &overlays{ player->m_AnimOverlay( ) };
    const float     scale{ layer_interval / TICK_INTERVAL };

    for ( i32 index{ }; index < count; ++index )
    {
        C_AnimationLayer       &layer{ overlays[ index ] };
        const C_AnimationLayer &was{ before[ index ] };
        if ( layer.m_flWeight <= 0.0f )
        {
            continue;
        }

        if ( was.m_flWeight <= 0.0f || layer.m_nSequence != was.m_nSequence || layer.m_flCycle < was.m_flCycle )
        {
            layer.m_flCycle     = 0.0f;
            layer.m_flPrevCycle = 0.0f;
            continue;
        }

        layer.m_flCycle = math::smaller( was.m_flCycle + ( layer.m_flCycle - was.m_flCycle ) * scale, 1.0f );
    }
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_gamemovement.cpp#L1023
constexpr float AIR_DASH_SPEED{ 268.3281572999747f };

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_gamemovement.cpp#L1315
ATLAS_FORCE_INLINE float rise_after_jump( i32 jump, i32 last, bool air_jump, float gravity, float &velocity )
{
    const float half{ 0.5f * gravity * TICK_INTERVAL };
    float       rise{ ( JUMP_SPEED - 2.0f * half ) * TICK_INTERVAL };
    velocity = JUMP_SPEED - 3.0f * half;
    for ( i32 tick{ jump + 1 }; tick <= last; ++tick )
    {
        if ( air_jump && tick == jump + 2 )
        {
            rise += AIR_DASH_SPEED * TICK_INTERVAL;
            velocity = AIR_DASH_SPEED - half;
            continue;
        }

        rise += ( velocity - half ) * TICK_INTERVAL;
        velocity -= 2.0f * half;
    }

    return rise;
}

ATLAS_FORCE_INLINE i32 jump_command( const remote_anim_record_t &previous, const remote_anim_record_t &current, i32 earliest,
                                     bool air_jump, float gravity, float &velocity )
{
    velocity = 0.0f;
    if ( current.m_flags & FL_ONGROUND )
    {
        return earliest;
    }

    // a duck in the air (FinishDuck) moves the origin by the hull difference; on the ground it does not, and a jump from
    // the ground needs the duck released first
    const bool from_ground{ ( previous.m_flags & FL_ONGROUND ) != 0 };
    float      top{ current.m_origin.z };
    if ( !from_ground || current.m_hull.z < previous.m_hull.z )
    {
        top += current.m_hull.z - previous.m_hull.z;
    }

    const float half{ 0.5f * gravity * TICK_INTERVAL };
    i32         best{ earliest };
    float       best_error{ 1e30f };
    for ( i32 jump{ from_ground ? earliest : math::bigger( earliest, previous.m_tick + 2 ) }; jump <= current.m_tick; ++jump )
    {
        if ( air_jump && jump + 2 > current.m_tick )
        {
            break;
        }

        float       end_velocity{ };
        const float floor{ top - rise_after_jump( jump, current.m_tick, air_jump, gravity, end_velocity ) };
        float       error{ math::abs( floor - previous.m_origin.z ) };
        if ( !from_ground )
        {
            float above{ previous.m_origin.z };
            float fall{ previous.m_vertical_velocity };
            for ( i32 tick{ previous.m_tick + 1 }; tick < jump - 1; ++tick )
            {
                above += ( fall - half ) * TICK_INTERVAL;
                fall -= 2.0f * half;
            }

            // how far the floor is from the heights the fall crosses on the command before the jump
            const float landed{ above + ( fall - half ) * TICK_INTERVAL };
            error = math::bigger( math::bigger( floor - above, landed - floor ), 0.0f );
        }

        if ( error < best_error )
        {
            best_error = error;
            best       = jump;
            velocity   = end_velocity;
        }
    }

    return best;
}

// The jump goes on the command its kinematics give, the air jump two commands after it, any other event on the batch's
// first command after the events the server fired before it.
i32 animations::place_events( C_TFPlayer *player, const remote_anim_record_t &previous, remote_anim_record_t &current,
                              i32 first_tick, float gravity )
{
    const i32 index{ player->entindex( ) };
    const i32 count{ this->m_event_counts[ index ] };
    bool      air_jump{ };
    for ( i32 event{ }; event < count; ++event )
    {
        air_jump |= this->m_events[ index ][ event ].m_event == PLAYERANIMEVENT_DOUBLEJUMP;
    }

    i32 jump_tick{ -1 };
    i32 earliest{ first_tick };
    for ( i32 event{ }; event < count; ++event )
    {
        anim_event_t &queued{ this->m_events[ index ][ event ] };
        queued.m_tick = earliest;
        if ( queued.m_event == PLAYERANIMEVENT_JUMP && previous.m_valid && current.m_tick > first_tick )
        {
            float velocity{ };
            queued.m_tick               = jump_command( previous, current, earliest, air_jump, gravity, velocity );
            current.m_vertical_velocity = velocity;
            jump_tick                   = queued.m_tick;
        } else if ( queued.m_event == PLAYERANIMEVENT_DOUBLEJUMP && jump_tick >= 0 )
        {
            queued.m_tick = math::smaller( jump_tick + 2, current.m_tick );
        }

        earliest = queued.m_tick;
    }

    return jump_tick;
}

void animations::update_others( )
{
    if ( !g_tf2.m_engine->IsInGame( ) || !g_cl.m_local )
    {
        return;
    }

    const auto players{ g_entity_cache.players( ) };

    const i32 local_index{ g_cl.m_local->entindex( ) };

    for ( const auto *cached : players )
    {
        const i32 index{ cached->m_index };
        if ( !cached->networked( ) || index >= MAX_TRACKED_PLAYERS || index == local_index )
        {
            continue;
        }
        C_BaseEntity         *base_entity{ cached->m_entity };
        remote_anim_record_t &previous{ this->m_records[ index ] };
        if ( base_entity->IsDormant( ) )
        {
            previous = remote_anim_record_t{ };
            continue;
        }

        C_TFPlayer *player{ base_entity->As< C_TFPlayer * >( ) };

        if ( !player->IsAlive( ) || player->m_nSequence( ) < 0 || !player->m_PlayerAnimState( ) ||
             player->InCond( ETFCond::TF_COND_TAUNTING ) )
        {
            previous = remote_anim_record_t{ };
            continue;
        }

        remote_anim_record_t current{ };
        current.m_origin              = player->GetAbsOrigin( );
        current.m_hull                = player->m_vecMaxs( ) - player->m_vecMins( );
        current.m_angles              = player->m_angEyeAngles( );
        current.m_scale               = player->m_flModelScale( );
        current.m_tick                = TIME_TO_TICKS( player->m_flSimulationTime( ) );
        current.m_flags               = player->m_fFlags( );
        current.m_spawn               = player->m_iSpawnCounter( );
        current.m_model               = player->m_nModelIndex( );
        current.m_class               = player->m_iClass( );
        current.m_parent              = player->m_hMoveParent( ).ToInt( );
        current.m_handle              = cached->m_handle;
        current.m_water_level         = player->m_nWaterLevel( );
        current.m_interpolation_frame = player->m_ubInterpolationFrame( );
        current.m_valid               = true;

        const bool same_life{ previous.m_valid && current.m_handle == previous.m_handle &&
                              current.m_spawn == previous.m_spawn && current.m_model == previous.m_model &&
                              current.m_class == previous.m_class && current.m_scale == previous.m_scale &&
                              current.m_parent == previous.m_parent &&
                              current.m_interpolation_frame == previous.m_interpolation_frame };
        if ( same_life && current.m_tick == previous.m_tick )
        {
            continue;
        }

        const i32  gap{ current.m_tick - previous.m_tick };
        const bool continuous{ same_life && gap > 0 && gap <= 24 };
        const i32  first_tick{ continuous ? math::clamp( TIME_TO_TICKS( player->m_flAnimTime( ) ),
                                                         previous.m_tick + 1, current.m_tick )
                                          : current.m_tick };
        const i32  ticks{ current.m_tick - first_tick + 1 };

        this->m_replay_velocity = vec3{ };
        if ( continuous )
        {
            vec3 displacement{ current.m_origin - previous.m_origin };
            if ( !( current.m_flags & FL_ONGROUND ) && !( previous.m_flags & FL_ONGROUND ) )
            {
                displacement += current.m_hull - previous.m_hull; // FinishDuck/FinishUnDuck
            }
            this->m_replay_velocity = displacement / TICKS_TO_TIME( gap );
        }

        // the velocity the last command leaves with, half a tick of gravity under the batch's mean per command it fell
        const float gravity{ g_tf2.sv_gravity->GetFloat( ) * ( player->m_flGravity( ) != 0.0f ? player->m_flGravity( ) : 1.0f ) };
        if ( continuous && !( current.m_flags & FL_ONGROUND ) )
        {
            current.m_vertical_velocity = this->m_replay_velocity.z - 0.5f * gravity * TICKS_TO_TIME( gap );
        }

        const i32 jump_tick{ this->place_events( player, previous, current, first_tick, gravity ) };
        const i32 event_count{ this->m_event_counts[ index ] };
        i32       next_event{ };

        globals_backup_t backup{ };
        backup.store( );

        g_tf2.m_globals->frametime = TICK_INTERVAL;
        this->m_replay_player      = player;

        if ( !continuous )
        {
            // There is no observed interval to advance across a new life, teleport,
            // model change or visibility gap. Establish a new animation clock.
            player->m_flAnimTime( )          = TICKS_TO_TIME( current.m_tick - 1 );
            this->m_prev_anim_times[ index ] = TICKS_TO_TIME( current.m_tick - 2 );
        }

        if ( !continuous || ticks > 1 )
        {
            for ( server_layer_t &layer : this->m_server_layers[ index ] )
            {
                layer = server_layer_t{ };
            }
        }

        g_resolver.begin_batch( player, current, ticks, continuous );

        for ( i32 tick{ }; tick < ticks; ++tick )
        {
            const i32 replay_tick{ current.m_tick - ticks + tick + 1 };
            g_tf2.m_globals->curtime   = TICKS_TO_TIME( replay_tick );
            g_tf2.m_globals->tickcount = replay_tick;

            const bool    endpoint{ tick == ticks - 1 };
            constexpr i32 movement_flags{ FL_ONGROUND | FL_DUCKING };
            player->m_fFlags( )      = endpoint ? current.m_flags : ( current.m_flags & ~movement_flags ) | ( previous.m_flags & movement_flags );
            player->m_nWaterLevel( ) = endpoint ? current.m_water_level : previous.m_water_level;
            if ( jump_tick >= 0 && !endpoint )
            {
                // CheckJumpButton runs on the ground, so the command before the jump stood (a landing inside the batch)
                if ( replay_tick >= jump_tick )
                {
                    player->m_fFlags( ) &= ~FL_ONGROUND;
                } else if ( replay_tick == jump_tick - 1 )
                {
                    player->m_fFlags( ) |= FL_ONGROUND;
                }
            }

            // the server fires them in the command's movement and ItemPostFrame, before PostThink animates it
            for ( ; next_event < event_count && this->m_events[ index ][ next_event ].m_tick <= replay_tick; ++next_event )
            {
                const anim_event_t &queued{ this->m_events[ index ][ next_event ] };
                g_hooks.m_anim_state_do_animation_event.thiscall< void >( player->m_PlayerAnimState( ), queued.m_event, queued.m_data );
            }

            const float fraction{ continuous ? static_cast< float >( replay_tick - previous.m_tick ) / gap : 1.0f };
            const float yaw{ continuous ? previous.m_angles.y +
                                              math::normalize_yaw( current.m_angles.y - previous.m_angles.y ) * fraction
                                        : current.m_angles.y };
            const float pitch{ continuous ? previous.m_angles.x +
                                                ( current.m_angles.x - previous.m_angles.x ) * fraction
                                          : current.m_angles.x };

            const float vertical_velocity{ this->m_replay_velocity.z };
            if ( player->m_fFlags( ) & FL_ONGROUND )
            {
                this->m_replay_velocity.z = 0.0f;
            }

            // https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/Multiplayer/multiplayer_animstate.cpp#L1709
            const bool moving{ this->m_replay_velocity.length( ) > 1.0f };
            this->step( player, g_resolver.command_angles( player, vec3{ pitch, yaw, 0.0f }, current.m_angles, endpoint, moving ) );
            this->m_replay_velocity.z = vertical_velocity;
        }

        this->m_event_counts[ index ] = 0;
        this->m_replay_player         = nullptr;
        previous                      = current;

        g_resolver.end_batch( player );

        // PreDataUpdate restores cycle/poses from the interpolation watchers even
        // when Interpolate is bypassed; We replaced UpdateClientSideAnimation, so
        // we also own its latch; save the final replay state before the next packet..
        player->OnLatchInterpolatedVariables( LATCH_ANIMATION_VAR );

        g_bone_mgr.store_pose( player );
        backup.restore( );
    }
}
