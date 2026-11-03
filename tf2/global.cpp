#include "global.hpp"

#include "features/anims.hpp"
#include "features/chams.hpp"
#include "features/entity_cache.hpp"
#include "features/esp.hpp"
#include "features/glow.hpp"
#include "features/melee.hpp"
#include "features/networking.hpp"
#include "features/party_network.hpp"
#include "features/player_data.hpp"
#include "features/ragebot.hpp"
#include "features/skin_share.hpp"
#include "features/streamer_mode.hpp"
#include "features/tickbase.hpp"
#include "features/watchdog.hpp"

#include "steam.hpp"

ATLAS_NO_DESTROY tf2     g_tf2{ };
ATLAS_NO_DESTROY client  g_cl{ };
ATLAS_NO_DESTROY api     g_api{ };
ATLAS_NO_DESTROY netvars g_netvars{ };
ATLAS_NO_DESTROY hooks   g_hooks{ };

void tf2::random_seed( i32 seed )
{
    this->m_RandomSeed_fn.call_as< void >( seed );
}

float tf2::random_float( float minimum, float maximum )
{
    return this->m_RandomFloat_fn.call_as< float >( minimum, maximum );
}

i32 tf2::random_int( i32 minimum, i32 maximum )
{
    return this->m_RandomInt_fn.call_as< i32 >( minimum, maximum );
}

float client::attrib_hook( const C_BaseEntity *entity, float value, const char *hook,
                           bool global_const_string ) const
{
    if ( !entity )
    {
        return value;
    }

    return g_tf2.m_CAttributeManager_AttribHookValue
        .call_as< float, float, const char *, const C_BaseEntity *, void *, bool >(
            value, hook, entity, nullptr, global_const_string );
}

vec3 client::projectile_fire_offset( C_TFWeaponBase *weapon, vec3 offset ) const
{
    if ( g_tf2.cl_flipviewmodels->GetInt( ) )
    {
        offset.y = -offset.y;
    }

    if ( static_cast< i32 >( this->attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "centerfire_projectile" ).get( ) ) ) == 1 )
    {
        offset.y = 0.0f;
    }

    return offset;
}

vec3 client::projectile_fire_origin( C_TFPlayer *player, C_TFWeaponBase *weapon, const vec3 &angles, vec3 offset ) const
{
    offset = this->projectile_fire_offset( weapon, offset );

    vec3 forward{ }, right{ }, up{ };
    math::angle_vectors( angles, &forward, &right, &up );

    return player->Weapon_ShootPosition( ) + forward * offset.x + right * offset.y + up * offset.z;
}

void client::projectile_fire_setup( C_TFPlayer *player, C_TFWeaponBase *weapon, const vec3 &angles,
                                    vec3 offset, bool hit_teammates, vec3 &out_position,
                                    vec3 &out_angles, const projectile_target_t *target )
{
    vec3 forward{ };
    math::angle_vectors( angles, &forward );

    const vec3 shoot_position{ player->Weapon_ShootPosition( ) };
    const vec3 end_position{ shoot_position + forward * PROJECTILE_AIM_DISTANCE };

    trace_t                        trace{ };
    CTraceFilterProjectileRedirect filter{ player, hit_teammates ? 0 : player->m_iTeamNum( ) };
    Ray_t                          ray{ };

    ray.Init( shoot_position, end_position );
    filter.m_pSkipTarget = target ? target->m_entity : nullptr;
    g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );

    if ( target )
    {
        const vec3 origin{ target->m_track[ 0 ] };
        const f32  box{ math::segment_box_fraction( shoot_position, end_position, origin + target->m_mins, origin + target->m_maxs ) };
        if ( box < trace.fraction )
        {
            trace.fraction = box;
            trace.endpos   = shoot_position + forward * ( PROJECTILE_AIM_DISTANCE * box );
        }
    }
    out_position = this->projectile_fire_origin( player, weapon, angles, offset );

    const vec3 aim_at{ trace.fraction > PROJECTILE_AIM_FRACTION ? trace.endpos : end_position };
    math::vector_angles( aim_at - out_position, out_angles );

    if ( out_angles.x < 0.0f )
    {
        out_angles.x += 360.0f;
    }

    if ( out_angles.y < 0.0f )
    {
        out_angles.y += 360.0f;
    }
}

bool client::projectile_grenade_setup( C_TFPlayer *player, C_TFWeaponBase *weapon, const vec3 &angles,
                                       vec3 &out_position )
{
    vec3 forward{ }, right{ }, up{ };
    math::angle_vectors( angles, &forward, &right, &up );

    const float side{ this->m_vm_flipped ? -8.0f : 8.0f };

    const vec3 shoot_position{ player->Weapon_ShootPosition( ) };
    const vec3 muzzle{ shoot_position + forward * 16.0f + right * side + up * -6.0f };

    trace_t                       trace{ };
    CTraceFilterWorldAndPropsOnly filter{ };
    Ray_t                         ray{ };

    ray.Init( shoot_position, muzzle, vec3( -8.0f, -8.0f, -8.0f ), vec3( 8.0f, 8.0f, 8.0f ) );
    g_tf2.m_trace->TraceRay( ray, MASK_SOLID_BRUSHONLY, &filter, &trace );

    out_position = trace.endpos;

    return !trace.startsolid;
}

ATLAS_FORCE_INLINE void movement_axes( const vec3 &angles, vec2 &forward, vec2 &right )
{
    vec3 forward_3d{ };
    vec3 right_3d{ };
    math::angle_vectors( angles, &forward_3d, &right_3d );

    const f32 forward_length{ math::sqrt( forward_3d.x * forward_3d.x + forward_3d.y * forward_3d.y ) };
    const f32 right_length{ math::sqrt( right_3d.x * right_3d.x + right_3d.y * right_3d.y ) };

    forward = forward_length > 0.0f ? vec2{ forward_3d.x / forward_length, forward_3d.y / forward_length } : vec2{ };
    right   = right_length > 0.0f ? vec2{ right_3d.x / right_length, right_3d.y / right_length } : vec2{ };
}

