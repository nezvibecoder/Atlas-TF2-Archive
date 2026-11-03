#include "medbot.hpp"

#include "holdaim_mgr.hpp"
#include "lagcomp.hpp"

ATLAS_NO_DESTROY medbot g_medbot{ };

void medbot::think( )
{
    if ( !this->should_run( ) )
    {
        return;
    }

    this->get_medbot_targets( );

    // No viable healing targets...
    if ( this->m_targets.empty( ) )
    {
        return;
    }

    atl::sort( this->m_targets.begin( ), this->m_targets.end( ), [ & ]( const medbot_target_t &first, const medbot_target_t &second ) -> bool
    {
        return first.m_health_frac < second.m_health_frac;
    } );

    if ( g_cfg.m_medibot.m_prefer_mode == config::MEDBOT_PREFER_FRIENDS )
    {
        atl::sort( this->m_targets.begin( ), this->m_targets.end( ), [ & ]( const medbot_target_t &first, const medbot_target_t &second ) -> bool
        {
            return first.m_is_friend > second.m_is_friend;
        } );
    }

    const auto wish_player_heal{ this->get_best_heal_target( ) };
    if ( !wish_player_heal )
    {
        DBGOUT( "medbot::think: Couldn't find target to heal." );
        return;
    }

    // NOTE: Hoku; This bool can probably get removed at some point in time. I thought about adding auto-reheal when our heal target takes out
    // fists of steel so they could get faster ubercharge build but I decided not to since I think it should be on the medic to do that.
    bool       should_replace_beam{ false };
    const auto cur_healing_target = g_cl.m_weapon->As< C_WeaponMedigun * >( )->m_hHealingTarget( ).Get( );
    if ( cur_healing_target != wish_player_heal )
    {
        should_replace_beam = true;
    }

    DBGOUT( "medbot::think: Wished for player %i to be healed. should_replace_beam: %i", wish_player_heal->entindex( ), should_replace_beam );
    if ( should_replace_beam )
    {
        this->place_beam( wish_player_heal );
    }
}

void medbot::on_level_shutdown( )
{
    this->m_targets.clear( );
    this->m_next_search_time = 0.0f;
    this->m_uber_lock_target = nullptr;
}

bool medbot::should_run( )
{
    if ( !g_cfg.m_medibot.m_enabled )
    {
        return false;
    }

    if ( !gui::bind_active( g_cfg.m_medibot.m_activate_key ) )
    {
        return false;
    }

    if ( !g_cl.m_local->IsAlive( ) ||  // We're dead.
         g_cl.m_local->IsInKart( ) ||  // We're in a kart.
         g_cl.m_local->IsGhost( ) ||   // We're a ghost.
         g_cl.m_local->IsTaunting( ) ) // We're taunting.
    {
        return false;
    }

    if ( !g_cl.m_weapon )
    {
        return false;
    }

    if ( g_cl.m_weapon->GetWeaponID( ) != TF_WEAPON_MEDIGUN )
    {
        return false;
    }

    if ( g_gui.open( ) )
    {
        return false;
    }

    if ( g_cl.m_cmd->buttons & IN_ATTACK )
    {
        return false;
    }

    if ( this->m_next_search_time > g_tf2.m_globals->curtime )
    {
        DBGOUT( "medbot::should_run: Search time condition not met." );
        return false;
    }

    return true;
}

void medbot::get_medbot_targets( )
{
    this->m_targets.clear( );
    for ( u32 i{ 1 }; i < g_tf2.m_engine->GetMaxClients( ); ++i )
    {
        if ( i == g_tf2.m_engine->GetLocalPlayer( ) )
        {
            continue;
        }

        IClientEntity *client_entity{ g_tf2.m_entity_list->GetClientEntity( i ) };
        C_BaseEntity  *base_entity{ client_entity ? client_entity->GetBaseEntity( ) : nullptr };
        ClientClass   *client_class{ base_entity ? base_entity->GetClientClass( ) : nullptr };

        if ( !client_class )
        {
            continue;
        }

        Assert( base_entity != nullptr );

        // Sanity check, would be weird if the entity list suddenly started placing non-player entities in the reserved player slots...
        if ( base_entity->IsDormant( ) || client_class->GetClassID( ) != ETFClassID::CTFPlayer )
        {
            continue;
        }

        const auto player{ base_entity->As< C_TFPlayer * >( ) };
        if ( !player->IsAlive( ) )
        {
            continue;
        }

        if ( player->m_iTeamNum( ) != g_cl.m_local->m_iTeamNum( ) )
        {
            continue;
        }

        this->handle_teammate( player );
    }
}

