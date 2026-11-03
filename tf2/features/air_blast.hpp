#pragma once

#include "link.hpp"

class air_blast
{
public:
    void run( );

private:
    enum class family_t : u8
    {
        none,
        rocket,
        grenade,
    };

    struct threat_t
    {
        C_BaseEntity *m_projectile{ };
        family_t      m_family{ family_t::none };
        vec3          m_from{ };
        vec3          m_to{ };
        vec3          m_heading{ };
        float         m_speed{ };
        float         m_runway{ };
        bool          m_critical{ };
    };

    static family_t family_of( ETFClassID class_id );
    static vec3     velocity_of( C_BaseEntity *projectile, family_t family );

    bool find_projectile( threat_t &best, vec3 &angles ) const;
    bool find_player( vec3 &angles ) const;

    bool locate( C_BaseEntity *projectile, ETFClassID class_id, threat_t &out ) const;
    bool covers( const vec3 &from, const vec3 &to, const vec3 &angles ) const;
    bool will_cover( const threat_t &threat, const vec3 &angles ) const;
    bool aim_for( const vec3 &from, const vec3 &to, vec3 &angles ) const;
    bool redirect( const threat_t &threat, vec3 &angles );

    vec3  m_eye{ };
    float m_radius{ };
    i32   m_snapshot_tick{ };
    i32   m_command_tick{ };

#if defined( ATLAS_DEV )
    struct claim_t
    {
        u32  m_projectile{ INVALID_EHANDLE_INDEX };
        vec3 m_aim_point{ };
        vec3 m_start{ };
        vec3 m_last{ };
        bool m_turned{ };
    };

    void grade( );

    claim_t m_claim{ };
#endif
};

extern air_blast g_air_blast;
