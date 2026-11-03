#pragma once

#include "link.hpp"

#include "proj_sim.hpp"

struct trajectory_landing_t
{
    vec3 m_position{ };
    vec3 m_normal{ };
    f32  m_time{ };
    f32  m_yaw{ };
    bool m_hit{ };
};

class trajectory
{
public:
    static constexpr i32 SPLASH_POINTS{ 48 };

    void capture( );
    void on_create_move( );
    void paint( );
    void clear( );

    const trajectory_landing_t &landing( ) const
    {
        return this->m_landing;
    }

private:
    bool wanted( ) const;
    void mark_visibility( const projectile_setup_t &setup, i32 interval );
    void trace_splash( );
    void draw_markers( );

    projectile_setup_t m_setup{ };
    f32                m_setup_radius{ };

    vec3                 m_path[ projectile_sim::MAX_PATH_POINTS ]{ };
    bool                 m_visible[ projectile_sim::MAX_PATH_POINTS ]{ };
    i32                  m_count{ };
    vec3                 m_hull{ };
    vec3                 m_hull_centre{ };
    f32                  m_radius{ };
    vec3                 m_splash[ SPLASH_POINTS ]{ };
    bool                 m_splash_valid{ };
    trajectory_landing_t m_landing{ };

    f32 m_shown_time{ };
    u64 m_shown_tick{ };
};

extern trajectory g_trajectory;