C_TFPlayer *medbot::get_best_heal_target( )
{
    for ( auto &&tgt : this->m_targets )
    {
        if ( tgt.m_is_healable )
        {
            return tgt.m_player;
        }
    }

    return nullptr;
}

void medbot::handle_teammate( C_TFPlayer *player )
{
    Assert( player != nullptr );
    if ( player->IsCloaked( ) ||
         player->IsGhost( ) ||
         player->IsInKart( ) )
    {
        return;
    }

    player_info_t info{ };
    if ( !g_tf2.m_engine->GetPlayerInfo( player->entindex( ), &info ) )
    {
        return;
    }

    const bool is_friend{ g_cl.is_friend( info.friendsID ) };
    if ( g_cfg.m_medibot.m_prefer_mode == config::MEDBOT_ONLY_FRIENDS && !is_friend )
    {
        return;
    }

    const auto shared_player_data{ player->GetSharedPlayerData( ) };
    if ( !shared_player_data )
    {
        return;
    }

    Assert( shared_player_data != nullptr );

    i32 priority{ this->get_base_class_priority( player ) };
    if ( g_cfg.m_medibot.m_prefer_mode == config::MEDBOT_PREFER_FRIENDS && is_friend ) // Boost their priority a bit since they're a friend.
    {
        priority += 2;
    }

    // Player is ubercharged with multiple healers, we can drop them down the list.
    if ( player->IsUbered( ) && shared_player_data->m_nNumHealers( ) >= 2 )
    {
        priority -= 2;
    }

    const auto health{ player->m_iHealth( ) };
    const auto max_health{ g_cl.m_resource->m_iMaxHealth( )[ player->entindex( ) ] };
    if ( health < max_health )
    {
        priority += 1;
    }

    // The target player is below half health, they should step priority up quite a bit since they're gonna die on the next shot from the enemy.
    if ( const auto half_health{ max_health / 2 };
         health <= half_health )
    {
        priority += this->is_volatile_class( player ) ? 2 : 3;
    }

    if ( priority <= -1 )
    {
        return;
    }

    const auto healable_pos{ is_player_in_healable_pos( player ) };
    const auto health_percent{ ( f32 )health / ( f32 )max_health };

    DBGOUT( "medbot::handle_teammate: tested entindex: %i | priority: %i - healable_pos: %i - hp: %i/%i (%.2f)", player->entindex( ), priority, healable_pos, health, max_health, health_percent );

    auto target{ this->m_targets.emplace_back( ) };

    target->m_player        = player;
    target->m_priority      = priority;
    target->m_is_healable   = healable_pos;
    target->m_health_frac   = health_percent;
    target->m_is_overhealed = health > max_health;
    target->m_is_friend     = is_friend;
}

i32 medbot::get_base_class_priority( C_TFPlayer *player )
{
#if defined( MEDBOT_PRIORITY_TEST )
    switch ( player->m_iClass( ) )
    {
        case TF_CLASS_SCOUT: return 2;
        case TF_CLASS_SOLDIER: return 3;
        case TF_CLASS_PYRO: return 2;
        case TF_CLASS_DEMOMAN: return 3;
        case TF_CLASS_HEAVYWEAPONS: return 4; // Heavies are classed as top of the food chain here.
        case TF_CLASS_ENGINEER: return 1;
        case TF_CLASS_MEDIC: return 1;
        case TF_CLASS_SNIPER: return 2;
        case TF_CLASS_SPY: return 1;
        default: return 0;
    }
#else
    return 3;
#endif // MEDBOT_PRIORITY_TEST
}

bool medbot::is_volatile_class( C_TFPlayer *player )
{
    switch ( player->m_iClass( ) )
    {
        case TF_CLASS_SOLDIER:
        case TF_CLASS_PYRO:
        case TF_CLASS_DEMOMAN:
        case TF_CLASS_HEAVYWEAPONS:
        case TF_CLASS_SNIPER:
        case TF_CLASS_MEDIC: return false;
        default: return true;
    }
}

