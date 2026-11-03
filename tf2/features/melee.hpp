#pragma once

#include "extrapolate.hpp"
#include "lagcomp.hpp"
#include "link.hpp"

struct melee_point_t
{
    vec3 m_mins{ };
    vec3 m_maxs{ };
    i32  m_tick{ -1 };
};

struct melee_target_t
{
    C_BaseEntity *m_entity{ nullptr };
    f32           m_distance{ };
    bool          m_is_player{ false };
};

struct swing_ctx_t
{
    f32  m_range{ 48.0f };
    vec3 m_hull_mins{ };
    vec3 m_hull_maxs{ };
    i32  m_smack_ticks{ };
};

struct swing_view_t
{
    vec3            m_eye{ };
    rewind_window_t m_window{ };
    i32             m_ticks{ };
};

struct swing_choice_t
{
    C_BaseEntity *m_entity{ nullptr };
    melee_point_t m_point{ };
    f32           m_margin{ };
};

struct own_path_t
{
    vec3 m_eyes[ 2 ]{ };
    bool m_airborne{ false };
    bool m_simulated{ false };
};

struct own_forecast_t
{
    i32  m_tickbase{ -1 };
    vec3 m_eye{ };
    bool m_airborne{ false };
};

struct own_stray_t
{
    i32 m_tickbase{ -1 };
    f32 m_error{ };
};

class melee_assist
{
public:
    void think( );

    bool aimed_this_command( ) const
    {
        return g_cl.m_cmd && this->m_aimed_command == g_cl.m_cmd->command_number;
    }

private:
    bool should_run( ) const;
    bool weapon_ready( ) const;
    void build_swing_ctx( swing_ctx_t &ctx ) const;
    bool allow_player( C_TFPlayer *player ) const;
    bool allow_building( C_BaseEntity *building, ETFClassID id ) const;
    void fetch_targets( );
    bool smack_this_command( );
    bool select_record( C_TFPlayer *player, const vec3 &eye, const swing_ctx_t &ctx, const rewind_window_t &window,
                        bool backstab, melee_point_t &point, vec3 &angle ) const;

    void weigh_player( C_TFPlayer *player, const swing_ctx_t &ctx, const swing_view_t *views, i32 view_count,
                       swing_choice_t *choices );
    void weigh_building( C_BaseEntity *building, const swing_ctx_t &ctx, const swing_view_t *views, i32 view_count,
                         swing_choice_t *choices );
    void weigh_targets( const swing_ctx_t &ctx, const swing_view_t *views, i32 view_count, swing_choice_t *choices );
    void track_own_path( const swing_ctx_t &ctx, own_path_t &own );
    f32  own_stray( i32 smack ) const;
    bool should_swing( const swing_ctx_t &ctx, const own_path_t &own );

    bool steer_smack( const swing_ctx_t &ctx );
    bool try_backstab( const swing_ctx_t &ctx );
    void aim( const vec3 &angle, i32 declare_tick );

    atl::vector< melee_target_t > m_targets{ };
    f32                           m_smack_time{ -1.0f };
    u32                           m_smack_weapon{ INVALID_EHANDLE_INDEX };
    i32                           m_auto_command{ -1 };
    i32                           m_aimed_command{ -1 };
    own_forecast_t                m_forecasts[ MAX_PATH_TICKS ]{ };
    own_stray_t                   m_strays[ MAX_PATH_TICKS ]{ };
};

extern melee_assist g_melee;