void client::fix_movement( CUserCmd *cmd, const vec3 &original_angles )
{
    vec2 forward{ };
    vec2 right{ };
    vec2 sent_forward{ };
    vec2 sent_right{ };
    movement_axes( original_angles, forward, right );
    movement_axes( cmd->viewangles, sent_forward, sent_right );

    const f32 wish_x{ forward.x * cmd->forwardmove + right.x * cmd->sidemove };
    const f32 wish_y{ forward.y * cmd->forwardmove + right.y * cmd->sidemove };

    const f32 determinant{ sent_forward.x * sent_right.y - sent_forward.y * sent_right.x };
    if ( determinant != 0.0f )
    {
        cmd->forwardmove = ( wish_x * sent_right.y - wish_y * sent_right.x ) / determinant;
        cmd->sidemove    = ( sent_forward.x * wish_y - sent_forward.y * wish_x ) / determinant;
        return;
    }

    cmd->forwardmove = wish_x * sent_forward.x + wish_y * sent_forward.y;
    cmd->sidemove    = wish_x * sent_right.x + wish_y * sent_right.y;
}

bool client::has_live_weapon_command( ) const
{
    return this->m_cmd && this->m_local && this->m_weapon &&
           this->m_local->IsAlive( ) && this->m_cmd->weaponselect == 0 &&
           this->m_local->m_hActiveWeapon( ).Get( ) == this->m_weapon;
}

bool client::weapon_attack_allowed( f32 time ) const
{
    return !this->m_weapon->m_bLowered( ) && !this->m_local->IsInKart( ) &&
           !this->m_local->IsGhost( ) && !this->m_local->IsTaunting( ) &&
           !this->m_local->IsBonked( ) && !this->m_local->InCond( TF_COND_STEALTHED ) &&
           !this->m_local->IsFeigned( ) && this->m_local->m_flNextAttack( ) <= time &&
           ( this->m_local->m_flStealthNoAttackExpire( ) <= time ||
             this->m_local->InCond( TF_COND_STEALTHED_USER_BUFF ) ) &&
           this->attrib_hook( this->m_local, 0.0f, ATLAS_XORSTR( "no_attack" ).get( ) ) == 0.0f;
}

bool client::weapon_fired( f32 previous_last_fire ) const
{
    return this->m_weapon->m_flLastFireTime( ) != previous_last_fire;
}

bool client::weapon_charge_pending( ) const
{
    if ( !this->m_weapon )
    {
        return false;
    }

    switch ( this->m_weapon->GetWeaponID( ) )
    {
        case TF_WEAPON_COMPOUND_BOW:
        case TF_WEAPON_PIPEBOMBLAUNCHER: return this->m_weapon->As< C_TFPipebombLauncher * >( )->m_flChargeBeginTime( ) > 0.0f;
        case TF_WEAPON_CANNON: return this->m_weapon->As< C_TFGrenadeLauncher * >( )->m_flDetonateTime( ) > 0.0f;
        default: return false;
    }
}

bool client::flame_ball_ready( C_TFWeaponBase *weapon ) const
{
    if ( this->m_local->m_nWaterLevel( ) == 3 ) // WL_Eyes
    {
        return false;
    }

    const f32 rate{ this->attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "item_meter_charge_rate" ).get( ) ) *
                    this->attrib_hook( weapon, 1.0f, ATLAS_XORSTR( "mult_item_meter_charge_rate" ).get( ) ) };
    const f32 refill{ rate != 0.0f ? weapon->As< C_TFWeaponFlameBall * >( )->m_flRechargeScale( ) * TICK_INTERVAL * 100.0f / rate : 0.0f };

    return this->m_local->m_flItemChargeMeter( ) + refill >= 100.0f;
}

bool client::trace_line( C_BaseEntity *skip, C_BaseEntity *target, const vec3 &from, const vec3 &to, u32 mask )
{
    trace_t             trace{ };
    CTraceFilterHitscan filter{ };
    Ray_t               ray{ };

    filter.m_pSkip       = skip;
    filter.m_pSkipTarget = target;

    ray.Init( from, to );

    g_tf2.m_trace->TraceRay( ray, mask, &filter, &trace );

    return trace.fraction == 1.f;
}

void client::trace_hull( const vec3 &start, const vec3 &end, const vec3 &mins, const vec3 &maxs, trace_t &trace )
{
    Ray_t ray{ };
    ray.Init( start, end, mins, maxs );

    CTraceFilterWorldOnly filter{ };
    g_tf2.m_trace->TraceRay( ray, MASK_PLAYERSOLID_BRUSHONLY, &filter, &trace );
}

void client::update_w2s( )
{
    CViewSetup view{ };
    if ( !g_tf2.m_client->GetPlayerView( view ) )
    {
        return;
    }

    this->m_view_fov = view.fov;

    VMatrix world_to_view{ };
    VMatrix view_to_projection{ };
    VMatrix world_to_pixels{ };
    g_tf2.m_render_view->GetMatricesForView( view, &world_to_view, &view_to_projection, &this->m_w2s, &world_to_pixels );
}

bool client::world_to_screen( const vec3 &world, vec2 &screen ) const
{
    const auto &m = this->m_w2s;

    const float w{ m[ 3 ][ 0 ] * world.x + m[ 3 ][ 1 ] * world.y + m[ 3 ][ 2 ] * world.z + m[ 3 ][ 3 ] };
    if ( w <= 0.001f )
    {
        return false;
    }

    const float cx{ m[ 0 ][ 0 ] * world.x + m[ 0 ][ 1 ] * world.y + m[ 0 ][ 2 ] * world.z + m[ 0 ][ 3 ] };
    const float cy{ m[ 1 ][ 0 ] * world.x + m[ 1 ][ 1 ] * world.y + m[ 1 ][ 2 ] * world.z + m[ 1 ][ 3 ] };

    screen.x = 0.5f * ( 1.f + cx / w ) * static_cast< float >( this->m_screen_width );
    screen.y = 0.5f * ( 1.f - cy / w ) * static_cast< float >( this->m_screen_height );

    return true;
}

