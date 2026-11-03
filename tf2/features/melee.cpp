#include "melee.hpp"

#include "gui/gui.hpp"
#include "render/render.hpp"

#include "entity_cache.hpp"
#include "holdaim_mgr.hpp"
#include "move_sim.hpp"
#include "player_data.hpp"
#include "tickbase.hpp"

ATLAS_NO_DESTROY melee_assist g_melee{ };

constexpr f32 MELEE_TARGET_RADIUS{ 512.0f };

ATLAS_FORCE_INLINE i32 first_tick_after( f32 time )
{
    i32 tick{ static_cast< i32 >( time / TICK_INTERVAL ) };
    while ( TICKS_TO_TIME( tick ) <= time )
    {
        ++tick;
    }

    return tick;
}

ATLAS_FORCE_INLINE melee_point_t record_point( const lag_record_t &record, f32 scale )
{
    return melee_point_t{ record.m_pose.m_origin + record.m_mins * scale, record.m_pose.m_origin + record.m_maxs * scale,
                          record.m_tick };
}

ATLAS_FORCE_INLINE melee_point_t building_point( C_BaseEntity *building )
{
    const vec3 origin{ building->m_vecOrigin( ) };
    return melee_point_t{ origin + building->m_vecMins( ), origin + building->m_vecMaxs( ), -1 };
}

ATLAS_FORCE_INLINE f32 reach_depth( const vec3 &eye, const melee_point_t &point, const swing_ctx_t &ctx )
{
    vec3 nearest{ };
    for ( i32 axis{ }; axis < 3; ++axis )
    {
        nearest[ axis ] = math::clamp( eye[ axis ], point.m_mins[ axis ] - ctx.m_hull_maxs[ axis ],
                                       point.m_maxs[ axis ] - ctx.m_hull_mins[ axis ] );
    }

    return ctx.m_range - eye.dist_to( nearest );
}

ATLAS_FORCE_INLINE f32 unseen_drift( i32 ticks, f32 accel_step, f32 maxspeed )
{
    f32 spread{ };
    f32 drift{ };
    for ( i32 tick{ }; tick < ticks; ++tick )
    {
        spread = math::smaller( spread + 2.0f * accel_step, 2.0f * maxspeed );
        drift += spread * TICK_INTERVAL;
    }

    return drift;
}

ATLAS_FORCE_INLINE f32 released_drift( i32 ticks, f32 speed, f32 stopspeed, f32 friction )
{
    f32 slowed{ speed };
    f32 drift{ };
    for ( i32 tick{ }; tick < ticks; ++tick )
    {
        slowed = math::bigger( slowed - math::bigger( slowed, stopspeed ) * friction * TICK_INTERVAL, 0.0f );
        drift += ( speed - slowed ) * TICK_INTERVAL;
    }

    return drift;
}

ATLAS_FORCE_INLINE i32 kept_ticks( i32 index, f32 moving, f32 accel_step, f32 friction )
{
    const lag_record_t *newer{ };
    i32                 turns[ 3 ]{ };
    i32                 count{ };
    for ( i32 age{ }; age < g_lagcomp.count( index ) && count < 3; ++age )
    {
        const lag_record_t &record{ *g_lagcomp.record( index, age ) };
        if ( !g_lagcomp.continuous( index, record ) )
        {
            break;
        }

        const f32 speed{ record.m_velocity.length2d( ) };
        if ( speed < moving )
        {
            continue;
        }

        if ( newer && record.m_velocity.x * newer->m_velocity.x + record.m_velocity.y * newer->m_velocity.y < 0.0f )
        {
            const f32 slowing{ accel_step + ( record.m_on_ground ? speed * friction * TICK_INTERVAL : 0.0f ) };
            turns[ count++ ] = record.m_tick - static_cast< i32 >( ( speed - moving ) / slowing );
        }

        newer = &record;
    }

    if ( count < 2 )
    {
        return MAX_RECORDS;
    }

    const i32 shortest{ count == 3 ? math::smaller( turns[ 0 ] - turns[ 1 ], turns[ 1 ] - turns[ 2 ] ) : turns[ 0 ] - turns[ 1 ] };
    return math::bigger( shortest - ( g_lagcomp.record( index, 0 )->m_tick - turns[ 0 ] ), 0 );
}