// Partial rebuild of CWeaponMedigun::MaintainTargetInSlot.
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_weapon_medigun.cpp#L650
bool medbot::is_player_in_healable_pos( C_TFPlayer *player )
{
    const auto weapon_data{ g_cl.m_weapon->GetWeaponInfo( ) };
    if ( !weapon_data )
    {
        return false;
    }

    const auto weapon_info{ weapon_data->GetWeaponData( g_cl.m_weapon->m_iWeaponMode( ) ) };
    auto       range{ weapon_info.m_flRange }; // https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_weapon_medigun.cpp#L489
    const auto heal_target{ g_cl.m_weapon->As< C_WeaponMedigun * >( )->m_hHealingTarget( ) };
    const auto shoot_pos{ g_cl.m_local->Weapon_ShootPosition( ) };
    const auto point{ player->WorldSpaceCenter( ) };

    // Account for GetStickRange if we're already healing this player.
    // https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_weapon_medigun.cpp#L497
    if ( player == heal_target.Get( ) )
    {
        range *= 1.2;
    }

    // https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_weapon_medigun.cpp#L668
    const auto distance{ ( point - shoot_pos ).length( ) };
    if ( distance > range ) // We can't even begin healing this person (they're out of heal range), so ditch it.
    {
        return false;
    }

    Ray_t          ray{ };
    trace_t        trace{ };
    CMedigunFilter filter{ g_cl.m_local };
    ray.Init( shoot_pos, point );

    g_tf2.m_trace->TraceRay( ray, MASK_SHOT, &filter, &trace );

    DBGOUT( "medbot::is_player_in_healable_pos: Tested player %i - Trace ent: %i | fraction: %f", player->entindex( ), trace.m_pEnt ? trace.m_pEnt->entindex( ) : 0, trace.fraction );

    return ( ( trace.fraction == 1.0f ) || ( trace.m_pEnt == player ) ); // Either our trace hit our target player or never hit an enemy/world.
}

void medbot::place_beam( C_TFPlayer *player )
{
    bool can_attack{ g_cl.m_weapon->m_flNextPrimaryAttack( ) < g_tf2.m_globals->curtime && g_cl.m_local->m_flNextAttack( ) < g_tf2.m_globals->curtime };
    if ( !can_attack )
    {
        return;
    }

    const auto model{ player->GetModel( ) };
    if ( !model )
    {
        return;
    }

    const auto hdr{ g_tf2.m_model_info->GetStudiomodel( model ) };
    if ( !hdr )
    {
        return;
    }

    const auto set{ hdr->pHitboxSet( player->m_nHitboxSet( ) ) };
    if ( !set || set->numhitboxes <= 0 )
    {
        return;
    }

    const i32  hitbox_count{ math::smaller( static_cast< i32 >( HITBOX_MAX ), set->numhitboxes ) };
    const i32  index{ player->entindex( ) };
    const auto shoot_pos{ g_cl.m_local->Weapon_ShootPosition( ) };

    matrix3x4 matrix[ MAX_STUDIO_BONES ]{ };
    if ( !player->SetupBones( matrix, MAX_STUDIO_BONES, BONE_USED_BY_HITBOX, player->m_flSimulationTime( ) ) )
    {
        return;
    }

    bool got_pos{ false };
    vec3 out_pos{ };
    for ( i32 i{ HITBOX_PELVIS }; i < hitbox_count; ++i )
    {
        const auto box = set->pHitbox( i );
        if ( !box )
        {
            continue;
        }

        math::vector_transform( vec3{ }, matrix[ box->bone ], out_pos );

        got_pos = g_cl.trace_line( g_cl.m_local, player, shoot_pos, out_pos, MASK_SOLID | CONTENTS_HITBOX );
        if ( got_pos )
        {
            break;
        }
    }

    if ( !got_pos )
    {
        return;
    }

    vec3 va{ };
    math::vector_angles( out_pos - shoot_pos, va );

    const vec3 *held{ g_holdaim_mgr.held( ) };

    g_cl.m_cmd->viewangles = held ? *held : va;
    g_cl.m_cmd->buttons |= IN_ATTACK;
    g_cl.m_cmd->tick_count = TIME_TO_TICKS( player->m_flSimulationTime( ) + lag_compensation::lerp_time( ) );

    if ( g_cl.m_weapon->As< C_WeaponMedigun * >( )->GetMedigunType( ) == MEDIGUN_RESIST )
    {
        this->m_next_search_time = g_tf2.m_globals->curtime + 0.5f;
    } else
    {
        // I'm fast as fuck boyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy
        this->m_next_search_time = g_tf2.m_globals->curtime + TICK_INTERVAL * 2.f;
    }
}