// an endpoint behind the near plane folds across the screen when projected, so the segment is cut
// at the plane and only the part in front of it is handed back
bool client::clip_segment_to_screen( const vec3 &start, const vec3 &end, vec2 &screen_start,
                                     vec2 &screen_end ) const
{
    constexpr float NEAR_W{ 0.01f };

    const auto &m = this->m_w2s;

    const auto projected_w{ [ & ]( const vec3 &point )
    {
        return m[ 3 ][ 0 ] * point.x + m[ 3 ][ 1 ] * point.y + m[ 3 ][ 2 ] * point.z + m[ 3 ][ 3 ];
    } };

    const float start_w{ projected_w( start ) };
    const float end_w{ projected_w( end ) };

    if ( start_w < NEAR_W && end_w < NEAR_W )
    {
        return false;
    }

    vec3 visible_start{ start };
    vec3 visible_end{ end };

    if ( start_w < NEAR_W )
    {
        visible_start = start + ( end - start ) * ( ( NEAR_W - start_w ) / ( end_w - start_w ) );
    } else if ( end_w < NEAR_W )
    {
        visible_end = end + ( start - end ) * ( ( NEAR_W - end_w ) / ( start_w - end_w ) );
    }

    return this->world_to_screen( visible_start, screen_start ) &&
           this->world_to_screen( visible_end, screen_end );
}

bool client::is_on_screen( const rect_t &bounds ) const
{
    constexpr i32 margin{ 64 };

    const i32 width{ this->m_screen_width };
    const i32 height{ this->m_screen_height };

    return bounds.right >= -margin && bounds.left <= width + margin && bounds.bottom >= -margin && bounds.top <= height + margin;
}

bool client::is_on_screen( C_BaseEntity *entity ) const
{
    const i32 width{ this->m_screen_width };
    const i32 height{ this->m_screen_height };
    if ( width <= 0 || height <= 0 )
    {
        return true;
    }

    vec3 mins{ };
    vec3 maxs{ };
    entity->GetRenderBounds( mins, maxs );

    const float ( *transform )[ 4 ]{ entity->RenderableToWorldTransform( ) };

    const vec3 corners[ 8 ]{
        { mins.x, mins.y, mins.z },
        { mins.x, mins.y, maxs.z },
        { mins.x, maxs.y, mins.z },
        { mins.x, maxs.y, maxs.z },
        { maxs.x, mins.y, mins.z },
        { maxs.x, mins.y, maxs.z },
        { maxs.x, maxs.y, mins.z },
        { maxs.x, maxs.y, maxs.z },
    };

    const float horizontal{ 1.0f + 130.0f / static_cast< float >( width ) };
    const float vertical{ 1.0f + 130.0f / static_cast< float >( height ) };
    const auto &m{ this->m_w2s };
    u32         outside{ 0x1Fu };

    for ( const vec3 &corner : corners )
    {
        const vec3 world{
            corner.x * transform[ 0 ][ 0 ] + corner.y * transform[ 0 ][ 1 ] +
                corner.z * transform[ 0 ][ 2 ] + transform[ 0 ][ 3 ],
            corner.x * transform[ 1 ][ 0 ] + corner.y * transform[ 1 ][ 1 ] +
                corner.z * transform[ 1 ][ 2 ] + transform[ 1 ][ 3 ],
            corner.x * transform[ 2 ][ 0 ] + corner.y * transform[ 2 ][ 1 ] +
                corner.z * transform[ 2 ][ 2 ] + transform[ 2 ][ 3 ],
        };

        const float w{ m[ 3 ][ 0 ] * world.x + m[ 3 ][ 1 ] * world.y + m[ 3 ][ 2 ] * world.z + m[ 3 ][ 3 ] };
        const float x{ m[ 0 ][ 0 ] * world.x + m[ 0 ][ 1 ] * world.y + m[ 0 ][ 2 ] * world.z + m[ 0 ][ 3 ] };
        const float y{ m[ 1 ][ 0 ] * world.x + m[ 1 ][ 1 ] * world.y + m[ 1 ][ 2 ] * world.z + m[ 1 ][ 3 ] };

        u32 planes{ };
        if ( w <= 0.001f )
            planes |= 0x01u;
        if ( x < -horizontal * w )
            planes |= 0x02u;
        if ( x > horizontal * w )
            planes |= 0x04u;
        if ( y < -vertical * w )
            planes |= 0x08u;
        if ( y > vertical * w )
            planes |= 0x10u;

        outside &= planes;
        if ( !outside )
        {
            return true;
        }
    }
    return false;
}

usize client::pickup_kind( C_BaseEntity *entity, ETFClassID class_id ) const
{
    switch ( class_id )
    {
        case ETFClassID::CCaptureFlag: return config::PICKUP_INTEL;
        case ETFClassID::CBaseAnimating:
        case ETFClassID::CTFAmmoPack: break;
        default: return config::PICKUP_COUNT;
    }

    // map pickups derive from CItem, which declares no server class (server/items.h), so they
    // arrive as plain CBaseAnimating and only the model tells them apart from anything else
    const model_t *model{ entity->GetModel( ) };
    const char    *path{ model ? g_tf2.m_model_info->GetModelName( model ) : nullptr };

    if ( !path )
    {
        return config::PICKUP_COUNT;
    }

    const string_view name{ path };

    // a dropped lunchbox is an item_healthkit wearing a plate model (tf_weapon_lunchbox.cpp)
    if ( name.contains( ATLAS_XORSTR( "medkit" ).get( ) ) || name.contains( ATLAS_XORSTR( "/plate" ).get( ) ) )
    {
        return config::PICKUP_HEALTH;
    }

    if ( name.contains( ATLAS_XORSTR( "ammopack" ).get( ) ) )
    {
        return config::PICKUP_AMMO;
    }

    return config::PICKUP_COUNT;
}

