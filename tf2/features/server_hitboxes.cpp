#include "server_hitboxes.hpp"

#include "bone_setup.hpp"
#include "entity_cache.hpp"

ATLAS_NO_DESTROY server_hitboxes g_server_hitboxes{ };

void server_hitboxes::paint( ) const
{
    if ( !g_cfg.m_debug.m_server_hitboxes && !g_cfg.m_debug.m_client_hitboxes )
    {
        return;
    }

    if ( !g_tf2.m_engine->IsInGame( ) || !g_cl.m_local )
    {
        return;
    }

    const bool third_person{ g_tf2.m_input->CAM_IsThirdPerson( ) };
    for ( const auto *cached : g_entity_cache.players( ) )
    {
        C_TFPlayer *player{ cached->m_entity->As< C_TFPlayer * >( ) };
        if ( !cached->networked( ) || player->IsDormant( ) || !player->IsAlive( ) ||
             ( player == g_cl.m_local && !third_person ) )
        {
            continue;
        }

        CStudioHdr *studio{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< CStudioHdr * >( player ) };
        if ( !studio || !studio->m_pStudioHdr )
        {
            continue;
        }

        const mstudiohitboxset_t *set{ studio->m_pStudioHdr->pHitboxSet( player->m_nHitboxSet( ) ) };
        if ( g_cfg.m_debug.m_server_hitboxes )
        {
            this->draw_server( player, set );
        }

        if ( g_cfg.m_debug.m_client_hitboxes )
        {
            this->draw_client( player, set );
        }
    }
}

void server_hitboxes::draw_server( C_TFPlayer *player, const mstudiohitboxset_t *set ) const
{
    const edict_t *edict{ g_tf2.m_engine_server->PEntityOfEntIndex( player->entindex( ) ) };
    if ( !edict || edict->m_fStateFlags & FL_EDICT_FREE || !edict->m_pUnk ||
         player->InCond( ETFCond::TF_COND_DISGUISED ) )
    {
        return;
    }

    CBaseAnimating *server_player{ edict->m_pUnk->GetBaseEntity( ) };
    for ( i32 index{ }; index < set->numhitboxes; ++index )
    {
        const mstudiobbox_t *box{ set->pHitbox( index ) };
        matrix3x4            bone_to_world{ };
        server_player->GetBoneTransform( box->bone, bone_to_world );
        g_cl.draw_3d_box( bone_to_world, box->bbmin, box->bbmax, g_cfg.m_debug.m_server_hitbox_color );
    }
}

void server_hitboxes::draw_client( C_TFPlayer *player, const mstudiohitboxset_t *set ) const
{
    matrix3x4 bones[ MAX_STUDIO_BONES ]{ };
    if ( !g_bone_mgr.player_setup( player, bones, MAX_STUDIO_BONES, BONE_USED_BY_HITBOX ) )
    {
        return;
    }

    for ( i32 index{ }; index < set->numhitboxes; ++index )
    {
        const mstudiobbox_t *box{ set->pHitbox( index ) };
        g_cl.draw_3d_box( bones[ box->bone ], box->bbmin, box->bbmax, g_cfg.m_debug.m_client_hitbox_color );
    }
}