ATLAS_FORCE_INLINE f32 unseen_rise( i32 ticks, f32 gravity )
{
    f32 speed{ JUMP_SPEED - gravity * TICK_INTERVAL };
    f32 rise{ };
    for ( i32 tick{ }; tick < ticks; ++tick )
    {
        rise += math::bigger( speed, 0.0f ) * TICK_INTERVAL;
        speed -= gravity * TICK_INTERVAL;
    }

    return rise;
}

ATLAS_FORCE_INLINE bool behind_and_facing( const vec3 &owner_center, const vec3 &target_center, const vec3 &owner_angles,
                                           const vec3 &target_angles )
{
    vec3 to_target{ target_center - owner_center };
    to_target.z = 0.0f;
    to_target.normalize( );

    vec3 owner_forward{ };
    vec3 target_forward{ };
    math::angle_vectors( owner_angles, &owner_forward );
    math::angle_vectors( target_angles, &target_forward );
    owner_forward.z  = 0.0f;
    target_forward.z = 0.0f;
    owner_forward.normalize( );
    target_forward.normalize( );

    return to_target.dot( target_forward ) > 0.0f && to_target.dot( owner_forward ) > 0.5f &&
           target_forward.dot( owner_forward ) > -0.3f;
}

ATLAS_FORCE_INLINE bool ray_box( const vec3 &start, const vec3 &direction, const vec3 &mins, const vec3 &maxs, f32 &enter )
{
    f32 t_enter{ 0.0f };
    f32 t_exit{ FLT_MAX };
    for ( i32 axis{ }; axis < 3; ++axis )
    {
        if ( math::abs( direction[ axis ] ) < 0.0000001f )
        {
            if ( start[ axis ] < mins[ axis ] || start[ axis ] > maxs[ axis ] )
            {
                return false;
            }

            continue;
        }

        const f32 inverse{ 1.0f / direction[ axis ] };
        f32       t_near{ ( mins[ axis ] - start[ axis ] ) * inverse };
        f32       t_far{ ( maxs[ axis ] - start[ axis ] ) * inverse };
        if ( t_near > t_far )
        {
            const f32 swapped{ t_near };
            t_near = t_far;
            t_far  = swapped;
        }

        t_enter = math::bigger( t_enter, t_near );
        t_exit  = math::smaller( t_exit, t_far );
        if ( t_enter > t_exit )
        {
            return false;
        }
    }

    enter = t_enter;
    return true;
}

class CTraceFilterMelee : public CTraceFilterSimple
{
public:
    CTraceFilterMelee( ) : CTraceFilterSimple( g_cl.m_local, COLLISION_GROUP_NONE )
    {
    }

    bool ShouldHitEntity( IHandleEntity *handle, i32 mask ) override
    {
        IClientEntity *client{ g_tf2.m_entity_list->GetClientEntityFromHandle( handle->GetRefEHandle( ) ) };
        if ( client )
        {
            C_BaseEntity    *entity{ client->GetBaseEntity( ) };
            const ETFClassID id{ entity->GetClientClass( )->GetClassID( ) };
            if ( id == ETFClassID::CTFPlayer ||
                 ( ( id == ETFClassID::CTFMedigunShield || id == ETFClassID::CTFReviveMarker ) &&
                   entity->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) ) )
            {
                return false;
            }
        }

        return CTraceFilterSimple::ShouldHitEntity( handle, mask );
    }
};

ATLAS_FORCE_INLINE bool melee_rewinds( C_TFPlayer *player, const vec3 &eye, const vec3 &direction )
{
    const vec3 local_origin{ eye - g_cl.m_local->m_vecViewOffset( ) };
    vec3       delta{ player->m_vecOrigin( ) - local_origin };
    const f32  distance{ delta.normalize( ) };
    return distance < 1.5f * player->m_flMaxspeed( ) * g_lagcomp.max_unlag( ) ||
           player->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) || direction.dot( delta ) >= 0.707107f;
}

