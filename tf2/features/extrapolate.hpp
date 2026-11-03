#pragma once

#include "lagcomp.hpp"

constexpr i32 MAX_PATH_TICKS{ 66 };

struct move_sim_player_t;

struct warp_plan_t
{
    i32  m_commands{ };
    i32  m_warp{ };
    bool m_breaks{ };
    bool m_frozen{ };
    bool m_valid{ };
};

struct predicted_state_t
{
    vec3        m_origin{ };
    vec3        m_velocity{ };
    i32         m_tick{ };
    i32         m_step{ };
    float       m_turn_rate{ };
    i32         m_pairs{ };
    const char *m_stop{ };
    bool        m_on_ground{ };
    bool        m_valid{ true };
};

struct local_duck_t
{
    vec3  m_stand_maxs{ };
    vec3  m_duck_maxs{ };
    float m_stand_view{ };
    float m_duck_view{ };
    float m_view{ };
    float m_ducktime{ };
    float m_timer{ };
    i32   m_air_ducks{ };
    bool  m_ducked{ };
    bool  m_ducking{ };
    bool  m_held{ };
};

class extrapolation
{
public:
    warp_plan_t         plan( i32 index, i32 ahead = 0 ) const;
    bool                predict( i32 index, i32 commands, predicted_state_t &out ) const;
    i32                 predict_path( i32 index, predicted_state_t *out, i32 count ) const;
    void                update_local_input( );
    i32                 simulate_local( i32 ticks, vec3 *eyes, i32 capacity, vec3 *path, bool *grounded ) const;
    const lag_record_t *synthesize( i32 index, i32 commands, lag_record_t &storage );
    void                on_record( i32 index, i32 tick, const vec3 &origin );
    void                forget( i32 index );
    void                paint( ) const;
    void                clear( );

private:
    struct local_input_t
    {
        i32   m_command{ -1 };
        float m_view_yaw{ };
        float m_wish_yaw{ };
        float m_view_delta{ };
        float m_wish_delta{ };
        float m_view_turn{ };
        float m_wish_turn{ };
    };

    local_input_t m_local_input{ };

    struct sim_context_t
    {
        float       m_maxspeed{ };
        float       m_wishspeed{ };
        float       m_turn_rate{ };
        float       m_turn_age{ };
        i32         m_pairs{ };
        const char *m_stop{ };
        vec3        m_mins{ };
        vec3        m_maxs{ };
        bool        m_ducked{ };
        bool        m_swimming{ };
        bool        m_parachuting{ };
    };

    struct obstacle_t
    {
        i32               m_index{ };
        i32               m_commands{ };
        vec3              m_networked{ };
        sim_context_t     m_context{ };
        predicted_state_t m_state{ };
    };

    sim_context_t     build_context( i32 index, const lag_record_t *newest ) const;
    predicted_state_t seed( i32 index, const lag_record_t *newest ) const;
    void              step( predicted_state_t &state, const sim_context_t &context ) const;
    void              remember( i32 index, const predicted_state_t &state, const lag_record_t *from );
    i32               gather_obstacles( C_TFPlayer *local, i32 ticks, move_sim_player_t *players, obstacle_t *obstacles ) const;
    void              advance_obstacles( i32 ahead, move_sim_player_t *players, obstacle_t *obstacles, i32 count ) const;

    struct pending_t
    {
        vec3        m_origin{ };
        vec3        m_from_origin{ };
        float       m_turn_rate{ };
        i32         m_pairs{ };
        const char *m_stop{ };
        i32         m_tick{ -1 };
        i32         m_from_tick{ -1 };
    };

    pending_t m_pending[ MAX_TRACKED_PLAYERS ]{ };

    struct seed_origin_t
    {
        vec3 m_origin{ };
        i32  m_server_tick{ -1 }; // of the state it was unstuck for, a record or a loose one
        bool m_valid{ true };
    };

    mutable seed_origin_t m_seed_origins[ MAX_TRACKED_PLAYERS ]{ };
};

extern extrapolation g_extrapolate;
