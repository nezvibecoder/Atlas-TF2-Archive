#pragma once

#include "link.hpp"

struct remote_anim_record_t;

inline constexpr i32 RESOLVER_YAWS{ 7 };

inline constexpr i32 RESOLVER_PITCHES{ 3 };

struct resolver_track_t
{
    float m_goal[ RESOLVER_YAWS ]{ };
    float m_feet[ RESOLVER_YAWS ]{ };
    u32   m_handle{ };
    i32   m_yaw{ };
    i32   m_pitch{ };
    i32   m_applied{ };
    i32   m_tick{ };
    i32   m_switch_tick{ };
    float m_sent_yaw{ };
    bool  m_tracked{ };
    bool  m_engaged{ };
    bool  m_continuous{ };
    bool  m_seeded{ };
    bool  m_hidden_yaw{ };
    bool  m_hidden_pitch{ };
};

struct resolver_shot_t
{
    i32  m_command{ };
    i32  m_health{ };
    i32  m_weapon{ };
    bool m_pending{ };
    bool m_acked{ };
};

class resolver
{
public:
    void begin_batch( C_TFPlayer *player, const remote_anim_record_t &current, i32 ticks, bool continuous );
    vec3 command_angles( C_TFPlayer *player, const vec3 &interpolated, const vec3 &sent, bool endpoint, bool moving );
    void end_batch( C_TFPlayer *player );

    void on_shot( C_TFPlayer *player, i32 tick, i32 hitbox, const vec3 &eye, const vec3 &point );
    void on_player_hurt( IGameEvent *event );
    void think( );
    void clear( );

    bool guessing( i32 index ) const;

private:
    bool aimed_at_record_head( C_TFPlayer *player, i32 tick, const vec3 &eye, const vec3 &point ) const;
    void settle( i32 index, bool head );

    resolver_track_t m_tracks[ MAX_TRACKED_PLAYERS ]{ };
    resolver_shot_t  m_shots[ MAX_TRACKED_PLAYERS ]{ };
};

extern resolver g_resolver;
