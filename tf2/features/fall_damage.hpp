#pragma once

#include "link.hpp"

enum class fall_outcome_t : u8
{
    open,
    saved,
    landed,
    hurt,
};

enum class jump_bug_verdict_t : u8
{
    none,
    works,
    fails,
};

struct fall_state_t
{
    vec3 m_origin{ };
    vec3 m_velocity{ };
    bool m_ducked{ };
    bool m_held{ };
    i32  m_air_ducks{ };
};

struct fall_physics_t
{
    C_TFPlayer *m_local{ };
    vec3        m_forward{ };
    vec3        m_right{ };
    vec3        m_wish{ };
    f32         m_scale{ };
    f32         m_lift{ };
    f32         m_half_gravity{ };
    f32         m_max_speed{ };
};

struct fall_result_t
{
    fall_outcome_t m_outcome{ };
    i32            m_ticks{ };
    f32            m_ground{ };
};

struct edge_bug_plan_t
{
    bool m_duck{ };
    bool m_steer{ };
    i32  m_brake{ };
};

class fall_damage
{
public:
    void run( );
    void paint( ) const;

private:
    bool jump_bug( const fall_physics_t &physics, const fall_state_t &state ) const;
    void edge_bug( const fall_physics_t &physics, const fall_state_t &state, f32 next_fall );

    jump_bug_verdict_t m_verdict{ };

#if defined( ATLAS_DEV )
    i32 m_expected_command{ -1 };
#endif
};

extern fall_damage g_fall_damage;