void client::on_directx_frame( void *device )
{
    render::d3d9::D3DVIEWPORT9 vp{ };
    util::call_virtual< i32 ( * )( void *, render::d3d9::D3DVIEWPORT9 * ) >( device, ( u16 )render::d3d9::DeviceMethod::GetViewport )( device, &vp );

    this->m_screen_width  = ( i32 )vp.width;
    this->m_screen_height = ( i32 )vp.height;
}

void client::update_local_data( )
{
    IClientEntity *local_entity{ g_tf2.m_entity_list->GetClientEntity( g_tf2.m_engine->GetLocalPlayer( ) ) };
    C_TFPlayer    *local{ local_entity ? local_entity->As< C_TFPlayer * >( ) : nullptr };

    g_cl.m_local  = nullptr;
    g_cl.m_weapon = nullptr;

    if ( local )
    {
        g_cl.m_local = local;

        if ( C_BaseEntity * active_weapon{ g_cl.m_local->m_hActiveWeapon( ).Get( ) } )
        {
            g_cl.m_weapon = active_weapon->As< C_TFWeaponBase * >( );
        }
    }

    g_cl.m_resource = g_entity_cache.player_resource( );
}

void client::on_entity_deleted( C_BaseEntity *entity, u32 handle )
{
    g_esp.on_entity_deleted( entity, handle );
    g_glow.on_entity_deleted( entity );
    g_chams.on_entity_deleted( handle );
    g_ragebot.on_entity_deleted( entity, handle );
    g_anims.on_entity_deleted( handle );

    if ( entity == this->m_local )
    {
        this->m_local  = nullptr;
        this->m_weapon = nullptr;
    }
    if ( entity == this->m_weapon )
    {
        this->m_weapon = nullptr;
    }
    if ( entity == this->m_resource )
    {
        this->m_resource = nullptr;
    }
}

bool client::is_delayed_throw_weapon( C_TFWeaponBase *weapon ) const
{
    if ( !weapon )
    {
        return false;
    }

    switch ( weapon->GetWeaponID( ) )
    {
        case TF_WEAPON_JAR:
        case TF_WEAPON_JAR_MILK:
        case TF_WEAPON_JAR_GAS:
        case TF_WEAPON_CLEAVER: return true;
        default: return false;
    }
}

bool client::weapon_throw_pending( ) const
{
    return this->m_throw_command >= 0 && this->has_live_weapon_command( ) &&
           this->m_throw_weapon == static_cast< u32 >( this->m_weapon->GetRefEHandle( ).ToInt( ) );
}

void client::clear_throw( )
{
    this->m_throw_command = -1;
    this->m_throw_tick    = -1;
    this->m_throw_weapon  = ~0u;
    this->m_throw_angle   = { };
}

i32 client::throw_spawn_tick( ) const
{
    if ( this->weapon_throw_pending( ) )
    {
        return this->m_throw_tick;
    }

    const i32                           delay{ g_networking.send_delay( ).ticks( ) };
    const command_tickbase_projection_t press{ g_tickbase.project_current_command_tickbase( delay ) };
    const i32                           tick{ press.valid( ) ? press.m_player_tickbase : this->m_local->m_nTickBase( ) };

    // CTFJar::FireJar schedules a context think; it cannot run in the frame that arms it.
    return math::bigger( TIME_TO_TICKS( TICKS_TO_TIME( tick ) + 0.1f ), press.m_server.m_projected_server_tick + 1 );
}

void client::latch_throw( f32 previous_last_fire )
{
    if ( !this->is_delayed_throw_weapon( this->m_weapon ) || !this->weapon_fired( previous_last_fire ) )
    {
        return;
    }

    this->clear_throw( );
    this->m_throw_tick    = this->throw_spawn_tick( );
    this->m_throw_command = g_networking.sequence_number( );
    this->m_throw_weapon  = static_cast< u32 >( this->m_weapon->GetRefEHandle( ).ToInt( ) );
    this->m_throw_angle   = this->m_cmd->viewangles;
}

void client::reconcile_throw_batch(
    i32 first_command,
    i32 simulation_ticks,
    i32 batch_player_tickbase,
    i32 server_tick )
{
    if ( this->m_throw_command < first_command || this->m_throw_command >= first_command + simulation_ticks )
    {
        return;
    }

    const i32 press_tick{ batch_player_tickbase + this->m_throw_command - first_command };
    this->m_throw_tick = math::bigger( TIME_TO_TICKS( TICKS_TO_TIME( press_tick ) + 0.1f ), server_tick + 1 );
}

void client::update_throw( )
{
    if ( this->m_throw_command < 0 )
    {
        return;
    }

    if ( !this->weapon_throw_pending( ) )
    {
        this->clear_throw( );
        return;
    }

    // Usercmds can be batched. A local tickbase deadline can release the angle before TossJarThink.
    if ( g_tf2.m_cl->command_ack >= this->m_throw_command )
    {
        const i32 scheduled{ TIME_TO_TICKS( this->m_weapon->m_flLastFireTime( ) + 0.1f ) };
        if ( g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick > math::bigger( this->m_throw_tick, scheduled ) )
        {
            this->clear_throw( );
        }
    }
}

void client::apply_throw_angle( )
{
    if ( this->weapon_throw_pending( ) )
    {
        this->m_cmd->viewangles = this->m_throw_angle;
    }
}