ATLAS_FORCE_INLINE C_BaseEntity *swing_victim( const vec3 &eye, const vec3 &direction, const swing_ctx_t &ctx, bool hull,
                                               C_BaseEntity *target, const melee_point_t &point, bool &hit )
{
    const vec3 mins{ hull ? ctx.m_hull_mins : vec3{ } };
    const vec3 maxs{ hull ? ctx.m_hull_maxs : vec3{ } };

    Ray_t ray{ };
    ray.Init( eye, eye + direction * ctx.m_range, mins, maxs );

    trace_t           trace{ };
    CTraceFilterMelee filter{ };
    g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );

    f32                nearest{ trace.fraction * ctx.m_range };
    C_BaseEntity      *victim{ trace.m_pEnt };
    CTraceFilterSimple player_filter{ g_cl.m_local, COLLISION_GROUP_NONE };
    for ( i32 index{ 1 }; index <= g_tf2.m_globals->maxClients; ++index )
    {
        IClientEntity *client{ g_tf2.m_entity_list->GetClientEntity( index ) };
        if ( !client || client->IsDormant( ) )
        {
            continue;
        }

        C_TFPlayer *player{ client->GetBaseEntity( )->As< C_TFPlayer * >( ) };
        if ( player == g_cl.m_local || !player->IsAlive( ) || !player_filter.ShouldHitEntity( player, MASK_SOLID ) )
        {
            continue;
        }

        vec3 box_mins{ point.m_mins };
        vec3 box_maxs{ point.m_maxs };
        if ( player != target )
        {
            vec3 origin{ player->m_vecOrigin( ) };
            vec3 bounds_min{ player->m_vecMins( ) };
            vec3 bounds_max{ player->m_vecMaxs( ) };
            if ( point.m_tick >= 0 && melee_rewinds( player, eye, direction ) &&
                 g_lagcomp.hull_at( index, point.m_tick, origin, bounds_min, bounds_max ) )
            {
                bounds_min *= player->m_flModelScale( );
                bounds_max *= player->m_flModelScale( );
            }

            box_mins = origin + bounds_min;
            box_maxs = origin + bounds_max;
        }

        f32 enter{ };
        if ( ray_box( eye, direction, box_mins - maxs, box_maxs - mins, enter ) && enter < nearest )
        {
            nearest = enter;
            victim  = player;
        }
    }

    hit = nearest < ctx.m_range || trace.startsolid || trace.allsolid;
    return victim;
}

ATLAS_FORCE_INLINE bool swing_connects( const vec3 &eye, const vec3 &angle, const swing_ctx_t &ctx, C_BaseEntity *target,
                                        const melee_point_t &point )
{
    vec3 direction{ };
    math::angle_vectors( angle, &direction );

    if ( point.m_tick >= 0 && !melee_rewinds( target->As< C_TFPlayer * >( ), eye, direction ) )
    {
        return false;
    }

    bool          hit{ };
    C_BaseEntity *victim{ swing_victim( eye, direction, ctx, false, target, point, hit ) };
    if ( !hit )
    {
        victim = swing_victim( eye, direction, ctx, true, target, point, hit );
    }

    return hit && victim == target;
}

ATLAS_FORCE_INLINE vec3 aim_toward( const vec3 &eye, const vec3 &destination )
{
    vec3       angle{ g_cl.m_cmd->viewangles };
    const vec3 delta{ destination - eye };
    if ( delta.length_sqr( ) > 0.0f )
    {
        math::vector_angles( delta, angle );
    }

    math::clamp_angles( angle );
    return angle;
}

ATLAS_FORCE_INLINE bool can_hit( const vec3 &eye, const melee_point_t &point, const swing_ctx_t &ctx, C_BaseEntity *target,
                                 vec3 &angle )
{
    if ( reach_depth( eye, point, ctx ) <= 0.0f )
    {
        return false;
    }

    if ( const vec3 *held{ g_holdaim_mgr.held( ) } )
    {
        angle = *held;
        return swing_connects( eye, angle, ctx, target, point );
    }

    const f32 epsilon{ static_cast< f32 >( DIST_EPSILON ) };
    vec3      inside{ };
    vec3      swept{ };
    for ( i32 axis{ }; axis < 3; ++axis )
    {
        inside[ axis ] = math::clamp( eye[ axis ], point.m_mins[ axis ], point.m_maxs[ axis ] );
        swept[ axis ]  = math::clamp( eye[ axis ], point.m_mins[ axis ] - ctx.m_hull_maxs[ axis ] + epsilon,
                                      point.m_maxs[ axis ] - ctx.m_hull_mins[ axis ] - epsilon );
    }

    angle = aim_toward( eye, inside );
    if ( swing_connects( eye, angle, ctx, target, point ) )
    {
        return true;
    }

    angle = aim_toward( eye, swept );
    return swing_connects( eye, angle, ctx, target, point );
}

