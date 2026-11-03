#include "auto_det.hpp"
#include "entity_cache.hpp"

#include "gui/gui.hpp"
#include "holdaim_mgr.hpp"
#include "move_sim.hpp"
#include "networking.hpp"
#include "player_data.hpp"
#include "ragebot.hpp"
#include "splash.hpp"
#include "tickbase.hpp"

ATLAS_NO_DESTROY auto_det g_auto_det{ };

constexpr i32 REST_TICKS{ 4 };

namespace
{
    class CTraceFilterDetonate : public CTraceFilterSimple
    {
    public:
        explicit CTraceFilterDetonate( C_BaseEntity *projectile ) :
            CTraceFilterSimple( projectile, COLLISION_GROUP_NONE )
        {
        }

        bool ShouldHitEntity( IHandleEntity *handle, i32 mask ) override
        {
            if ( !g_tf2.m_static_prop_mgr->IsStaticProp( handle ) )
            {
                IClientEntity *client{ g_tf2.m_entity_list->GetClientEntityFromHandle( handle->GetRefEHandle( ) ) };
                C_BaseEntity  *entity{ client ? client->GetBaseEntity( ) : nullptr };
                if ( entity && entity->GetClientClass( )->GetClassID( ) == ETFClassID::CTFPlayer )
                {
                    return false;
                }
            }

            return CTraceFilterSimple::ShouldHitEntity( handle, mask );
        }
    };

    ATLAS_FORCE_INLINE bool targetable_building( C_BaseEntity *entity )
    {
        C_BaseObject *object{ entity->As< C_BaseObject * >( ) };
        return !object->m_bPlacing( ) && !object->m_bCarried( ) && object->m_iHealth( ) > 0;
    }

    ATLAS_FORCE_INLINE bool networks_as( const vec3 &position, const vec3 &origin )
    {
        for ( i32 axis{ }; axis < 3; ++axis )
        {
            if ( static_cast< f32 >( static_cast< i32 >( position[ axis ] ) ) != origin[ axis ] )
            {
                return false;
            }
        }

        return true;
    }
} // namespace

void auto_det::run( )
{
    this->m_blast_count  = 0;
    this->m_target_count = 0;

    config::auto_det_t &cfg{ g_cfg.m_auto_det };

    if ( !cfg.m_enabled || !g_cl.m_cmd || !g_cl.m_local || !g_cl.m_weapon || g_gui.open( ) )
    {
        return;
    }

    if ( !g_cl.m_local->IsAlive( ) || g_cl.m_local->IsTaunting( ) || g_cl.m_weapon->m_bLowered( ) )
    {
        return;
    }

    if ( !gui::bind_active( cfg.m_key ) )
    {
        return;
    }

    const launcher_t launcher{ resolve( g_cl.m_local, g_cl.m_weapon ) };
    if ( launcher.m_mode == mode_t::none || !this->timing( ) )
    {
        return;
    }

    this->collect_blasts( launcher );
    if ( this->m_blast_count == 0 )
    {
        return;
    }

    this->collect_targets( );
    if ( this->m_target_count == 0 )
    {
        return;
    }

    if ( launcher.m_mode == mode_t::stickies )
    {
        for ( usize index{ }; index < this->m_blast_count; ++index )
        {
            this->place( this->m_blasts[ index ] );
        }
    }

    const f32 scale{ static_cast< f32 >( cfg.m_radius_scale ) * 0.01f };
    i32       hits{ };

    for ( usize index{ }; index < this->m_target_count; ++index )
    {
        if ( this->hit( this->m_targets[ index ], scale ) )
        {
            ++hits;
        }
    }

    if ( hits < cfg.m_min_targets )
    {
        return;
    }

    if ( cfg.m_avoid_self && this->hurts_self( launcher ) )
    {
        return;
    }

    g_cl.m_cmd->buttons |= IN_ATTACK2;
    g_cl.m_send_packet = true;

    DBGOUT( "auto det: %d targets, %d blasts, lead %d ticks", hits, static_cast< i32 >( this->m_blast_count ),
            this->m_server_tick - this->m_snapshot_tick );
}