void client::manage_psilent( )
{
    if ( this->weapon_throw_pending( ) )
    {
        this->m_send_packet     = true;
        this->m_psilent_choked  = false;
        this->m_psilent_release = false;
        return;
    }

    if ( this->m_psilent_choked &&
         g_tf2.m_cl->chokedcommands + 2 <= MAX_NEW_COMMANDS + MAX_BACKUP_COMMANDS )
    {
        this->m_send_packet     = false;
        this->m_psilent_release = true;
        return;
    }

    this->m_psilent_choked = false;
    if ( this->m_psilent_release )
    {
        this->m_send_packet = true;
    }
}

bool client::chat_open( ) const
{
    const MessageMode_t mode{ g_tf2.m_client_mode->m_pChatElement->m_nMessageMode };

    Assert( mode >= MM_NONE && mode <= MM_SAY_PARTY ); // m_nMessageMode

    return mode != MM_NONE;
}

bool client::game_owns_keyboard( ) const
{
    return this->chat_open( ) || g_tf2.m_vgui->IsGameUIVisible( );
}

void client::sync_chat_cursor( )
{
    const bool open{ this->chat_open( ) };

    if ( open == this->m_chat_cursor_held )
    {
        return;
    }

    this->m_chat_cursor_held = open;
    g_tf2.m_surface->SetCursorAlwaysVisible( open );
}

bool client::should_perform_clean_frame( )
{
    return g_cfg.m_misc.m_clean_screenshots && g_tf2.m_engine->IsTakingScreenshot( );
}

bool client::is_friend( u32 account )
{
    return g_cfg.player_tag( account ) == config::PLAYER_TAG_FRIEND || g_steam.is_friend( account );
}

bool client::is_friend( C_TFPlayer *player )
{
    const i32     index{ player->entindex( ) };
    player_info_t info{ };

    if ( index < 1 || index >= MAX_TRACKED_PLAYERS || !g_tf2.m_engine->GetPlayerInfo( index, &info ) || !info.friendsID )
    {
        return false;
    }

    return this->is_friend( info.friendsID );
}

bool client::is_atlas_user( u32 account ) const
{
    return g_skin_share.is_peer( account ) || g_party_network.has_peer( account );
}

bool client::is_atlas_user( C_TFPlayer *player ) const
{
    player_info_t info{ };
    return g_tf2.m_engine->GetPlayerInfo( player->entindex( ), &info ) && this->is_atlas_user( info.friendsID );
}

bool client::is_party_member( u32 account ) const
{
    for ( usize index{ }; index < this->m_party_member_count; ++index )
    {
        if ( this->m_party_members[ index ] == account )
        {
            return true;
        }
    }

    return false;
}

bool client::is_ignored_player( u32 account )
{
    const bool *ignores{ g_cfg.m_ragebot.m_ignores };

    return ( ignores[ config::IGNORE_FRIENDS ] && this->is_friend( account ) ) ||
           ( ignores[ config::IGNORE_ATLAS_USERS ] && this->is_atlas_user( account ) ) ||
           ( ignores[ config::IGNORE_PARTY ] && this->is_party_member( account ) ) ||
           g_cfg.player_tag( account ) == config::PLAYER_TAG_IGNORED;
}

void client::refresh_party( )
{
    this->m_party_member_count = 0;

    C_TFParty *party{ TFPartyClient( )->m_pActiveParty };
    if ( !party )
    {
        return;
    }

    const i32 count{ math::smaller( party->m_PlayerGroup.GetNumMembers( ), static_cast< i32 >( MAX_PARTY_SIZE ) ) };
    for ( i32 index{ }; index < count; ++index )
    {
        const u32 account{ party->m_PlayerGroup.GetMemberAccount( index ) };
        if ( account && account != g_steam.local_account_id( ) )
        {
            this->m_party_members[ this->m_party_member_count++ ] = account;
        }
    }
}

void client::refresh_lobby_parties( )
{
    this->m_lobby_party_count = 0;

    CTFGSLobby *lobby{ TFGCClientSystem( )->GetLobby( ) };
    if ( !lobby )
    {
        return;
    }

    struct member_t
    {
        u32 m_account{ };
        u64 m_party{ };
    };

    member_t  members[ MAX_TRACKED_PLAYERS ]{ };
    usize     count{ };
    const u32 local_account{ g_steam.local_account_id( ) };
    const i32 total{ math::smaller( lobby->GetNumMembers( ), MAX_TRACKED_PLAYERS ) };
    for ( i32 index{ }; index < total; ++index )
    {
        const CTFLobbyPlayerProto *proto{ lobby->GetMemberDetails( index ) };
        const u32                  account{ static_cast< u32 >( proto->m_id ) };
        if ( !proto->m_original_party_id ||     // no party on record
             account == local_account ||        // ourselves
             this->is_party_member( account ) ) // our party is index 1 already
        {
            continue;
        }

        usize slot{ count++ };
        for ( ; slot && members[ slot - 1 ].m_party > proto->m_original_party_id; --slot )
        {
            members[ slot ] = members[ slot - 1 ];
        }
        members[ slot ] = member_t{ account, proto->m_original_party_id };
    }

    u8 party{ 1 };
    for ( usize first{ }; first < count; )
    {
        usize last{ first + 1 };
        while ( last < count && members[ last ].m_party == members[ first ].m_party )
        {
            ++last;
        }

        if ( last - first > 1 )
        {
            ++party;
            for ( usize index{ first }; index < last; ++index )
            {
                this->m_lobby_parties[ this->m_lobby_party_count++ ] = lobby_party_t{ members[ index ].m_account, party };
            }
        }

        first = last;
    }
}