ATLAS_FORCE_INLINE bool can_backstab( const vec3 &eye, const melee_point_t &point, const swing_ctx_t &ctx, C_TFPlayer *target,
                                      vec3 &angle )
{
    if ( reach_depth( eye, point, ctx ) <= 0.0f )
    {
        return false;
    }

    const vec3 center{ ( point.m_mins + point.m_maxs ) * 0.5f };
    if ( const vec3 *held{ g_holdaim_mgr.held( ) } )
    {
        angle = *held;
    } else
    {
        angle = aim_toward( eye, vec3{ center.x, center.y, math::clamp( eye.z, point.m_mins.z, point.m_maxs.z ) } );
    }

    if ( !behind_and_facing( g_cl.m_local->WorldSpaceCenter( ), center, angle, target->m_angEyeAngles( ) ) )
    {
        return false;
    }

    return swing_connects( eye, angle, ctx, target, point );
}

ATLAS_FORCE_INLINE void weigh( const swing_view_t &view, const melee_point_t &point, f32 margin, C_BaseEntity *entity,
                               const swing_ctx_t &ctx, swing_choice_t &choice )
{
    if ( margin <= choice.m_margin )
    {
        return;
    }

    vec3 angle{ };
    if ( !can_hit( view.m_eye, point, ctx, entity, angle ) )
    {
        return;
    }

    choice = swing_choice_t{ entity, point, margin };
}

ATLAS_FORCE_INLINE bool closer( const melee_target_t &first, const melee_target_t &second )
{
    return first.m_distance < second.m_distance;
}

void melee_assist::think( )
{
    if ( !this->should_run( ) )
    {
        this->m_smack_time = -1.0f;
        return;
    }

    const bool  knife{ g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_KNIFE };
    swing_ctx_t ctx{ };
    this->build_swing_ctx( ctx );

    own_path_t own{ };
    if ( !knife && g_cfg.m_melee.m_swing_prediction )
    {
        this->track_own_path( ctx, own );
    }

    const bool keyed{ gui::bind_active( g_cfg.m_melee.m_activate_key ) };
    if ( !keyed && this->m_smack_time <= 0.0f && this->m_auto_command != g_cl.m_cmd->command_number - 1 )
    {
        return;
    }

    const bool smack{ !knife && this->smack_this_command( ) };

    const bool auto_swing{ g_cfg.m_melee.m_auto_swing && !g_cl.m_local->InCond( TF_COND_SHIELD_CHARGE ) };

    if ( !smack && ( this->m_smack_time > 0.0f || !this->weapon_ready( ) ) )
    {
        return;
    }

    if ( !smack && ( !keyed || ( !knife && !auto_swing ) ) )
    {
        return;
    }

    this->fetch_targets( );

    if ( knife )
    {
        this->try_backstab( ctx );
        return;
    }

    if ( smack )
    {
        this->steer_smack( ctx );
        return;
    }

    if ( this->should_swing( ctx, own ) && auto_swing )
    {
        g_cl.m_cmd->buttons |= IN_ATTACK;
        this->m_auto_command = g_cl.m_cmd->command_number;
    }
}

bool melee_assist::should_run( ) const
{
    Assert( g_cl.m_cmd != nullptr && g_cl.m_local != nullptr && g_cl.m_weapon != nullptr );

    if ( !g_cfg.m_melee.m_enabled )
    {
        return false;
    }

    if ( g_gui.open( ) )
    {
        return false;
    }

    C_TFPlayer *local{ g_cl.m_local };
    if ( local->IsInKart( ) ||
         local->IsGhost( ) ||
         local->IsTaunting( ) ||
         local->IsBonked( ) ||
         local->IsCloaked( ) ||
         local->IsFeigned( ) )
    {
        return false;
    }

    if ( g_cl.m_weapon->GetSlot( ) != 2 )
    {
        return false;
    }

    if ( g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_KNIFE && !g_cfg.m_melee.m_auto_backstab )
    {
        return false;
    }

    return true;
}

bool melee_assist::weapon_ready( ) const
{
    const f32 curtime{ TICKS_TO_TIME( g_cl.m_local->m_nTickBase( ) ) };
    if ( g_cl.m_weapon->m_flNextPrimaryAttack( ) > curtime )
    {
        return false;
    }

    return g_cl.weapon_attack_allowed( curtime );
}

