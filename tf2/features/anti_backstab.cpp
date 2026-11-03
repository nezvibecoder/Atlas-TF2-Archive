#include "anti_backstab.hpp"

#include "melee.hpp"
#include "ragebot.hpp"
#include "splash.hpp"
#include "tickbase.hpp"

ATLAS_NO_DESTROY anti_backstab g_anti_backstab{ };

constexpr f32 KNIFE_SWING_RANGE{ 48.0f };
constexpr f32 KNIFE_SWING_BOUNDS{ 18.0f };

void anti_backstab::think( )
{
    if ( !this->should_run( ) )
    {
        return;
    }

    const i32 batch{ g_tf2.m_cl->chokedcommands + 1 };

    vec3 face{ };
    if ( !this->find_threat( batch, face ) )
    {
        return;
    }

    vec3 angle{ };
    math::vector_angles( face - g_cl.m_local->WorldSpaceCenter( ), angle );
    g_cl.m_cmd->viewangles.y = angle.y;
}

bool anti_backstab::should_run( ) const
{
    if ( !g_cfg.m_anti_hack.m_anti_backstab )
    {
        return false;
    }

    if ( g_cl.m_will_fire ||
         g_melee.aimed_this_command( ) ||
         g_tickbase.shifting( ) ||
         g_cl.m_local->IsInKart( ) ||
         g_cl.m_local->IsTaunting( ) ||
         g_cl.m_local->IsCharging( ) ||
         g_cl.m_local->m_MoveType( ) == MOVETYPE_LADDER )
    {
        return false;
    }

    return true;
}

bool anti_backstab::find_threat( i32 batch, vec3 &face ) const
{
    const vec3 origin{ g_cl.m_local->GetAbsOrigin( ) };
    const vec3 bounds{ KNIFE_SWING_BOUNDS, KNIFE_SWING_BOUNDS, KNIFE_SWING_BOUNDS };
    const vec3 mins{ origin + g_cl.m_local->m_vecMins( ) - bounds };
    const vec3 maxs{ origin + g_cl.m_local->m_vecMaxs( ) + bounds };

    f32 closest{ FLT_MAX };
    for ( i32 i{ 1 }; i <= g_tf2.m_engine->GetMaxClients( ); ++i )
    {
        IClientEntity *client_entity{ g_tf2.m_entity_list->GetClientEntity( i ) };
        C_BaseEntity  *base_entity{ client_entity ? client_entity->GetBaseEntity( ) : nullptr };
        ClientClass   *client_class{ base_entity ? base_entity->GetClientClass( ) : nullptr };

        if ( !client_class || client_class->GetClassID( ) != ETFClassID::CTFPlayer || base_entity->IsDormant( ) )
        {
            continue;
        }

        const auto player{ base_entity->As< C_TFPlayer * >( ) };

        if ( player->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) ||
             !player->IsAlive( ) ||
             player->InCond( TF_COND_STEALTHED ) )
        {
            continue;
        }

        C_BaseEntity *weapon{ player->m_hActiveWeapon( ).Get( ) };
        if ( !weapon || weapon->As< C_TFWeaponBase * >( )->GetWeaponID( ) != TF_WEAPON_KNIFE )
        {
            continue;
        }

        const i32  ticks{ g_ragebot.target_catch_up_ticks( player, 0 ) };
        const vec3 spy_origin{ player->m_vecOrigin( ) };
        const vec3 eye{ splash::player_eye( player, spy_origin, player->m_fFlags( ) ) };
        const f32  gap{ eye.dist_to( math::nearest_on_box( mins, maxs, eye ) ) - KNIFE_SWING_RANGE };
        if ( gap > player->m_flMaxspeed( ) * TICKS_TO_TIME( ticks + batch ) || gap >= closest )
        {
            continue;
        }

        closest = gap;
        face    = spy_origin + ( player->m_vecMins( ) + player->m_vecMaxs( ) ) * 0.5f +
                  player->GetAbsVelocity( ) * ( TICKS_TO_TIME( ticks ) + TICKS_TO_TIME( batch ) * 0.5f );
    }

    return closest != FLT_MAX;
}