void auto_det::capture( )
{
    if ( !g_cfg.m_auto_det.m_enabled || !g_cl.m_will_fire || g_cl.m_cmd->weaponselect != 0 ||
         g_cl.m_weapon->GetWeaponID( ) != TF_WEAPON_PIPEBOMBLAUNCHER )
    {
        return;
    }

    const vec3        *held{ g_holdaim_mgr.held( ) };
    projectile_setup_t launch{ };
    if ( !g_proj_sim.gather( g_cl.m_local, g_cl.m_weapon, held ? *held : g_cl.m_cmd->viewangles, launch,
                             g_cl.m_cmd->command_number ) )
    {
        return;
    }

    simulate( launch, TIME_TO_TICKS( launch.m_lifetime ), this->m_flights[ this->m_flight_next ] );
    this->m_flight_next = ( this->m_flight_next + 1 ) % MAX_FLIGHTS;
}

void auto_det::on_network_update( )
{
    C_TFWeaponBase *launcher{ g_cfg.m_auto_det.m_enabled && g_cl.m_local
                                  ? g_cl.m_local->Weapon_OwnsThisID( TF_WEAPON_PIPEBOMBLAUNCHER )
                                  : nullptr };
    if ( !launcher )
    {
        this->clear( );
        return;
    }

    for ( fired_t &fired : this->m_fired )
    {
        if ( fired.m_handle == INVALID_EHANDLE_INDEX )
        {
            continue;
        }

        C_BaseEntity *entity{ g_entity_cache.resolve( fired.m_handle ) };
        if ( !entity || entity->As< C_TFGrenadePipebombProjectile * >( )->m_hLauncher( ).Get( ) != launcher )
        {
            fired = fired_t{ };
        }
    }

    for ( const auto *cached : g_entity_cache.projectiles( ) )
    {
        if ( cached->m_class_id != ETFClassID::CTFGrenadePipebombProjectile || !cached->networked( ) )
        {
            continue;
        }

        C_TFGrenadePipebombProjectile *pipe{ cached->m_entity->As< C_TFGrenadePipebombProjectile * >( ) };
        if ( pipe->m_hLauncher( ).Get( ) != launcher )
        {
            continue;
        }

        if ( fired_t * record{ this->fired( pipe, launcher ) } )
        {
            if ( !pipe->m_bTouched( ) )
            {
                record->m_touched_tick = -1;
            } else if ( record->m_touched_tick < 0 )
            {
                record->m_touched_tick = TIME_TO_TICKS( pipe->m_flSimulationTime( ) );
            }
        }

        this->bind( pipe );
    }
}

void auto_det::clear( )
{
    for ( fired_t &fired : this->m_fired )
    {
        fired = fired_t{ };
    }

    for ( flight_t &flight : this->m_flights )
    {
        flight.m_handle = INVALID_EHANDLE_INDEX;
        flight.m_count  = 0;
    }

    this->m_blast_count  = 0;
    this->m_target_count = 0;
}

auto_det::launcher_t auto_det::resolve( C_TFPlayer *local, C_TFWeaponBase *active )
{
    launcher_t launcher{ };

    if ( C_TFWeaponBase * weapon{ local->Weapon_OwnsThisID( TF_WEAPON_PIPEBOMBLAUNCHER ) } )
    {
        launcher.m_mode        = mode_t::stickies;
        launcher.m_weapon      = weapon;
        launcher.m_self_radius = TF_GRENADE_JUMP_RADIUS;
        launcher.m_live_time   = splash::arm_time( weapon );
        launcher.m_fizzle_time = g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "stickybomb_fizzle_time" ).get( ) );
        launcher.m_view_gated  = static_cast< i32 >( g_cl.attrib_hook(
                                     weapon, 0.0f, ATLAS_XORSTR( "set_detonate_mode" ).get( ) ) ) == TF_DETONATE_MODE_DOT;
    } else if ( active->GetWeaponID( ) == TF_WEAPON_FLAREGUN &&
                static_cast< i32 >( g_cl.attrib_hook( active, 0.0f, ATLAS_XORSTR( "set_weapon_mode" ).get( ) ) ) == FLAREGUN_DETONATE )
    {
        // the flares only go off from the gun in hand
        launcher.m_mode        = mode_t::flare;
        launcher.m_weapon      = active;
        launcher.m_self_radius = TF_FLARE_RADIUS_FOR_FJS;
    } else
    {
        return launcher;
    }

    launcher.m_radius = launcher.m_weapon->GetProjectileBlastRadius( );
    if ( launcher.m_radius <= 0.0f || launcher.m_weapon->As< C_TFWeaponBaseGun * >( )->GetProjectileDamage( ) <= 0.0f )
    {
        return launcher_t{ };
    }

    return launcher;
}