void melee_assist::build_swing_ctx( swing_ctx_t &ctx ) const
{
    C_TFPlayer     *local{ g_cl.m_local };
    C_TFWeaponBase *weapon{ g_cl.m_weapon };

    f32 range{ 48.0f };
    if ( local->InCond( TF_COND_SHIELD_CHARGE ) )
    {
        range = 128.0f;
    } else if ( weapon->GetWeaponID( ) == TF_WEAPON_SWORD ||
                g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "is_a_sword" ).get( ) ) != 0.0f )
    {
        range = 72.0f;
    }

    f32       bounds{ 18.0f * g_cl.attrib_hook( weapon, 1.0f, ATLAS_XORSTR( "melee_bounds_multiplier" ).get( ) ) };
    const f32 scale{ local->m_flModelScale( ) };
    if ( scale > 1.0f )
    {
        range *= scale;
        bounds *= scale;
    }

    ctx.m_range     = g_cl.attrib_hook( weapon, range, ATLAS_XORSTR( "melee_range_multiplier" ).get( ) );
    ctx.m_hull_mins = vec3{ -bounds, -bounds, -bounds };
    ctx.m_hull_maxs = vec3{ bounds, bounds, bounds };

    CTFWeaponInfo *info{ weapon->GetWeaponInfo( ) };
    Assert( info != nullptr );

    const i32 tickbase{ local->m_nTickBase( ) };
    ctx.m_smack_ticks = first_tick_after( TICKS_TO_TIME( tickbase ) + info->GetWeaponData( 0 ).m_flSmackDelay ) - tickbase;
}

bool melee_assist::allow_player( C_TFPlayer *player ) const
{
    if ( player == g_cl.m_local || player->IsDormant( ) || !player->IsAlive( ) || player->IsGhost( ) )
    {
        return false;
    }

    if ( player->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) )
    {
        return false;
    }

    const bool *ignored{ g_cfg.m_ragebot.m_ignores };
    if ( player->IsCloaked( ) && ignored[ config::IGNORE_CLOAKED ] )
    {
        return false;
    }

    if ( player->IsDisguised( ) && ignored[ config::IGNORE_DISGUISED ] )
    {
        return false;
    }

    if ( player->IsFeigned( ) && ignored[ config::IGNORE_FEIGNED ] )
    {
        return false;
    }

    if ( player->IsUbered( ) && ignored[ config::IGNORE_UBERED ] )
    {
        return false;
    }

    if ( player->IsBonked( ) && ignored[ config::IGNORE_BONKED ] )
    {
        return false;
    }

    if ( player->IsTaunting( ) && ignored[ config::IGNORE_TAUNTING ] )
    {
        return false;
    }

    player_info_t info{ };
    if ( !g_tf2.m_engine->GetPlayerInfo( player->entindex( ), &info ) )
    {
        return false;
    }

    return !g_cl.is_ignored_player( info.friendsID );
}

bool melee_assist::allow_building( C_BaseEntity *entity, ETFClassID id ) const
{
    C_BaseObject *building{ entity->As< C_BaseObject * >( ) };
    if ( building->IsDormant( ) ||
         building->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) ||
         building->m_iHealth( ) <= 0 ||
         building->m_bCarried( ) ||
         building->m_bPlacing( ) )
    {
        return false;
    }

    switch ( id )
    {
        case ETFClassID::CObjectSentrygun: return g_cfg.m_ragebot.m_targets[ config::TARGET_SENTRYGUNS ];
        case ETFClassID::CObjectDispenser: return g_cfg.m_ragebot.m_targets[ config::TARGET_DISPENSERS ];
        case ETFClassID::CObjectTeleporter: return g_cfg.m_ragebot.m_targets[ config::TARGET_TELEPORTERS ];
        default: return false;
    }
}

void melee_assist::fetch_targets( )
{
    this->m_targets.clear( );

    const vec3 eye{ g_cl.m_local->Weapon_ShootPosition( ) };
    for ( const auto *cached : g_entity_cache.players( ) )
    {
        C_TFPlayer *player{ cached->m_entity->As< C_TFPlayer * >( ) };
        if ( !this->allow_player( player ) )
        {
            continue;
        }

        this->m_targets.push_back( melee_target_t{ player, eye.dist_to( player->WorldSpaceCenter( ) ), true } );
    }

    for ( const auto *cached : g_entity_cache.buildings( ) )
    {
        if ( !this->allow_building( cached->m_entity, cached->m_class_id ) )
        {
            continue;
        }

        C_BaseEntity *building{ cached->m_entity };
        this->m_targets.push_back( melee_target_t{ building, eye.dist_to( building->WorldSpaceCenter( ) ), false } );
    }

    atl::sort( this->m_targets.begin( ), this->m_targets.end( ), closer );
}

