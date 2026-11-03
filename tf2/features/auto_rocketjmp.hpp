#pragma once

#include "link.hpp"

struct rjump_landing_t
{
    vec3  m_point{ };
    vec3  m_normal{ };
    float m_speed{ };
    i32   m_tick{ };
};

class auto_rocketjmp
{
public:
    void run( );
    void run_predicted( );
    void render( );

    bool attacked_this_command( ) const
    {
        return g_cl.m_cmd && this->m_attack_command == g_cl.m_cmd->command_number;
    }

private:
    bool should_run( ) const;
    i32  fire_lead( ) const;
    bool steer_pending( );
    void ground_jump( );
    void blast_jump( );
    bool aim_blast( vec3 &out, i32 &blast_commands ) const;
    void land_shot( bool pogo );
    bool predict_landing( vec3 *path, i32 capacity, rjump_landing_t &out ) const;
    void fire( const vec3 &angles );
    void press( const vec3 &angles );
    void land_outcome( );
    void track_rocket( );

    vec3 m_aim{ };
    i32  m_aim_ticks{ };
    i32  m_attack_command{ -1 };
    i32  m_fire_lead{ -1 };
    i32  m_coast_until{ -1 };
    bool m_key_held{ };
    bool m_jump_pending{ };
    bool m_blast_duck{ };
    bool m_shot_duck{ };
    bool m_shot_stand{ };

    vec3 m_land_point{ };
    vec3 m_land_normal{ };
    bool m_land_live{ };
    i32  m_land_tick{ };
    i32  m_land_total{ };

    vec3   m_draw_point{ };
    vec3   m_draw_normal{ };
    float  m_draw_ticks{ };
    float  m_draw_fade{ };
    double m_draw_time{ };

    // DEBUGGING!
    i32   m_fire_base{ -1 };
    i32   m_pred_touchdown{ };
    i32   m_pred_impact{ };
    float m_pred_land_speed{ };
    float m_air_fall_speed{ };
    i32   m_rocket_entity{ -1 };
    u32   m_rocket_handle{ INVALID_EHANDLE_INDEX };
    bool  m_skip_logged{ };
};

extern auto_rocketjmp g_auto_rocketjmp;
