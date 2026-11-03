#pragma once

#include "link.hpp"

struct medbot_target_t
{
    C_TFPlayer *m_player{ nullptr };
    i32         m_priority{ 0 };
    bool        m_is_healable{ false };
    f32         m_health_frac{ 0.0f };
    bool        m_is_overhealed{ false };
    bool        m_is_friend{ false };
};

class medbot
{
public:
    void think( );
    void on_level_shutdown( );

private:
    bool        should_run( );
    void        get_medbot_targets( );
    C_TFPlayer *get_best_heal_target( );

    void handle_teammate( C_TFPlayer *player );

    i32  get_base_class_priority( C_TFPlayer *player );
    bool is_volatile_class( C_TFPlayer *player );

    bool is_player_in_healable_pos( C_TFPlayer *player );
    void place_beam( C_TFPlayer *player );

    atl::vector< medbot_target_t > m_targets{ };

    C_TFPlayer *m_uber_lock_target{ nullptr };
    f32         m_next_search_time{ 0.0f };
};

extern medbot g_medbot;