u8 client::party_index( u32 account ) const
{
    if ( this->is_party_member( account ) )
    {
        return 1;
    }

    for ( usize index{ }; index < this->m_lobby_party_count; ++index )
    {
        if ( this->m_lobby_parties[ index ].m_account == account )
        {
            return this->m_lobby_parties[ index ].m_party;
        }
    }

    return 0;
}

color_t client::party_color( u8 party )
{
    if ( party == 1 )
    {
        return color_t{ 90, 220, 150 };
    }

    switch ( ( party - 2 ) % 5 )
    {
        case 0: return color_t{ 255, 170, 60 };
        case 1: return color_t{ 90, 170, 255 };
        case 2: return color_t{ 235, 110, 235 };
        case 3: return color_t{ 255, 230, 90 };
        default: return color_t{ 120, 230, 230 };
    }
}

const char *client::get_player_name( IGameResources *resources, i32 idx )
{
    const bool    hidden{ g_streamer_mode.should_hide_player( idx ) };
    player_info_t info{ };
    const u32     account{ g_tf2.m_engine->GetPlayerInfo( idx, &info ) ? info.friendsID : 0 };
    if ( const char *alias{ hidden ? nullptr : g_cfg.player_alias( account ) } )
    {
        return alias;
    }

    if ( const char *name{ g_watchdog.known_cheater_name( idx, account, hidden ) } )
    {
        return name;
    }

    if ( !hidden )
    {
        return g_hooks.m_get_player_name.thiscall< const char * >( resources, idx );
    }

    return g_streamer_mode.get_ideal_player_name( idx );
}

const char *client::get_player_name( i32 idx )
{
    IGameResources *resources{ GameResources( ) };
    if ( !resources )
    {
        return ATLAS_XORSTR_STATIC( "Player" );
    }

    return this->get_player_name( resources, idx );
}

bool client::should_draw_teammate( C_TFPlayer *player )
{
    if ( !g_cfg.m_esp.m_draw_teammates_conditionally || !this->m_local )
    {
        return true;
    }

    if ( !this->m_local->IsAlive( ) ||
         this->m_local->IsGhost( ) ||
         this->m_local->IsInKart( ) ||
         this->m_local->m_iClass( ) != TF_CLASS_MEDIC )
    {
        return false;
    }

#if defined( ATLAS_DEV )
    const auto team{ player->IsDormant( ) ? g_player_data.team( player->entindex( ) ) : player->m_iTeamNum( ) };
    Assert( this->m_local->m_iTeamNum( ) == team && "Team number mismatch, perhaps you have a bug in your code?" );
#endif // ATLAS_DEV

    const auto hp{ player->IsDormant( ) ? this->m_resource->m_iHealth( )[ player->entindex( ) ] : player->m_iHealth( ) };
    const auto max_health{ this->m_resource->m_iMaxHealth( )[ player->entindex( ) ] };

    return hp < max_health;
}

void client::draw_3d_box( const vec3 &origin, const vec3 &mins, const vec3 &maxs, const color_t &col, const vec3 &angle_of_rotation )
{
    if ( col.a == 0 )
    {
        return;
    }

    vec3 ang{ angle_of_rotation };
    ang.y = math::normalize_yaw( angle_of_rotation.y );

    matrix3x4 mat{ };
    math::angle_matrix( ang, origin, mat );

    this->draw_3d_box( mat, mins, maxs, col );
}

void client::draw_3d_box( const matrix3x4 &transform, const vec3 &mins, const vec3 &maxs, const color_t &col )
{
    if ( col.a == 0 )
    {
        return;
    }

    vec3 points[ 8 ]{ };
    for ( i32 i{ }; i < 8; ++i )
    {
        points[ i ].x = ( i & 0x1 ) ? maxs.x : mins.x;
        points[ i ].y = ( i & 0x2 ) ? maxs.y : mins.y;
        points[ i ].z = ( i & 0x4 ) ? maxs.z : mins.z;

        points[ i ] = math::transform_point( points[ i ], transform );
    }

    static i32 face_indices[ 6 ][ 4 ] = {
        { 0, 4, 6, 2 },
        { 5, 1, 3, 7 },
        { 0, 1, 5, 4 },
        { 2, 6, 7, 3 },
        { 0, 2, 3, 1 },
        { 4, 5, 7, 6 },
    };

    for ( i32 i{ }; i < 6; ++i )
    {
        auto face = face_indices[ i ];
        for ( i32 j{ }; j < 4; ++j )
        {
            vec2 pos1{ }, pos2{ };
            if ( !this->clip_segment_to_screen( points[ face[ j ] ], points[ face[ j == 3 ? 0 : j + 1 ] ], pos1, pos2 ) )
            {
                return;
            }

            g_draw.line( pos1, pos2, col );
        }
    }
}

void client::draw_3d_line( const vec3 &start, const vec3 &end, const color_t &col, bool outlined )
{
    if ( col.a == 0 )
    {
        return;
    }

    vec2 start_pos{ }, end_pos{ };
    if ( !this->clip_segment_to_screen( start, end, start_pos, end_pos ) )
    {
        return;
    }

    if ( outlined )
    {
        g_draw.line( start_pos, end_pos, color_t{ 0, 0, 0, col.a }, 3.0f );
    }

    g_draw.line( start_pos, end_pos, col );
}

void client::draw_3d_overlay_circle( const vec3 &origin, const color_t &col, f32 radius, bool outlined )
{
    if ( col.a == 0 )
    {
        return;
    }

    vec2 screen_pos{ };
    if ( !this->world_to_screen( origin, screen_pos ) )
    {
        return;
    }

    if ( outlined )
    {
        g_draw.circle( screen_pos, color_t{ 0, 0, 0, col.a }, radius + 1.f, 1.5f );
        g_draw.circle( screen_pos, color_t{ 0, 0, 0, col.a }, radius - 1.f, 1.5f );
    }

    g_draw.circle( screen_pos, col, radius, 1.5f );
}

