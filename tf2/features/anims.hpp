#pragma once

#include "link.hpp"

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/Multiplayer/multiplayer_animstate.cpp#L1702
ATLAS_FORCE_INLINE void step_feet_yaw( f32 &goal_feet, f32 &current_feet, f32 eye_yaw, bool moving )
{
    if ( moving )
    {
        goal_feet = eye_yaw;
    } else
    {
        const f32 delta{ math::normalize_yaw( goal_feet - eye_yaw ) };
        if ( math::abs( delta ) > 45.0f )
        {
            goal_feet += delta > 0.0f ? -45.0f : 45.0f;
        }
    }

    goal_feet = math::normalize_yaw( goal_feet );
    if ( goal_feet == current_feet )
    {
        return;
    }

    // https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/Multiplayer/multiplayer_animstate.cpp#L1793
    const f32 raw{ goal_feet - current_feet };
    const f32 step{ 720.0f * TICK_INTERVAL * math::clamp( math::abs( raw ) / 60.0f, 0.01f, 1.0f ) };
    if ( math::abs( raw ) < step )
    {
        current_feet = goal_feet;
        return;
    }

    current_feet = math::normalize_yaw( current_feet + ( math::normalize_yaw( raw ) < 0.0f ? -step : step ) );
}

struct remote_anim_record_t
{
    vec3  m_origin{ };
    vec3  m_hull{ };
    vec3  m_angles{ };
    float m_scale{ };
    i32   m_tick{ };
    i32   m_flags{ };
    i32   m_spawn{ };
    i32   m_model{ };
    i32   m_class{ };
    i32   m_parent{ };
    u32   m_handle{ };
    float m_vertical_velocity{ };
    u8    m_water_level{ };
    u8    m_interpolation_frame{ };
    bool  m_valid{ };
};

inline constexpr i32 MAX_QUEUED_EVENTS{ 8 };

inline constexpr usize ANIM_STATE_SIZE{ 384 };

struct anim_event_t
{
    i32 m_event{ };
    i32 m_data{ };
    i32 m_tick{ };
};

struct anim_clock_t
{
    f32 m_anim_time{ };
    f32 m_prev_anim_time{ };
    f32 m_jump_start_time{ };
    f32 m_hold_deployed_until{ };
};

struct server_layer_t
{
    i32  m_sequence{ };
    i32  m_order{ MAX_OVERLAYS };
    bool m_weighted{ };
    bool m_finished{ };
};

struct local_anim_record_t
{
    u8               m_anim_state[ ANIM_STATE_SIZE ]{ };
    gesture_slot_t   m_gesture_slots[ MAX_OVERLAYS ]{ };
    C_AnimationLayer m_layers[ MAX_OVERLAYS ]{ };
    server_layer_t   m_server_layers[ MAX_OVERLAYS ]{ };
    float            m_pose_parameters[ MAX_POSE_PARAMETERS ]{ };
    anim_event_t     m_events[ MAX_QUEUED_EVENTS ]{ };
    i32              m_command{ };
    i32              m_tick{ };
    bool             m_restarted{ };
    bool             m_sequence_loops{ };
    i32              m_event_command{ };
    i32              m_event_count{ };
    i32              m_gesture_slot_count{ };
    i32              m_layer_count{ };
    i32              m_copies{ };
    i32              m_sequence{ };
    float            m_cycle{ };
    float            m_playback_rate{ };
    float            m_anim_time{ };
    float            m_prev_anim_time{ };
};

class animations
{
public:
    void begin_local_command( C_TFPlayer *local, i32 command, i32 seen_copies );
    void update_local( C_TFPlayer *local );
    void update_others( );
    void clear( );
    void on_entity_deleted( u32 handle );
    void begin_packet( );
    bool defer_update( );
    bool end_packet( );
    const vec3 *replay_velocity( const C_BaseEntity *entity ) const;

    void begin_events( bool replay_follows );
    bool queue_event( C_BaseEntity *player, i32 event, i32 data );
    void end_events( );
    void play_event( CTFPlayerAnimState *anim_state, C_BaseEntity *player, i32 event, i32 data ) const;
    void record_local_event( C_TFPlayer *local, i32 event, i32 data );
    void begin_copy( );
    void end_copy( );
    void on_post_data_update( );

    CTFPlayerAnimState *sent_anim_state( );

private:
    void  clear_local( );
    void  save_local( C_TFPlayer *local, i32 command, bool restarted );
    bool  restore_local( C_TFPlayer *local, i32 command );
    void  store_local( C_TFPlayer *local, local_anim_record_t &record );
    void  load_local( C_TFPlayer *local, const local_anim_record_t &record, float shift );
    void         step_shown( C_TFPlayer *local, i32 command, const anim_clock_t &clock );
    anim_clock_t local_clock( C_TFPlayer *local ) const;
    void         replay_life_start( C_TFPlayer *local, i32 command );
    bool  step( C_TFPlayer *player, const vec3 &angles );
    float advance_main_sequence( C_TFPlayer *player );
    void  match_gesture_advance( C_TFPlayer *player, const C_AnimationLayer *before, i32 count, float layer_interval ) const;
    i32   place_events( C_TFPlayer *player, const remote_anim_record_t &previous, remote_anim_record_t &current, i32 first_tick,
                        float gravity );

    i32                  m_last_command{ };
    i32                  m_last_run_command{ };
    i32                  m_local_spawn{ };
    i32                  m_snapshot_spawn{ -1 };
    i32                  m_snapshot_ack{ };
    i32                  m_life_command{ };
    local_anim_record_t  m_life_base{ };
    bool                 m_rewound{ };
    bool                 m_in_copy{ };
    i32                  m_copy_steps{ };
    local_anim_record_t  m_local_records[ MULTIPLAYER_BACKUP ]{ };
    local_anim_record_t  m_copy_records[ MULTIPLAYER_BACKUP ]{ }; // by the command the server's copies ran before
    local_anim_record_t  m_shown{ };
    float                m_prev_anim_times[ MAX_TRACKED_PLAYERS ]{ };
    server_layer_t       m_server_layers[ MAX_TRACKED_PLAYERS ][ MAX_OVERLAYS ]{ };
    remote_anim_record_t m_records[ MAX_TRACKED_PLAYERS ]{ };
    anim_event_t         m_events[ MAX_TRACKED_PLAYERS ][ MAX_QUEUED_EVENTS ]{ };
    i32                  m_event_counts[ MAX_TRACKED_PLAYERS ]{ };
    C_BaseEntity        *m_replay_player{ };
    vec3                 m_replay_velocity{ };
    bool                 m_receiving_packet{ };
    bool                 m_update_pending{ };
    bool                 m_queue_events{ };
};

extern animations g_anims;