bool melee_assist::smack_this_command( )
{
    const u32 weapon{ static_cast< u32 >( g_cl.m_weapon->GetRefEHandle( ).ToInt( ) ) };
    const f32 pending{ g_cl.m_weapon->m_flSmackTime( ) };

    if ( pending > 0.0f )
    {
        this->m_smack_time   = pending;
        this->m_smack_weapon = weapon;
        return false;
    }

    const f32 stored{ this->m_smack_time };
    this->m_smack_time = -1.0f;
    if ( stored <= 0.0f )
    {
        return false;
    }

    const bool smack{ weapon == this->m_smack_weapon && TICKS_TO_TIME( g_cl.m_local->m_nTickBase( ) ) > stored };
    return smack;
}

bool melee_assist::select_record( C_TFPlayer *player, const vec3 &eye, const swing_ctx_t &ctx, const rewind_window_t &window,
                                  bool backstab, melee_point_t &point, vec3 &angle ) const
{
    const i32 index{ player->entindex( ) };
    const f32 scale{ player->m_flModelScale( ) };

    i32 order[ MAX_RECORDS ]{ };
    f32 offsets[ MAX_RECORDS ]{ };
    i32 count{ };
    for ( i32 age{ }; age < g_lagcomp.count( index ); ++age )
    {
        const lag_record_t &record{ *g_lagcomp.record( index, age ) };
        if ( !g_lagcomp.continuous( index, record ) )
        {
            break;
        }

        if ( !window.accepts( record.m_tick ) )
        {
            continue;
        }

        const f32 offset{ math::abs( window.m_correct - window.age( record.m_tick ) ) };
        i32       at{ count++ };
        for ( ; at > 0 && offsets[ at - 1 ] > offset; --at )
        {
            order[ at ]   = order[ at - 1 ];
            offsets[ at ] = offsets[ at - 1 ];
        }

        order[ at ]   = age;
        offsets[ at ] = offset;
    }

    for ( i32 rank{ }; rank < count; ++rank )
    {
        const melee_point_t tried{ record_point( *g_lagcomp.record( index, order[ rank ] ), scale ) };
        const bool          hit{ backstab ? can_backstab( eye, tried, ctx, player, angle ) : can_hit( eye, tried, ctx, player, angle ) };
        if ( hit )
        {
            point = tried;
            return true;
        }
    }

    return false;
}

void melee_assist::weigh_player( C_TFPlayer *player, const swing_ctx_t &ctx, const swing_view_t *views, i32 view_count,
                                 swing_choice_t *choices )
{
    const i32           index{ player->entindex( ) };
    const lag_record_t *newest{ g_lagcomp.record( index, 0 ) };
    if ( !newest )
    {
        return;
    }

    const f32 scale{ player->m_flModelScale( ) };
    for ( i32 age{ }; age < g_lagcomp.count( index ); ++age )
    {
        const lag_record_t &record{ *g_lagcomp.record( index, age ) };
        if ( !g_lagcomp.continuous( index, record ) )
        {
            break;
        }

        const melee_point_t point{ record_point( record, scale ) };
        for ( i32 view{ }; view < view_count; ++view )
        {
            if ( !views[ view ].m_window.accepts( point.m_tick ) )
            {
                continue;
            }

            const f32 reach{ reach_depth( views[ view ].m_eye, point, ctx ) };
            weigh( views[ view ], point, reach, player, ctx, choices[ view ] );
        }
    }

    const velocity_sample_t *seed{ g_player_data.velocity_sample( index, 0 ) };
    const i32                horizon{ views[ view_count - 1 ].m_ticks };
    if ( horizon <= 0 || !seed || views[ 0 ].m_eye.dist_to( newest->m_pose.m_origin ) > MELEE_TARGET_RADIUS )
    {
        return;
    }

    move_sim_ctx_t walk{ player };
    for ( i32 tick{ }; tick < horizon; ++tick )
    {
        if ( !g_move_sim.run_tick( &walk ) )
        {
            break;
        }
    }

    const f32  maxspeed{ player->m_flMaxspeed( ) };
    const f32  accel_step{ math::bigger( g_tf2.sv_accelerate->GetFloat( ), g_tf2.sv_airaccelerate->GetFloat( ) ) * maxspeed *
                           TICK_INTERVAL };
    const f32  stopspeed{ g_tf2.sv_stopspeed->GetFloat( ) };
    const f32  ground_friction{ g_tf2.sv_friction->GetFloat( ) };
    const f32  friction{ newest->m_on_ground ? ground_friction : 0.0f };
    const f32  speed{ newest->m_velocity.length2d( ) };
    const i32  kept{ kept_ticks( index, stopspeed, accel_step, ground_friction ) };
    const f32  gravity{ g_tf2.sv_gravity->GetFloat( ) };
    const bool can_jump{ newest->m_on_ground || player->m_iClass( ) == TF_CLASS_SCOUT };

    for ( i32 step{ 1 }; step < walk.m_track_count; ++step )
    {
        const vec3         &origin{ walk.m_track[ step ] };
        const melee_point_t point{ origin + newest->m_mins * scale, origin + newest->m_maxs * scale, seed->m_tick + step };
        const i32           unseen{ point.m_tick - newest->m_tick };
        const f32           drift{ math::bigger( released_drift( unseen, speed, stopspeed, friction ),
                                                 unseen_drift( unseen - kept, accel_step, maxspeed ) ) };

        const vec3          rise{ 0.0f, 0.0f, can_jump ? unseen_rise( unseen, gravity ) : 0.0f };
        const melee_point_t jumped{ point.m_mins + rise, point.m_maxs + rise, point.m_tick };
        for ( i32 view{ }; view < view_count; ++view )
        {
            if ( step > views[ view ].m_ticks || !views[ view ].m_window.accepts( point.m_tick ) )
            {
                continue;
            }

            const vec3 &eye{ views[ view ].m_eye };
            const f32   reach{ reach_depth( eye, point, ctx ) };
            const f32   margin{ math::smaller( reach, reach_depth( eye, jumped, ctx ) ) - drift };
            weigh( views[ view ], point, margin, player, ctx, choices[ view ] );
        }
    }
}