auto_det::fired_t *auto_det::fired( C_BaseEntity *pipe, C_TFWeaponBase *launcher )
{
    const u32 handle{ static_cast< u32 >( pipe->GetRefEHandle( ).ToInt( ) ) };
    fired_t  *slot{ };

    for ( fired_t &fired : this->m_fired )
    {
        if ( fired.m_handle == handle )
        {
            return &fired;
        }

        if ( !slot && fired.m_handle == INVALID_EHANDLE_INDEX )
        {
            slot = &fired;
        }
    }

    if ( slot )
    {
        *slot          = fired_t{ };
        slot->m_handle = handle;
        slot->m_time   = launcher->m_flLastFireTime( );
    }

    return slot;
}

bool auto_det::timing( )
{
    const i32                           delay{ g_networking.send_delay( ).ticks( ) };
    const command_tickbase_projection_t command{ g_tickbase.project_current_command_tickbase( delay ) };
    const i32                           frame{ g_tickbase.command_frame( g_cl.m_cmd->command_number ) };
    if ( !command.valid( ) || frame < 0 )
    {
        return false;
    }

    this->m_command_time  = TICKS_TO_TIME( command.m_player_tickbase );
    this->m_snapshot_tick = command.m_server.m_anchor_server_tick;
    this->m_server_tick   = frame + delay;
    return true;
}

void auto_det::locate( C_BaseEntity *projectile, const vec3 &launch, f32 gravity, trace_t &trace ) const
{
    const vec3 origin{ projectile->m_vecOrigin( ) };
    const f32  simulated{ projectile->m_flSimulationTime( ) };
    const f32  previous{ projectile->m_flOldSimulationTime( ) };
    const i32  seen{ TIME_TO_TICKS( simulated ) };

    trace          = trace_t{ };
    trace.endpos   = origin;
    trace.fraction = 1.0f;

    if ( this->m_snapshot_tick - seen > REST_TICKS )
    {
        return;
    }

    vec3 velocity{ launch };
    if ( previous > 0.0f && simulated > previous )
    {
        velocity = ( origin - projectile->m_vecOldOrigin( ) ) / ( simulated - previous );
        velocity.z -= gravity * ( simulated - previous ) * 0.5f;
    }

    // a flare we fired thinks after our command in the SimThink list, so it too goes off where the previous tick left it
    const f32 time{ TICKS_TO_TIME( math::bigger( this->m_server_tick - 1 - seen, 0 ) ) };
    vec3      ahead{ origin + velocity * time };
    ahead.z -= 0.5f * gravity * time * time;

    Ray_t                  ray{ };
    CTraceFilterProjectile filter{ };

    filter.m_pSkip         = projectile;
    filter.m_players_block = true;
    filter.m_skip_team     = g_cl.m_local->m_iTeamNum( );

    ray.Init( origin, ahead );
    g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );

    if ( trace.startsolid )
    {
        trace          = trace_t{ };
        trace.endpos   = origin;
        trace.fraction = 1.0f;
    }
}

bool auto_det::fly( C_BaseEntity *pipe, vec3 &position )
{
    const vec3 origin{ pipe->m_vecOrigin( ) };
    const i32  seen{ TIME_TO_TICKS( pipe->m_flSimulationTime( ) ) };

    position = origin;
    if ( this->m_snapshot_tick - seen > REST_TICKS )
    {
        return false;
    }

    // the command runs before its tick's PhysFrame (CPhysicsHook::FrameUpdatePostEntityThink), so a sticky explodes where
    // the previous tick left it on theory..
    const i32 ahead{ math::bigger( this->m_server_tick - 1 - seen, 0 ) };

    const flight_t *flight{ this->replay( pipe, origin, seen ) };
    if ( !flight )
    {
        flight = this->resume( pipe, origin, seen, ahead );
    }

    if ( !flight )
    {
        return false;
    }

    const i32 index{ seen - flight->m_first_tick + ahead };
    const i32 last{ flight->m_count - 1 };

    // VPhysicsCollision sets m_bTouched inside the PhysFrame of the contact, and the sticky stays where IVP left it
    if ( flight->m_landed && index >= last )
    {
        position = flight->m_path[ last ];
        return flight->m_attached;
    }

    position = flight->m_path[ math::smaller( index, last ) ];
    return false;
}

