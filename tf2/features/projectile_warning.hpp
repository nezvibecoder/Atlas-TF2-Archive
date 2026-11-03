#pragma once

#include "link.hpp"

#include "proj_sim.hpp"

enum class warning_kind : u8
{
    none,
    rocket,
    flare,
    arrow,
    sticky,
};

struct warning_threat_t
{
    static constexpr i32 MAX_POINTS{ 200 };

    u32          m_handle{ };
    warning_kind m_kind{ };
    i32          m_deflected{ -1 };
    f32          m_spawn_time{ };
    vec3         m_first_origin{ };
    f32          m_first_time{ };
    i32          m_first_tick{ };
    i32          m_seen{ };
    vec3         m_path[ MAX_POINTS + 1 ]{ };
    i32          m_count{ };
    i32          m_seed_tick{ };
    vec3         m_offset_low{ };
    vec3         m_offset_high{ };
    vec3         m_hull_mins{ };
    vec3         m_hull_maxs{ };
    vec3         m_landing{ };
    vec3         m_normal{ };
    i32          m_landing_tick{ };
    bool         m_landed{ };
    bool         m_resting{ };
    vec3         m_impact{ };
    i32          m_impact_tick{ };
    f32          m_radius{ };
    bool         m_direct{ };
    bool         m_danger{ };
    bool         m_live{ };
};

class projectile_warning
{
public:
    static constexpr i32 MAX_THREATS{ 24 };

    void on_network_update( );
    void paint( );
    void on_level_shutdown( );

private:
    bool              should_run( ) const;
    warning_threat_t *track( u32 handle, warning_kind kind, C_BaseEntity *projectile );
    void              update( warning_threat_t &threat, C_BaseEntity *projectile, ETFClassID class_id );
    bool              follows( warning_threat_t &threat, const vec3 &origin ) const;
    void              seed( warning_threat_t &threat, C_BaseEntity *projectile, const vec3 &origin );
    void              rest( warning_threat_t &threat, C_BaseEntity *projectile ) const;
    void              meet( warning_threat_t &threat ) const;
    bool              setup_flight( const warning_threat_t &threat, C_BaseEntity *projectile, const vec3 &position,
                                    const vec3 &velocity, projectile_setup_t &out ) const;
    vec3              chord_velocity( const warning_threat_t &threat, C_BaseEntity *projectile, f32 span ) const;
    f32               gravity_of( const warning_threat_t &threat, C_BaseEntity *projectile ) const;
    f32               blast_radius( C_BaseEntity *projectile, ETFClassID class_id ) const;
    i32               arm_tick( const warning_threat_t &threat, C_BaseEntity *projectile ) const;
    bool              reaches_us( const vec3 &point, f32 radius ) const;
    void              drop_gone( );

    void draw_threat( const warning_threat_t &threat, C_BaseEntity *projectile, i32 now ) const;
    void draw_trail( const warning_threat_t &threat, const vec3 &from, color_t color, f32 thickness ) const;
    void draw_zone( const warning_threat_t &threat, color_t color, f32 fill ) const;
    void draw_badge( const warning_threat_t &threat, const vec3 &position, color_t color, f32 progress, f32 remaining,
                     const char *label ) const;
    void draw_radar( const vec3 &position, color_t color, f32 progress, const char *label ) const;
    void draw_impact_mark( const warning_threat_t &threat, color_t color ) const;

    warning_threat_t m_threats[ MAX_THREATS ]{ };
    i32              m_count{ };
};

extern projectile_warning g_projectile_warning;