bool client::is_in_game_and_alive( )
{
    if ( !g_tf2.m_engine->IsInGame( ) )
    {
        return false;
    }

    if ( !g_cl.m_local || !g_cl.m_weapon )
    {
        return false;
    }

    if ( !g_cl.m_local->IsAlive( ) ||
         g_cl.m_local->IsGhost( ) )
    {
        return false;
    }

    return true;
}

usize client::get_profile( )
{
    if ( !g_cl.m_local )
    {
        return 0;
    }

    switch ( g_cl.m_local->m_iClass( ) )
    {
        case TF_CLASS_SCOUT: return config::CFG_PROFILE_SCOUT;
        case TF_CLASS_SOLDIER: return config::CFG_PROFILE_SOLDIER;
        case TF_CLASS_PYRO: return config::CFG_PROFILE_PYRO;
        case TF_CLASS_DEMOMAN: return config::CFG_PROFILE_DEMOMAN;
        case TF_CLASS_HEAVYWEAPONS: return config::CFG_PROFILE_HEAVY;
        case TF_CLASS_ENGINEER: return config::CFG_PROFILE_ENGINEER;
        case TF_CLASS_MEDIC: return config::CFG_PROFILE_MEDIC;
        case TF_CLASS_SNIPER: return config::CFG_PROFILE_SNIPER;
        case TF_CLASS_SPY: return config::CFG_PROFILE_SPY;
        default: return 0;
    }
}

void client::draw_3d_text( const vec3 &origin, const color_t &col, const char *buf )
{
    if ( col.a == 0 )
    {
        return;
    }

    vec2 screen_pos{ };
    if ( !this->world_to_screen( origin, screen_pos ) )
    {
        return;
    }

    g_draw.text( ATLAS_XORSTR( "Debug Overlay" ).get( ), point_t{ ( i32 )screen_pos.x, ( i32 )screen_pos.y }, col, buf, true );
}

bool client::will_fire_this_tick( ) const
{
    C_TFPlayer *local{ this->m_local };

    if ( !this->m_cmd || !local || !local->IsAlive( ) || local->IsTaunting( ) )
    {
        return false;
    }

    C_BaseEntity *active{ local->m_hActiveWeapon( ).Get( ) };

    if ( !active )
    {
        return false;
    }

    C_TFWeaponBase *weapon{ active->As< C_TFWeaponBase * >( ) };

    if ( !weapon )
    {
        return false;
    }

    CTFWeaponInfo *info{ weapon->GetWeaponInfo( ) };

    if ( !info )
    {
        return false;
    }

    // CTFWeaponBase::ItemPostFrame turns back before anything can fire while the weapon is down
    if ( weapon->m_bLowered( ) )
    {
        return false;
    }

    bool                attacking{ ( this->m_cmd->buttons & IN_ATTACK ) != 0 };
    const float         curtime{ TICKS_TO_TIME( local->m_nTickBase( ) ) };
    const WeaponData_t &data{ info->GetWeaponData( math::clamp( weapon->m_iWeaponMode( ), 0, 1 ) ) };
    if ( weapon->GetSlot( ) == 2 )
    {
        if ( attacking )
        {
            return true;
        }

        const float next{ weapon->m_flNextPrimaryAttack( ) };
        const float smack{ next - weapon->ApplyFireDelay( data.m_flTimeFireDelay ) +
                           data.m_flSmackDelay };

        return next > curtime && curtime <= smack;
    }

    switch ( weapon->GetWeaponID( ) )
    {
        case TF_WEAPON_MINIGUN:
        {
            if ( weapon->As< C_TFMinigun * >( )->m_iWeaponState( ) != AC_STATE_FIRING )
            {
                return false;
            }
            break;
        }

        case TF_WEAPON_PIPEBOMBLAUNCHER:
        {
            const float begin{ weapon->As< C_TFPipebombLauncher * >( )->m_flChargeBeginTime( ) };

            if ( begin <= 0.0f || local->m_flNextAttack( ) > curtime )
            {
                return false;
            }

            if ( !attacking )
            {
                return weapon->m_iClip1( ) > 0;
            }

            const float ceiling{ this->attrib_hook(
                weapon, 4.0f, ATLAS_XORSTR( "stickybomb_charge_rate" ).get( ) ) };

            return curtime - begin >= ceiling;
        }

        case TF_WEAPON_COMPOUND_BOW:
        {
            C_TFCompoundBow *bow{ weapon->As< C_TFCompoundBow * >( ) };
            if ( bow->m_bNoFire( ) || bow->m_iClip1( ) <= 0 || local->m_flNextAttack( ) > curtime )
            {
                return false;
            }

            // The primary timer gates a new draw; the inherited pipebomb release path does not
            // check it again for an arrow which is already drawn.
            return bow->m_flChargeBeginTime( ) > 0.0f && !attacking;
        }

        case TF_WEAPON_GRENADELAUNCHER:
        case TF_WEAPON_CANNON:
        {
            const float detonate{ weapon->As< C_TFGrenadeLauncher * >( )->m_flDetonateTime( ) };

            if ( detonate <= 0.0f )
            {
                if ( weapon->GetWeaponID( ) == TF_WEAPON_CANNON &&
                     this->attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "grenade_launcher_mortar_mode" ).get( ) ) > 0.0f )
                {
                    return false;
                }
                break;
            }

            if ( local->m_flNextAttack( ) > curtime )
            {
                return false;
            }

            if ( detonate <= curtime )
            {
                return true; // Misfire
            }

            return !attacking && weapon->m_iClip1( ) > 0;
        }

        case TF_WEAPON_SNIPERRIFLE_CLASSIC:
        {
            return weapon->As< C_TFSniperRifle * >( )->m_flChargedDamage( ) > 0.0f && !attacking;
        }

        case TF_WEAPON_PARTICLE_CANNON:
        {
            const float begin{ weapon->As< C_TFParticleCannon * >( )->m_flChargeBeginTime( ) };

            if ( local->m_flNextAttack( ) > curtime )
            {
                return false;
            }

            if ( begin > 0.0f )
            {
                return curtime - begin >= 2.0f;
            }

            const bool secondary{ ( this->m_cmd->buttons & IN_ATTACK2 ) != 0 &&
                                  weapon->m_flNextSecondaryAttack( ) <= curtime };
            return attacking && !secondary && weapon->m_flNextPrimaryAttack( ) <= curtime &&
                   weapon->HasPrimaryAmmoForShot( local );
        }

        case TF_WEAPON_FLAMETHROWER:
        {
            if ( local->m_nWaterLevel( ) == 3 ) // WL_Eyes
            {
                return false;
            }
            break;
        }

        case TF_WEAPON_FLAME_BALL:
        {
            if ( ( this->m_cmd->buttons & IN_ATTACK2 ) != 0 || !this->flame_ball_ready( weapon ) )
            {
                return false;
            }
            break;
        }

        default: break;
    }

    if ( weapon->m_iClip1( ) > 0 &&
         this->attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "auto_fires_full_clip" ).get( ) ) != 0.0f )
    {
        const i32  ammo_type{ weapon->m_iPrimaryAmmoType( ) };
        const bool reload_fires{ weapon->m_iReloadMode( ) == 0 &&
                                 ( ammo_type < 0 || ( &local->m_iAmmo( ) )[ ammo_type ] > 0 ) };

        attacking = !attacking || weapon->m_bFiringWholeClip( ) || reload_fires;
    }

    if ( !attacking )
    {
        return false;
    }

    if ( weapon->m_flNextPrimaryAttack( ) > curtime || local->m_flNextAttack( ) > curtime )
    {
        return false;
    }

    return weapon->HasPrimaryAmmoForShot( local );
}