void auto_det::bind( C_BaseEntity *pipe )
{
    const u32 handle{ static_cast< u32 >( pipe->GetRefEHandle( ).ToInt( ) ) };

    for ( const flight_t &flight : this->m_flights )
    {
        if ( flight.m_handle == handle )
        {
            return;
        }
    }

    if ( pipe->As< C_TFGrenadePipebombProjectile * >( )->m_bTouched( ) )
    {
        return;
    }

    const vec3 origin{ pipe->m_vecOrigin( ) };
    const i32  seen{ TIME_TO_TICKS( pipe->m_flSimulationTime( ) ) };

    // stickies show up in the order they were fired, so the oldest free launch goes first
    for ( usize at{ }; at < MAX_FLIGHTS; ++at )
    {
        flight_t &flight{ this->m_flights[ ( this->m_flight_next + at ) % MAX_FLIGHTS ] };
        if ( flight.m_handle != INVALID_EHANDLE_INDEX )
        {
            continue;
        }

        for ( i32 index{ 1 }; index < flight.m_count; ++index )
        {
            if ( networks_as( flight.m_path[ index ], origin ) )
            {
                flight.m_handle     = handle;
                flight.m_first_tick = seen - index;
                return;
            }
        }
    }
}

const auto_det::flight_t *auto_det::replay( C_BaseEntity *pipe, const vec3 &origin, i32 seen ) const
{
    const u32 handle{ static_cast< u32 >( pipe->GetRefEHandle( ).ToInt( ) ) };

    for ( const flight_t &flight : this->m_flights )
    {
        if ( flight.m_handle != handle )
        {
            continue;
        }

        // off it once deflected, pushed by a blast or bounced off a player
        const i32 index{ seen - flight.m_first_tick };
        return index >= 0 && index < flight.m_count && networks_as( flight.m_path[ index ], origin ) ? &flight : nullptr;
    }

    return nullptr;
}

auto_det::flight_t *auto_det::resume( C_BaseEntity *pipe, const vec3 &origin, i32 seen, i32 ahead )
{
    vec3      velocity{ pipe->As< C_TFGrenadePipebombProjectile * >( )->m_vInitialVelocity( ) };
    const f32 simulated{ pipe->m_flSimulationTime( ) };
    const f32 previous{ pipe->m_flOldSimulationTime( ) };
    if ( previous > 0.0f && simulated > previous )
    {
        velocity = ( origin - pipe->m_vecOldOrigin( ) ) / ( simulated - previous );
        velocity.z -= g_tf2.sv_gravity->GetFloat( ) * ( simulated - previous ) * 0.5f;
    }

    projectile_setup_t setup{ };
    if ( !g_proj_sim.resume_sticky( pipe, pipe->m_vecOrigin( ), velocity, vec3{ }, setup ) )
    {
        return nullptr;
    }

    simulate( setup, ahead, this->m_resumed );
    this->m_resumed.m_first_tick = seen;
    return &this->m_resumed;
}

void auto_det::simulate( const projectile_setup_t &setup, i32 ticks, flight_t &flight )
{
    const i32 limit{ math::smaller( ticks, projectile_sim::MAX_PATH_POINTS - 1 ) };
    const i32 interval{ math::clamp( static_cast< i32 >( math::sqrt( static_cast< f32 >( limit ) ) ), 1, 12 ) };
    impact_t  impact{ };

    flight.m_handle    = INVALID_EHANDLE_INDEX;
    flight.m_path[ 0 ] = setup.m_position;
    flight.m_count     = 1 + g_proj_sim.simulate( setup, &flight.m_path[ 1 ], limit, interval, impact );
    flight.m_landed    = impact.m_hit;
    flight.m_attached  = impact.m_attached;
}

