#include "post_process.hpp"

ATLAS_NO_DESTROY post_process g_post_process{ };

ATLAS_FORCE_INLINE void write_convar( ConVar *var, f32 value )
{
    var->m_fValue = value;
    var->m_nValue = static_cast< i32 >( value );
}

void post_process::grade( ) const
{
    const config::post_process_t &cfg{ g_cfg.m_post_process };

    if ( !cfg.m_enabled || g_cl.should_perform_clean_frame( ) || !g_tf2.m_engine->IsInGame( ) )
    {
        return;
    }

    render::grade_t grade{ };
    grade.m_exposure        = static_cast< f32 >( cfg.m_exposure ) / 100.0f;
    grade.m_contrast        = static_cast< f32 >( cfg.m_contrast ) / 100.0f;
    grade.m_saturation      = static_cast< f32 >( cfg.m_saturation ) / 100.0f;
    grade.m_vibrance        = static_cast< f32 >( cfg.m_vibrance ) / 100.0f;
    grade.m_shadows         = static_cast< f32 >( cfg.m_shadows ) / 100.0f;
    grade.m_highlights      = static_cast< f32 >( cfg.m_highlights ) / 100.0f;
    grade.m_temperature     = static_cast< f32 >( cfg.m_temperature ) / 100.0f;
    grade.m_tint            = static_cast< f32 >( cfg.m_tint ) / 100.0f;
    grade.m_vignette        = static_cast< f32 >( cfg.m_vignette ) / 100.0f;
    grade.m_bloom           = static_cast< f32 >( cfg.m_bloom ) / 100.0f;
    grade.m_bloom_threshold = static_cast< f32 >( cfg.m_bloom_threshold ) / 100.0f;

    g_render.grade( grade );
}

void post_process::begin_view( )
{
    const config::post_process_t &cfg{ g_cfg.m_post_process };

    if ( !cfg.m_enabled || !cfg.m_motion_blur )
    {
        return;
    }

    this->m_blur_enabled    = g_tf2.mat_motion_blur_enabled->m_fValue;
    this->m_blur_strength   = g_tf2.mat_motion_blur_strength->m_fValue;
    this->m_blur_overridden = true;

    write_convar( g_tf2.mat_motion_blur_enabled, 1.0f );
    write_convar( g_tf2.mat_motion_blur_strength, static_cast< f32 >( cfg.m_motion_blur ) / 100.0f );
}

void post_process::end_view( )
{
    if ( !this->m_blur_overridden )
    {
        return;
    }

    write_convar( g_tf2.mat_motion_blur_enabled, this->m_blur_enabled );
    write_convar( g_tf2.mat_motion_blur_strength, this->m_blur_strength );
    this->m_blur_overridden = false;
}