void client::predict_reload_shell( C_TFWeaponBase *weapon )
{
    if ( !g_tf2.m_prediction->m_bInPrediction || !this->m_local || weapon != this->m_weapon )
    {
        return;
    }

    if ( weapon->m_iReloadMode( ) != TF_RELOADING_CONTINUE )
    {
        this->m_reload_gesture_start = -1.0f;
        return;
    }

    const float curtime{ g_tf2.m_globals->curtime };
    if ( this->m_reload_gesture_start < 0.0f )
    {
        this->m_reload_gesture_start = curtime;
    }

    if ( weapon->m_bReloadedThroughAnimEvent( ) )
    {
        return;
    }

    if ( weapon->m_iClip1( ) < 0 ||
         this->attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "can_overload" ).get( ) ) != 0.0f )
    {
        return;
    }

    const anim_overlay_t &overlay{ this->m_local->m_AnimOverlay( ) };
    if ( overlay.m_count <= GESTURE_SLOT_ATTACK_AND_RELOAD )
    {
        return;
    }

    const C_AnimationLayer &layer{ overlay.m_layers[ GESTURE_SLOT_ATTACK_AND_RELOAD ] };
    CStudioHdr             *studio{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< CStudioHdr * >( this->m_local ) };
    const i32               sequence{ layer.m_nSequence };

    if ( !studio ||
         !g_tf2.m_CStudioHdr_SequencesAvailable.call_as< bool, CStudioHdr * >( studio ) ||
         sequence < 0 ||
         sequence >= g_tf2.m_CStudioHdr_GetNumSeq.call_as< i32, CStudioHdr * >( studio ) )
    {
        return;
    }

    const mstudioseqdesc_t *seqdesc{
        g_tf2.m_CStudioHdr_pSeqdesc.call_as< const mstudioseqdesc_t *, CStudioHdr *, i32 >(
            studio, sequence )
    };
    if ( !seqdesc )
    {
        return;
    }

    constexpr i32 server_event{ AE_TYPE_NEWEVENTSYSTEM | AE_TYPE_SERVER };
    float         event_cycle{ -1.0f };
    for ( i32 index{ }; index < seqdesc->numevents; ++index )
    {
        const mstudioevent_t *event{ seqdesc->pEvent( index ) };
        if ( ( event->type & server_event ) == server_event && event->event == AE_WPN_INCREMENTAMMO )
        {
            event_cycle = event->cycle;
            break;
        }
    }

    if ( event_cycle < 0.0f )
    {
        return;
    }

    const float duration{ g_tf2.m_CBaseAnimating_SequenceDuration.call_as< float, C_BaseAnimating *,
                                                                           CStudioHdr *, i32 >(
        this->m_local, studio, sequence ) };
    if ( duration <= 0.0f )
    {
        return;
    }

    const float rate{ layer.m_flPlaybackRate / duration };
    const float step{ TICK_INTERVAL * rate };
    const i32   commands{ static_cast< i32 >( ( curtime - this->m_reload_gesture_start ) / TICK_INTERVAL + 0.5f ) };
    if ( commands < 1 )
    {
        return;
    }

    const float begin{ commands * step };
    const float last_visible{ 1.0f - seqdesc->fadeouttime * rate };
    if ( begin >= last_visible )
    {
        return;
    }

    float end{ ( commands + 1 ) * step };
    if ( end >= last_visible )
    {
        end = 1.0f;
    }

    if ( event_cycle < begin || event_cycle >= end )
    {
        return;
    }

    const i32 ammo_type{ weapon->m_iPrimaryAmmoType( ) };
    if ( ammo_type < 0 || ( &this->m_local->m_iAmmo( ) )[ ammo_type ] <= 0 )
    {
        return;
    }

    weapon->m_iClip1( ) += 1;
    ( &this->m_local->m_iAmmo( ) )[ ammo_type ] -= 1;
    weapon->m_bReloadedThroughAnimEvent( ) = true;
}