void auto_det::collect_blasts( const launcher_t &launcher )
{
    const ETFClassID wanted{ launcher.m_mode == mode_t::stickies ? ETFClassID::CTFGrenadePipebombProjectile
                                                                 : ETFClassID::CTFProjectile_Flare };

    for ( const auto *cached : g_entity_cache.projectiles( ) )
    {
        if ( this->m_blast_count >= MAX_BLASTS )
        {
            break;
        }

        if ( cached->m_class_id != wanted || !cached->networked( ) )
        {
            continue;
        }

        if ( cached->m_entity->IsDormant( ) )
        {
            if ( launcher.m_mode != mode_t::stickies )
            {
                continue;
            }

            if ( cached->m_entity->IsDormant( ) && !g_cfg.m_auto_det.m_dormant )
            {
                continue;
            }
        }

        const bool added{ launcher.m_mode == mode_t::stickies ? this->add_sticky( cached->m_entity, launcher )
                                                              : this->add_flare( cached->m_entity, launcher ) };
        if ( added )
        {
            ++this->m_blast_count;
        }
    }
}

bool auto_det::add_sticky( C_BaseEntity *entity, const launcher_t &launcher )
{
    C_TFGrenadePipebombProjectile *pipe{ entity->As< C_TFGrenadePipebombProjectile * >( ) };
    if ( pipe->m_iType( ) != TF_GL_MODE_REMOTE_DETONATE || pipe->m_hLauncher( ).Get( ) != launcher.m_weapon )
    {
        return false;
    }

    const fired_t *record{ this->fired( pipe, launcher.m_weapon ) };
    const f32      age{ this->m_command_time - ( record ? record->m_time : launcher.m_weapon->m_flLastFireTime( ) ) };
    if ( launcher.m_view_gated ? !( age > launcher.m_live_time ) : age < launcher.m_live_time )
    {
        return false;
    }

    if ( launcher.m_fizzle_time > 0.0f && record && record->m_touched_tick >= 0 &&
         this->m_command_time > TICKS_TO_TIME( record->m_touched_tick ) + launcher.m_fizzle_time )
    {
        return false;
    }

    blast_t &blast{ this->m_blasts[ this->m_blast_count ] };

    blast              = blast_t{ };
    blast.m_projectile = pipe;
    blast.m_position   = pipe->m_vecOrigin( );
    blast.m_radius     = launcher.m_radius;

    const bool touched{ pipe->m_bTouched( ) || this->fly( pipe, blast.m_position ) };

    if ( !touched )
    {
        blast.m_radius = splash::air_radius( blast.m_radius, age );
    }

    return !launcher.m_view_gated || this->in_view( blast );
}

bool auto_det::add_flare( C_BaseEntity *entity, const launcher_t &launcher )
{
    C_TFProjectile_Flare *flare{ entity->As< C_TFProjectile_Flare * >( ) };
    if ( flare->m_hLauncher( ).Get( ) != launcher.m_weapon )
    {
        return false;
    }

    const f32 gravity{ g_tf2.sv_gravity->GetFloat( ) *
                       g_cl.attrib_hook( launcher.m_weapon, FLARE_GRAVITY, ATLAS_XORSTR( "mult_projectile_speed" ).get( ) ) };

    trace_t flight{ };
    this->locate( flare, flare->m_vInitialVelocity( ), gravity, flight );
    if ( flight.fraction < 1.0f )
    {
        return false;
    }

    blast_t &blast{ this->m_blasts[ this->m_blast_count ] };

    blast              = blast_t{ };
    blast.m_projectile = flare;
    blast.m_position   = flight.endpos;
    blast.m_origin     = flight.endpos;
    blast.m_radius     = launcher.m_radius;

    return true;
}

bool auto_det::in_view( const blast_t &blast ) const
{
    C_BaseEntity *pipe{ blast.m_projectile };
    if ( g_cl.m_local->GetAbsOrigin( ).dist_to( blast.m_position ) < blast.m_radius )
    {
        return true;
    }

    vec3 to_pipe{ blast.m_position + ( pipe->WorldSpaceCenter( ) - pipe->GetAbsOrigin( ) ) -
                  g_cl.m_local->Weapon_ShootPosition( ) };
    vec3 forward{ };

    to_pipe.normalize( );
    math::angle_vectors( g_cl.m_cmd->viewangles, &forward );

    return to_pipe.dot( forward ) > 0.975f;
}

