#pragma once

#include "sdk.hpp"

constexpr f32 DAMAGE_RANGE{ 0.5f };       // tf_damage_range
constexpr f32 DAMAGE_SPREAD{ 0.1f };
constexpr f32 OPTIMAL_DISTANCE{ 512.0f };

ATLAS_FORCE_INLINE f32 distance_spread( i32 weapon_id )
{
    switch ( weapon_id )
    {
        case TF_WEAPON_SHOTGUN_PRIMARY:
        case TF_WEAPON_SHOTGUN_SOLDIER:
        case TF_WEAPON_SHOTGUN_HWG:
        case TF_WEAPON_SHOTGUN_PYRO:
        case TF_WEAPON_SHOTGUN_BUILDING_RESCUE:
        case TF_WEAPON_SENTRY_REVENGE:
        case TF_WEAPON_SCATTERGUN:
        case TF_WEAPON_SODA_POPPER:
        case TF_WEAPON_PEP_BRAWLER_BLASTER:
        case TF_WEAPON_HANDGUN_SCOUT_PRIMARY:
        case TF_WEAPON_HANDGUN_SCOUT_SECONDARY:
        case TF_WEAPON_MINIGUN:
        case TF_WEAPON_SMG:
        case TF_WEAPON_CHARGED_SMG:
        case TF_WEAPON_PISTOL:
        case TF_WEAPON_PISTOL_SCOUT:
        case TF_WEAPON_REVOLVER:
        case TF_WEAPON_ROCKETLAUNCHER:
        case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
        case TF_WEAPON_PARTICLE_CANNON: return 1.0f;
        case TF_WEAPON_PIPEBOMBLAUNCHER: return 0.2f;
        default: return 0.0f;
    }
}

ATLAS_FORCE_INLINE f32 short_range_share( i32 weapon_id )
{
    switch ( weapon_id )
    {
        case TF_WEAPON_ROCKETLAUNCHER:
        case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
        case TF_WEAPON_PARTICLE_CANNON: return 0.5f;
        case TF_WEAPON_SCATTERGUN:
        case TF_WEAPON_SODA_POPPER:
        case TF_WEAPON_PEP_BRAWLER_BLASTER: return 1.5f;
        default: return 1.0f;
    }
}

ATLAS_FORCE_INLINE f32 distance_scale( i32 weapon_id, f32 spread, f32 distance, bool minicrit )
{
    if ( spread <= 0.0f )
    {
        return 1.0f;
    }

    const f32 center{ math::remap_clamped( distance / OPTIMAL_DISTANCE, 0.0f, 2.0f, 1.0f, 0.0f ) };
    const f32 value{ math::bigger( center - DAMAGE_SPREAD, 0.0f ) + DAMAGE_SPREAD };

    f32 range{ DAMAGE_RANGE * spread };
    if ( value > 0.5f )
    {
        range *= short_range_share( weapon_id );
    }

    const f32 variance{ range * ( 2.0f * math::smooth_step( value ) - 1.0f ) };
    if ( minicrit && variance < 0.0f )
    {
        return 1.0f;
    }

    return 1.0f + variance;
}