void melee_assist::weigh_building( C_BaseEntity *building, const swing_ctx_t &ctx, const swing_view_t *views, i32 view_count,
                                   swing_choice_t *choices )
{
    const melee_point_t point{ building_point( building ) };
    for ( i32 view{ }; view < view_count; ++view )
    {
        const f32 reach{ reach_depth( views[ view ].m_eye, point, ctx ) };
        weigh( views[ view ], point, reach, building, ctx, choices[ view ] );
    }
}

void melee_assist::weigh_targets( const swing_ctx_t &ctx, const swing_view_t *views, i32 view_count, swing_choice_t *choices )
{
    for ( const auto &target : this->m_targets )
    {
        if ( target.m_is_player )
        {
            this->weigh_player( target.m_entity->As< C_TFPlayer * >( ), ctx, views, view_count, choices );
            continue;
        }

        this->weigh_building( target.m_entity, ctx, views, view_count, choices );
    }
}

void melee_assist::track_own_path( const swing_ctx_t &ctx, own_path_t &own )
{
    const i32  smack{ ctx.m_smack_ticks };
    const i32  tickbase{ g_cl.m_local->m_nTickBase( ) };
    const vec3 eye{ g_cl.m_local->Weapon_ShootPosition( ) };
    Assert( smack + 2 <= MAX_PATH_TICKS );

    const own_forecast_t &due{ this->m_forecasts[ tickbase % MAX_PATH_TICKS ] };
    if ( due.m_tickbase == tickbase && due.m_airborne )
    {
        this->m_strays[ tickbase % MAX_PATH_TICKS ] = own_stray_t{ tickbase, eye.dist_to( due.m_eye ) };
    }

    vec3 eyes[ MAX_PATH_TICKS ]{ };
    g_extrapolate.simulate_local( smack + 1, eyes, MAX_PATH_TICKS, nullptr, nullptr );

    own.m_eyes[ 0 ] = eyes[ smack ];
    own.m_eyes[ 1 ] = eyes[ smack + 1 ];
    own.m_airborne  = !g_cl.m_local->IsOnGround( );
    own.m_simulated = true;

    this->m_forecasts[ ( tickbase + smack ) % MAX_PATH_TICKS ] = own_forecast_t{ tickbase + smack, own.m_eyes[ 0 ], own.m_airborne };

}

f32 melee_assist::own_stray( i32 smack ) const
{
    const i32 tickbase{ g_cl.m_local->m_nTickBase( ) };
    f32       largest{ };
    for ( const own_stray_t &stray : this->m_strays )
    {
        if ( stray.m_tickbase > tickbase - smack && stray.m_tickbase <= tickbase )
        {
            largest = math::bigger( largest, stray.m_error );
        }
    }

    return largest;
}