void auto_det::collect_targets( )
{
    const vec3 origin{ g_cl.m_local->GetAbsOrigin( ) };
    const vec3 step{ g_cl.m_local->GetAbsVelocity( ) * TICK_INTERVAL };

    this->m_self          = target_t{ };
    this->m_self.m_entity = g_cl.m_local;
    this->m_self.m_player = g_cl.m_local;
    this->m_self.m_origin = origin;
    this->m_self.m_mins   = origin + g_cl.m_local->m_vecMins( ) + math::lower( step, vec3{ } );
    this->m_self.m_maxs   = origin + g_cl.m_local->m_vecMaxs( ) + math::upper( step, vec3{ } );
    this->m_self.m_spot   = g_cl.m_local->Weapon_ShootPosition( );

    const i32 team{ g_cl.m_local->m_iTeamNum( ) };

    for ( const auto *cached : g_entity_cache.players( ) )
    {
        if ( this->m_target_count >= MAX_TARGETS )
        {
            return;
        }

        C_TFPlayer *player{ cached->m_entity->As< C_TFPlayer * >( ) };
        if ( player == g_cl.m_local )
        {
            continue;
        }

        if ( player->IsDormant( ) )
        {
            if ( g_cfg.m_auto_det.m_dormant )
            {
                this->add_dormant_player( player );
            }

            continue;
        }

        if ( player->m_iTeamNum( ) == team || !player->IsAlive( ) || player->IsGhost( ) || player->IsUbered( ) ||
             player->IsBonked( ) )
        {
            continue;
        }

        this->add_player( player );
    }

    if ( !g_cfg.m_auto_det.m_target_buildings )
    {
        return;
    }

    for ( const auto *cached : g_entity_cache.buildings( ) )
    {
        if ( this->m_target_count >= MAX_TARGETS )
        {
            return;
        }

        C_BaseEntity *building{ cached->m_entity };
        vec3          position{ building->m_vecOrigin( ) };

        if ( building->IsDormant( ) )
        {
            f32 remaining{ };
            if ( !g_cfg.m_auto_det.m_dormant || !g_player_data.dormant_origin( building, position, remaining ) )
            {
                continue;
            }
        }

        if ( building->m_iTeamNum( ) == team || !targetable_building( building ) )
        {
            continue;
        }

        const vec3 mins{ position + building->m_vecMins( ) };
        const vec3 maxs{ position + building->m_vecMaxs( ) };
        if ( !this->in_reach( mins, maxs, 0.0f ) )
        {
            continue;
        }

        target_t &target{ this->m_targets[ this->m_target_count++ ] };

        target          = target_t{ };
        target.m_entity = building;
        target.m_origin = position;
        target.m_mins   = mins;
        target.m_maxs   = maxs;
        target.m_spot   = building->WorldSpaceCenter( );
    }
}

void auto_det::add_player( C_TFPlayer *player )
{
    const i32  ticks{ g_ragebot.target_catch_up_ticks( player, 0 ) };
    const vec3 origin{ player->m_vecOrigin( ) };
    const f32  speed{ math::bigger( player->m_flMaxspeed( ), player->GetAbsVelocity( ).length( ) ) };
    if ( !this->in_reach( origin + player->m_vecMins( ), origin + player->m_vecMaxs( ), speed * TICKS_TO_TIME( ticks ) ) )
    {
        return;
    }

    move_sim_ctx_t ctx{ player };
    target_t      &target{ this->m_targets[ this->m_target_count++ ] };

    target = target_t{ };

    for ( i32 tick{ }; tick < ticks; ++tick )
    {
        if ( tick == ticks - 1 )
        {
            target.m_previous = splash_target_t{ player, ctx.m_pred_origin, ctx.m_pred_origin + ctx.m_pred_mins,
                                                 ctx.m_pred_origin + ctx.m_pred_maxs,
                                                 splash::player_eye( player, ctx.m_pred_origin, ctx.m_pred_flags ) };
        }

        if ( !g_move_sim.run_tick( &ctx ) )
        {
            break;
        }
    }

    target.m_entity = player;
    target.m_player = player;
    target.m_origin = ctx.m_pred_origin;
    target.m_mins   = ctx.m_pred_origin + ctx.m_pred_mins;
    target.m_maxs   = ctx.m_pred_origin + ctx.m_pred_maxs;
    target.m_spot   = splash::player_eye( player, ctx.m_pred_origin, ctx.m_pred_flags );
}

