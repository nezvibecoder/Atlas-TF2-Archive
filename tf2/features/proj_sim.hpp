#pragma once

#include "link.hpp"

enum class projectile_movetype : u8
{
    fly,         // MOVETYPE_FLY
    fly_gravity, // MOVETYPE_FLYGRAVITY
    vphysics,    // MOVETYPE_VPHYSICS
    flame,       // CTFPointManager points: dragged velocity plus a constant rise and carry
};

struct collide_info_t
{
    const CPhysCollide *m_collide{ };
    objectparams_t      m_params{ };
    i32                 m_material{ };
    vec3                m_half_extents{ };
    vec3                m_centre{ }; // the mesh AABB is not required to sit on the origin
    bool                m_resolved{ };
    bool                m_attempted{ };
};

enum class projectile_model : u8
{
    none,
    rocket,
    pipe,
    sticky,
    sticky_practice,
    cannonball,
    cleaver,
    baseball,
    ornament,
    jarate,
    madmilk,
    breadmonster,
    breadmonster_milk,
    gascan,
    sandvich,
    count,
};

struct projectile_setup_t
{
    vec3  m_position{ };
    vec3  m_angles{ };
    vec3  m_hull{ };
    vec3  m_hull_centre{ };
    float m_speed{ };
    float m_gravity{ };
    float m_lifetime{ };

    projectile_movetype m_movetype{ projectile_movetype::fly };
    projectile_model    m_model{ projectile_model::none };

    float m_up_kick{ };
    float m_jitter_right{ };
    float m_jitter_up{ };
    vec3  m_angular_velocity{ };

    float m_drag{ };
    float m_rise{ };
    vec3  m_carry{ };

    bool m_valid{ false };
    bool m_blocked{ false };
};

struct impact_t
{
    vec3    m_position{ };
    vec3    m_normal{ };
    f32     m_time{ };
    bool    m_hit{ };
    trace_t m_trace{ };
    bool    m_attached{ };
    bool    m_deferred{ };
    bool    m_target_hit{ };
};

struct projectile_target_t
{
    const vec3   *m_track{ };
    vec3          m_mins{ }, m_maxs{ };
    C_BaseEntity *m_entity{ };
};

struct projectile_work_t
{
    static constexpr i32 MAX_FLIGHTS{ 12 };
    static constexpr i32 MAX_EXACT{ 128 };
    static constexpr i32 MAX_STEPS{ 2048 };
    static constexpr i32 FINISH_STEPS{ 384 };
    static constexpr i32 MAX_PREPARATIONS{ 4 };

    i32  m_frame{ -1 };
    i32  m_flights{ };
    i32  m_exact{ };
    i32  m_steps{ };
    i32  m_damage{ };
    i32  m_preparations{ };
    bool m_exhausted{ };
    bool m_finalizing{ };

    void begin( i32 frame )
    {
        if ( frame == m_frame )
        {
            return;
        }

        m_frame   = frame;
        m_flights = m_exact = m_steps = m_damage = m_preparations = 0;
        m_exhausted                              = false;
        m_finalizing                             = false;
    }

    bool available( ) const
    {
        return !m_exhausted;
    }

    bool prepare( )
    {
        if ( m_preparations >= MAX_PREPARATIONS )
        {
            m_exhausted = true;
        }
        if ( !available( ) )
        {
            return false;
        }
        ++m_preparations;
        return true;
    }

    bool flight( )
    {
        if ( m_flights >= MAX_FLIGHTS - ( m_finalizing ? 0 : 2 ) )
        {
            m_exhausted = true; // im tired boss
        }
        if ( !available( ) )
        {
            return false;
        }
        ++m_flights;
        return true;
    }

    bool exact( )
    {
        if ( m_exact >= MAX_EXACT - ( m_finalizing ? 0 : 32 ) )
        {
            m_exhausted = true;
        }
        if ( !available( ) )
        {
            return false;
        }
        ++m_exact;
        return true;
    }

    bool damage( i32 limit )
    {
        if ( m_damage >= limit )
        {
            m_exhausted = true;
        }
        if ( !available( ) )
        {
            return false;
        }
        ++m_damage;
        return true;
    }

    bool step( )
    {
        if ( m_steps >= MAX_STEPS - ( m_finalizing ? 0 : FINISH_STEPS ) )
        {
            m_exhausted = true;
        }
        if ( m_exhausted )
        {
            return false;
        }
        ++m_steps;
        return true;
    }

    void finalize( bool enabled )
    {
        m_finalizing = enabled;
        m_exhausted  = m_flights >= MAX_FLIGHTS - ( enabled ? 0 : 2 ) ||
                       m_steps >= MAX_STEPS - ( enabled ? 0 : FINISH_STEPS ) ||
                       m_exact >= MAX_EXACT - ( enabled ? 0 : 32 );
    }
};

class projectile_sim
{
public:
    static constexpr i32 MAX_PATH_POINTS{ 668 };

    // charge_time is the seconds of charge to launch with; negative reads the weapon's own
    bool gather( C_TFPlayer *player, C_TFWeaponBase *weapon, const vec3 &angles,
                 projectile_setup_t &out, i32 command = 0, f32 charge_time = -1.0f,
                 const projectile_target_t *target = nullptr );
    bool resume_sticky( C_BaseEntity *pipe, const vec3 &position, const vec3 &velocity, const vec3 &spin,
                        projectile_setup_t &out ) const;
    i32  simulate( const projectile_setup_t &setup, vec3 *path, i32 max_ticks, i32 trace_interval, impact_t &impact, C_BaseEntity *target = nullptr, projectile_work_t *work = nullptr, const projectile_target_t *intercept = nullptr );
    void capture_shot( );
    void compensate( );
    void on_create_move( );
    void render( ) const; // inside the 3D view; the path goes through the engine's RenderLine
    void shutdown( );

    vec3 get_fire_offset( C_TFWeaponBase *gun, bool ducked );
    void snap_to_eye( C_TFPlayer *player, vec3 &position ) const;

private:
    void  apply_launch_randomness( projectile_setup_t &out, C_TFWeaponBase *weapon, i32 command, bool angular = false );
    void  clear_path( );
    bool  arm_physics( const projectile_setup_t &setup );
    void  release_physics( );
    float sv_gravity( ) const;

    IPhysicsEnvironment *m_environment{ };
    IPhysicsObject      *m_object{ };

    float m_environment_gravity{ };
    float m_environment_timestep{ };

    projectile_setup_t m_shot_setup{ };
    vec3               m_shot_angles{ };
    i32                m_shot_command{ };
    vec3               m_shot_path[ MAX_PATH_POINTS ]{ };
    i32                m_shot_count{ };
    double             m_shot_time{ };
    i32                m_shot_tick{ };
    bool               m_shot_hit{ };
    vec3               m_impact_angles{ };
};

extern projectile_sim g_proj_sim;