bool melee_assist::should_swing( const swing_ctx_t &ctx, const own_path_t &own )
{
    if ( !own.m_simulated )
    {
        const swing_view_t now{ g_cl.m_local->Weapon_ShootPosition( ), g_lagcomp.rewind_window( 0, 1 ), 0 };
        swing_choice_t     choice{ };
        this->weigh_targets( ctx, &now, 1, &choice );

        return choice.m_margin > 0.0f;
    }

    const i32          smack{ ctx.m_smack_ticks };
    const swing_view_t views[ 2 ]{
        swing_view_t{ own.m_eyes[ 0 ], g_lagcomp.rewind_window( smack, 1 ), smack },
        swing_view_t{ own.m_eyes[ 1 ], g_lagcomp.rewind_window( smack + 1, 1 ), smack + 1 },
    };

    swing_choice_t choices[ 2 ]{ };
    this->weigh_targets( ctx, views, own.m_airborne ? 2 : 1, choices );

    if ( choices[ 0 ].m_margin <= 0.0f )
    {
        return false;
    }

    const f32 stray{ own.m_airborne ? this->own_stray( smack ) : 0.0f };
    if ( choices[ 0 ].m_margin < stray && choices[ 1 ].m_margin > choices[ 0 ].m_margin )
    {
        DBGOUT( "melee_assist::should_swing: hold tick=%d cmd=%d tb=%d ent=%d cand=%d margin=%.1f next=%.1f stray=%.1f",
                g_tf2.m_globals->tickcount, g_cl.m_cmd->command_number, g_cl.m_local->m_nTickBase( ),
                choices[ 0 ].m_entity->entindex( ), choices[ 0 ].m_point.m_tick, choices[ 0 ].m_margin, choices[ 1 ].m_margin,
                stray );
        return false;
    }

    return true;
}

bool melee_assist::steer_smack( const swing_ctx_t &ctx )
{
    const vec3            eye{ g_cl.m_local->Weapon_ShootPosition( ) };
    const rewind_window_t window{ g_lagcomp.rewind_window( 0, 1 ) };

    for ( const auto &target : this->m_targets )
    {
        melee_point_t point{ };
        vec3          angle{ };
        bool          hit{ };
        if ( target.m_is_player )
        {
            hit = this->select_record( target.m_entity->As< C_TFPlayer * >( ), eye, ctx, window, false, point, angle );
        } else
        {
            point = building_point( target.m_entity );
            hit   = can_hit( eye, point, ctx, target.m_entity, angle );
        }

        if ( !hit )
        {
            continue;
        }

        this->aim( angle, point.m_tick );

        return true;
    }

    return false;
}

bool melee_assist::try_backstab( const swing_ctx_t &ctx )
{
    if ( g_cl.m_local->InCond( TF_COND_GRAPPLED_BY_PLAYER ) || g_cl.m_local->InCond( TF_COND_GRAPPLED_TO_PLAYER ) )
    {
        return false;
    }

    const vec3            eye{ g_cl.m_local->Weapon_ShootPosition( ) };
    const rewind_window_t window{ g_lagcomp.rewind_window( 0, 1 ) };
    for ( const auto &target : this->m_targets )
    {
        if ( !target.m_is_player )
        {
            continue;
        }

        C_TFPlayer *player{ target.m_entity->As< C_TFPlayer * >( ) };
        if ( player->IsUbered( ) ||
             player->IsBonked( ) ||
             player->InCond( TF_COND_RUNE_RESIST ) ||
             g_cl.attrib_hook( player, 0.0f, ATLAS_XORSTR( "cannot_be_backstabbed" ).get( ) ) != 0.0f )
        {
            continue;
        }

        melee_point_t point{ };
        vec3          angle{ };
        if ( this->select_record( player, eye, ctx, window, true, point, angle ) )
        {
            this->aim( angle, point.m_tick );
            g_cl.m_cmd->buttons |= IN_ATTACK;
            return true;
        }
    }

    return false;
}

void melee_assist::aim( const vec3 &angle, i32 declare_tick )
{
    g_cl.m_cmd->viewangles = angle;

    if ( declare_tick >= 0 )
    {
        g_cl.m_cmd->tick_count = g_lagcomp.command_tick( declare_tick );
    }

    this->m_aimed_command = g_cl.m_cmd->command_number;
    g_cl.m_psilent_choked = true;
}
