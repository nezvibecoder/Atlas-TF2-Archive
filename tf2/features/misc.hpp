#pragma once

#include "link.hpp"

class misc
{
public:
    void        on_pre_prediction( );
    void        on_prediction( );
    void        on_post_prediction( );
    void        on_override_view( CViewSetup *setup, const vec3 &eye );
    void        on_enable_fog( bool skybox_pass );
    bool        overlay_removed( IMaterial *material ) const;
    bool        is_footstep( const char *name ) const;
    bool        mutes_footstep( const char *name ) const;
    bool        is_weapon_sound( const StartSoundParams_t &params ) const;
    const char *giant_shoot_sound( const char *sound ) const;
    void        noisemaker_spam( );
    void        noisemaker_notify_event( IGameEvent *event );
    bool        is_taunt_exploit_taking_place( );

    bool bhop_held( ) const
    {
        return g_cfg.m_misc.m_bhop && this->m_bhop_jump_held;
    }

private:
    void bhop( );
    void edge_jump( );
    void fan_jump( );
    void fast_stop( );
    void auto_strafe( );
    void taunt_spin( );
    void anti_afk( );
    void perform_taunt_cancel_exploit( );
    void fov( CViewSetup *setup );
    void freecam( CViewSetup *setup, const vec3 &eye );
    void freecam_input( );
    void thirdperson( CViewSetup *setup, const vec3 &eye );
    void advance_thirdperson( bool wanted );
    void restart_minigun_effects( C_TFMinigun *minigun );

    float  m_strafe_side{ };
    float  m_taunt_spin_yaw{ };
    bool   m_bhop_jump_held{ };
    bool   m_edge_grounded{ };
    float  m_third_fraction{ };
    double m_third_time{ };
    vec3   m_freecam_origin{ };
    vec3   m_freecam_angles{ };
    i32    m_freecam_buttons{ };
    bool   m_freecam_active{ };
    f32    m_last_action_time{ };
    f32    m_needs_action_time{ };
    bool   m_should_hit_noisemaker{ false };
    bool   m_is_taunt_exploit_taking_place{ false };
};

extern misc g_misc;
