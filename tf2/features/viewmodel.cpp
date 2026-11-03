#include "viewmodel.hpp"

#include "ragebot.hpp"

#include "math/math.hpp"

ATLAS_NO_DESTROY viewmodel g_viewmodel{ };

static float approach( float current, float target, float delta )
{
    return current + ( target - current ) * math::clamp( delta / 0.14f, 0.0f, 1.0f ); // gives a cool effect
}

static void approach_angles( vec3 &current, const vec3 &target, float step )
{
    current.x += math::normalize_yaw( target.x - current.x ) * step;
    current.y += math::normalize_yaw( target.y - current.y ) * step;
    current.z += math::normalize_yaw( target.z - current.z ) * step;
}

void viewmodel::on_override_view( const CViewSetup *setup )
{
    if ( !setup )
    {
        return;
    }

    this->m_setup    = setup;
    this->m_game_fov = setup->fov;
}

float viewmodel::view_model_fov( float original ) const
{
    /*if ( !this->m_setup )
    {
        return original;
    }

    // by now this holds the camera FOV the engine is going to render with, our override included
    const float view_fov{ this->m_setup->fov };
    if ( g_cfg.m_viewmodel.m_fov_override )
    {
        if ( !g_tf2.default_fov )
        {
            return original;
        }

        return static_cast< float >( g_cfg.m_viewmodel.m_fov ) +
               ( g_tf2.default_fov->GetFloat( ) - view_fov );
    }

    return original + ( view_fov - this->m_game_fov );*/
    return 0.f;
}

void viewmodel::scale_bob( BobState_t *state ) const
{
    const float scale{ static_cast< float >( g_cfg.m_viewmodel.m_bob ) * 0.01f };

    state->m_flVerticalBob *= scale;
    state->m_flLateralBob *= scale;
}

void viewmodel::on_calc_view( vec3 &eye_position, vec3 &eye_angles )
{
    if ( g_cl.m_local && g_cfg.m_misc.m_removals[ config::REMOVAL_VIEWPUNCH ] )
    {
        eye_angles -= g_cl.m_local->m_vecPunchAngle( );
        eye_angles.z = 0.0f;
    }

    if ( !g_cl.m_weapon ) // TODO: Could probably safely remove this...
    {
        return;
    }

    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    const float  delta{ this->m_last_time > 0.0
                            ? math::smaller( static_cast< float >( now - this->m_last_time ), 0.1f )
                            : 0.1f };

    this->m_last_time = now;

    const vec3 view{ eye_angles };

    this->aim( eye_angles, eye_position, delta );

    vec3 wanted{ this->sway( eye_angles, delta ) };

    if ( g_cfg.m_viewmodel.m_enabled )
    {
        wanted += this->offset( delta );
    }

    if ( g_cl.m_vm_flipped )
    {
        eye_angles.y = view.y - math::normalize_yaw( eye_angles.y - view.y );
        eye_angles.z = view.z - math::normalize_yaw( eye_angles.z - view.z );
    }

    vec3 forward{ };
    vec3 right{ };
    vec3 up{ };
    math::angle_vectors( eye_angles, &forward, &right, &up );

    if ( g_cl.m_vm_flipped )
    {
        eye_position += forward * wanted.x + right * -wanted.y + up * wanted.z;
    } else
    {
        eye_position += forward * wanted.x + right * wanted.y + up * wanted.z;
    }
}

vec3 viewmodel::sway( const vec3 &eye_angles, float delta )
{
    const float mix{ math::clamp( delta / 0.1f, 0.0f, 1.0f ) };

    approach_angles( this->m_sway_angle, eye_angles, mix );
    vec3 lagged{ };
    math::angle_vectors( eye_angles - this->m_sway_angle, &lagged );

    const vec3 lag{ vec3{ 1.0f, 0.0f, 0.0f } - lagged };

    return vec3{ lag.x, -lag.y, lag.z } *
           ( static_cast< float >( g_cfg.m_viewmodel.m_sway ) * 0.05f );
}

void viewmodel::aim( vec3 &eye_angles, const vec3 &eye_position, float delta )
{
    vec3       point{ };
    const bool aiming{ g_cfg.m_viewmodel.m_aim && g_ragebot.visual_aim( point ) };

    const float swing{ static_cast< float >( g_cfg.m_viewmodel.m_aim_swing ) * 0.001f };
    const float step{ swing > 0.0f ? math::smaller( delta / swing, 1.0f ) : 1.0f };

    if ( aiming )
    {
        vec3 solved{ };
        math::vector_angles( point - eye_position, solved );

        if ( this->m_aim_mix <= 0.0f )
        {
            this->m_aim_angle = solved;
        } else
        {
            approach_angles( this->m_aim_angle, solved, step );
        }
    }

    this->m_aim_mix = math::approach( this->m_aim_mix, aiming ? 1.0f : 0.0f, step );

    if ( this->m_aim_mix <= 0.0f )
    {
        return;
    }

    approach_angles( eye_angles, this->m_aim_angle, this->m_aim_mix );
}

vec3 viewmodel::offset( float delta )
{
    this->m_offset.x = approach( this->m_offset.x,
                                 static_cast< float >( g_cfg.m_viewmodel.m_forward ), delta );
    this->m_offset.y = approach( this->m_offset.y,
                                 static_cast< float >( g_cfg.m_viewmodel.m_right ), delta );
    this->m_offset.z = approach( this->m_offset.z,
                                 static_cast< float >( g_cfg.m_viewmodel.m_up ), delta );

    return this->m_offset;
}