void auto_det::add_dormant_player( C_TFPlayer *player )
{
    vec3 origin{ };
    f32  remaining{ };
    if ( !g_player_data.dormant_origin( player, origin, remaining ) ||
         g_player_data.team( player->entindex( ) ) == g_cl.m_local->m_iTeamNum( ) )
    {
        return;
    }

    if ( !this->in_reach( origin + player->m_vecMins( ), origin + player->m_vecMaxs( ), 0.0f ) )
    {
        return;
    }

    target_t &target{ this->m_targets[ this->m_target_count++ ] };

    target          = target_t{ };
    target.m_entity = player;
    target.m_player = player;
    target.m_origin = origin;
    target.m_mins   = origin + player->m_vecMins( );
    target.m_maxs   = origin + player->m_vecMaxs( );
    target.m_spot   = splash::player_eye( player, origin, player->m_fFlags( ) );
}

bool auto_det::in_reach( const vec3 &mins, const vec3 &maxs, f32 slack ) const
{
    const vec3 low{ mins - vec3( slack, slack, slack + 8.0f ) };
    const vec3 high{ maxs + vec3( slack, slack, slack + 24.0f ) };

    for ( usize index{ }; index < this->m_blast_count; ++index )
    {
        const blast_t &blast{ this->m_blasts[ index ] };
        if ( blast.m_position.dist_to_sqr( math::nearest_on_box( low, high, blast.m_position ) ) <=
             blast.m_radius * blast.m_radius )
        {
            return true;
        }
    }

    return false;
}

void auto_det::place( blast_t &blast ) const
{
    const vec3 start{ blast.m_position + vec3( 0.0f, 0.0f, 8.0f ) };
    const vec3 end{ start - vec3( 0.0f, 0.0f, 32.0f ) };

    trace_t              trace{ };
    Ray_t                ray{ };
    CTraceFilterDetonate filter{ blast.m_projectile };

    ray.Init( start, end );
    g_tf2.m_trace->TraceRay( ray, MASK_SHOT_HULL, &filter, &trace );

    f32  fraction{ trace.fraction };
    vec3 normal{ trace.plane.normal };

    const auto clip{ [ & ]( const target_t &target )
    {
        const f32 hit{ math::segment_box_fraction( start, end, target.m_mins, target.m_maxs ) };
        if ( hit < fraction )
        {
            fraction = hit;
            normal   = hit > 0.0f ? vec3( 0.0f, 0.0f, 1.0f ) : vec3{ };
        }
    } };

    clip( this->m_self );

    for ( usize index{ }; index < this->m_target_count; ++index )
    {
        if ( this->m_targets[ index ].m_player )
        {
            clip( this->m_targets[ index ] );
        }
    }

    blast.m_origin = fraction != 1.0f ? start + ( end - start ) * fraction + normal : blast.m_position;
}

bool auto_det::hit( const target_t &target, f32 scale ) const
{
    const splash_target_t splash{ target.m_entity, target.m_origin, target.m_mins, target.m_maxs, target.m_spot };

    return this->reached( splash, scale ) && ( !target.m_previous.m_entity || this->reached( target.m_previous, scale ) );
}

bool auto_det::reached( const splash_target_t &target, f32 scale ) const
{
    for ( usize index{ }; index < this->m_blast_count; ++index )
    {
        const blast_t &blast{ this->m_blasts[ index ] };
        if ( splash::reaches( blast.m_origin, blast.m_radius * scale, target ) )
        {
            return true;
        }
    }

    return false;
}

bool auto_det::hurts_self( const launcher_t &launcher ) const
{
    if ( g_cl.m_local->IsUbered( ) || g_cl.m_local->IsBonked( ) ||
         g_cl.attrib_hook( launcher.m_weapon, 0.0f, ATLAS_XORSTR( "no_self_blast_dmg" ).get( ) ) != 0.0f )
    {
        return false;
    }

    const splash_target_t self{ g_cl.m_local, this->m_self.m_origin, this->m_self.m_mins, this->m_self.m_maxs,
                                this->m_self.m_spot };

    for ( usize index{ }; index < this->m_blast_count; ++index )
    {
        if ( splash::reaches( this->m_blasts[ index ].m_origin, launcher.m_self_radius, self ) )
        {
            return true;
        }
    }

    return false;
}
