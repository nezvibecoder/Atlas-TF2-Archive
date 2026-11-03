#include "air_blast.hpp"
#include "entity_cache.hpp"

#include "gui/gui.hpp"
#include "lagcomp.hpp"
#include "move_sim.hpp"
#include "networking.hpp"
#include "ragebot.hpp"
#include "splash.hpp"
#include "tickbase.hpp"

ATLAS_NO_DESTROY air_blast g_air_blast{ };

constexpr float DEFLECT_RADIUS{ 128.0f };
constexpr float BURST_AMMO{ 20.0f }; // tf_flamethrower_burstammo
constexpr float PUSH_CONE{ 35.0f };  // tf_flamethrower_airblast_cone_angle
constexpr float REDIRECT_HORIZON{ 1.0f };
constexpr i32   LEAD_SLACK{ 1 };
constexpr i32   LAST_CALL{ 2 };
constexpr i32   REST_TICKS{ 4 };

class CTraceFilterDeflection : public CTraceFilterSimple
{
public:
    CTraceFilterDeflection( ) : CTraceFilterSimple( g_cl.m_local, COLLISION_GROUP_NONE )
    {
    }

    bool ShouldHitEntity( IHandleEntity *handle, i32 mask ) override
    {
        IClientEntity *client{ g_tf2.m_entity_list->GetClientEntityFromHandle( handle->GetRefEHandle( ) ) };
        if ( client )
        {
            switch ( client->GetBaseEntity( )->GetClientClass( )->GetClassID( ) )
            {
                case ETFClassID::CTFPlayer:
                case ETFClassID::CObjectSentrygun:
                case ETFClassID::CObjectDispenser:
                case ETFClassID::CObjectTeleporter:
                case ETFClassID::CObjectSapper:
                case ETFClassID::CObjectCartDispenser: return false;
                default: break;
            }
        }
        return CTraceFilterSimple::ShouldHitEntity( handle, mask );
    }
};

ATLAS_FORCE_INLINE bool can_blast( )
{
    const i32 id{ g_cl.m_weapon->GetWeaponID( ) };
    if ( id != TF_WEAPON_FLAMETHROWER && id != TF_WEAPON_FLAME_BALL )
    {
        return false;
    }

    if ( g_cl.m_local->IsTaunting( ) || g_cl.m_weapon->m_bLowered( ) ||
         g_cl.m_local->m_nWaterLevel( ) == 3 || // WL_Eyes
         g_cl.m_weapon->m_flNextSecondaryAttack( ) > g_tf2.m_globals->curtime ||
         ( id == TF_WEAPON_FLAME_BALL && g_cl.m_local->m_flItemChargeMeter( ) < 100.0f ) ) // HasFullCharge
    {
        return false;
    }

    // the Phlogistinator, whose secondary is the rage
    if ( g_cl.attrib_hook( g_cl.m_weapon, 0.0f, ATLAS_XORSTR( "airblast_disabled" ).get( ) ) != 0.0f )
    {
        return false;
    }

    const i32 cost{ static_cast< i32 >(
        g_cl.attrib_hook( g_cl.m_weapon, BURST_AMMO, ATLAS_XORSTR( "mult_airblast_cost" ).get( ) ) ) };

    return ( &g_cl.m_local->m_iAmmo( ) )[ g_cl.m_weapon->m_iPrimaryAmmoType( ) ] >= cost;
}

ATLAS_FORCE_INLINE bool spent( C_BaseEntity *projectile, ETFClassID class_id )
{
    C_TFGrenadePipebombProjectile *pipe{ projectile->As< C_TFGrenadePipebombProjectile * >( ) };
    if ( class_id != ETFClassID::CTFGrenadePipebombProjectile )
    {
        return pipe->m_bTouched( );
    }

    switch ( pipe->m_iType( ) )
    {
        case TF_GL_MODE_REMOTE_DETONATE: return pipe->m_bTouched( );
        case TF_GL_MODE_REMOTE_DETONATE_PRACTICE: return true; // the sticky jumper
        default: return false;
    }
}

ATLAS_FORCE_INLINE bool critical( C_BaseEntity *projectile, ETFClassID class_id )
{
    switch ( class_id )
    {
        case ETFClassID::CTFProjectile_Rocket:
        case ETFClassID::CTFProjectile_SentryRocket:
        case ETFClassID::CTFProjectile_BallOfFire:
        case ETFClassID::CTFProjectile_MechanicalArmOrb: return projectile->As< C_TFProjectile_Rocket * >( )->m_bCritical( );
        case ETFClassID::CTFProjectile_Flare: return projectile->As< C_TFProjectile_Flare * >( )->m_bCritical( );
        case ETFClassID::CTFProjectile_Arrow:
        case ETFClassID::CTFProjectile_HealingBolt: return projectile->As< C_TFProjectile_Arrow * >( )->m_bCritical( );
        case ETFClassID::CTFProjectile_EnergyBall: return false;
        default: return projectile->As< C_TFGrenadePipebombProjectile * >( )->m_bCritical( );
    }
}

// walks the player to the server frame of the command, then on until a projectile leaving source at that
// speed has had the time to reach them
ATLAS_FORCE_INLINE bool intercept( C_TFPlayer *player, const vec3 &source, float speed, vec3 &out )
{
    const i32 catch_up{ g_ragebot.target_catch_up_ticks( player, 1 ) };
    const i32 horizon{ catch_up + TIME_TO_TICKS( REDIRECT_HORIZON ) };

    move_sim_ctx_t ctx{ player };

    for ( i32 tick{ 1 }; tick <= horizon; ++tick )
    {
        if ( !g_move_sim.run_tick( &ctx ) )
        {
            return false;
        }

        out = ctx.m_pred_origin + ( ctx.m_pred_mins + ctx.m_pred_maxs ) * 0.5f;

        if ( tick > catch_up && out.dist_to( source ) <= speed * TICKS_TO_TIME( tick - catch_up ) )
        {
            return true;
        }
    }

    return false;
}

// CTFPlayerShared::IsImmuneToPushback, after the mode's own pick
ATLAS_FORCE_INLINE bool pushable( C_TFPlayer *player )
{
    switch ( g_cfg.m_air_blast.m_push )
    {
        case config::air_blast_t::PUSH_ALL: break;
        case config::air_blast_t::PUSH_MELEE:
        {
            C_BaseEntity *weapon{ player->m_hActiveWeapon( ).Get( ) };
            if ( !weapon || weapon->As< C_TFWeaponBase * >( )->GetSlot( ) != 2 )
            {
                return false;
            }

            break;
        }
        default: return false;
    }

    return !player->InCond( TF_COND_MEGAHEAL ) && !player->InCond( TF_COND_IMMUNE_TO_PUSHBACK );
}

ATLAS_FORCE_INLINE void stand( C_TFPlayer *player, bool teammate, vec3 &center, vec3 &eye )
{
    if ( !teammate )
    {
        center = player->m_vecOrigin( ) + ( player->m_vecMins( ) + player->m_vecMaxs( ) ) * 0.5f;
        eye    = splash::player_eye( player, player->m_vecOrigin( ), player->m_fFlags( ) );
        return;
    }

    const i32      ticks{ g_ragebot.target_catch_up_ticks( player, g_cfg.m_air_blast.m_aim ? 1 : 0 ) };
    move_sim_ctx_t ctx{ player };

    for ( i32 tick{ }; tick < ticks; ++tick )
    {
        if ( !g_move_sim.run_tick( &ctx ) )
        {
            break;
        }
    }

    center = ctx.m_pred_origin + ( ctx.m_pred_mins + ctx.m_pred_maxs ) * 0.5f;
    eye    = splash::player_eye( player, ctx.m_pred_origin, ctx.m_pred_flags );
}

ATLAS_FORCE_INLINE bool in_push_cone( const vec3 &to_target, const vec3 &angles )
{
    vec3 forward{ };
    math::angle_vectors( angles, &forward );
    return to_target.normalized( ).dot( forward ) >= math::cos( DEG2RAD( PUSH_CONE * 0.5f ) );
}

void air_blast::run( )
{
#if defined( ATLAS_DEV )
    this->grade( );
#endif

    config::air_blast_t &cfg{ g_cfg.m_air_blast };
    if ( !cfg.m_enabled || g_gui.open( ) || !can_blast( ) )
    {
        return;
    }

    if ( !gui::bind_active( cfg.m_key ) )
    {
        return;
    }

    // an aimed command is held one tick for the psilent
    const i32 delay{ g_networking.send_delay( cfg.m_aim ? 1 : 0 ).ticks( ) };
    const i32 frame{ g_tickbase.send_frame( ) };
    if ( frame < 0 )
    {
        return;
    }

    this->m_snapshot_tick = g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick;
    this->m_command_tick  = frame + delay;
    this->m_eye           = g_cl.m_local->Weapon_ShootPosition( );
    this->m_radius        = g_cl.attrib_hook( g_cl.m_weapon, DEFLECT_RADIUS,
                                              ATLAS_XORSTR( "deflection_size_multiplier" ).get( ) );

    const vec3 view{ g_cl.m_cmd->viewangles };
    vec3       angles{ view };
    threat_t   threat{ };

    if ( this->find_projectile( threat, angles ) )
    {
        // only the rocket family leaves the blast on a line straight enough to aim
        if ( cfg.m_aim && cfg.m_redirect && threat.m_family == family_t::rocket &&
             !this->redirect( threat, angles ) )
        {
            return;
        }
    } else if ( !this->find_player( angles ) ) // players only get the blast when no projectile is in reach
    {
        return;
    }

    g_cl.m_cmd->buttons |= IN_ATTACK2;

    if ( angles == view )
    {
        g_cl.m_send_packet = true;
        return;
    }

    g_cl.m_cmd->viewangles = angles;
    g_cl.m_psilent_choked  = true;
}

// server IsDeflectable overrides; the spell projectiles and the Bison ring return false
air_blast::family_t air_blast::family_of( ETFClassID class_id )
{
    switch ( class_id )
    {
        case ETFClassID::CTFProjectile_Rocket:
        case ETFClassID::CTFProjectile_SentryRocket:
        case ETFClassID::CTFProjectile_BallOfFire:
        case ETFClassID::CTFProjectile_EnergyBall:
        case ETFClassID::CTFProjectile_MechanicalArmOrb:
        case ETFClassID::CTFProjectile_Flare:
        case ETFClassID::CTFProjectile_Arrow:
        case ETFClassID::CTFProjectile_HealingBolt: return family_t::rocket;
        case ETFClassID::CTFGrenadePipebombProjectile:
        case ETFClassID::CTFStunBall:
        case ETFClassID::CTFProjectile_Cleaver:
        case ETFClassID::CTFProjectile_Jar:
        case ETFClassID::CTFProjectile_JarMilk:
        case ETFClassID::CTFProjectile_JarGas: return family_t::grenade;
        default: return family_t::none;
    }
}

vec3 air_blast::velocity_of( C_BaseEntity *projectile, family_t family )
{
    const float previous{ projectile->m_flOldSimulationTime( ) };
    if ( previous > 0.0f )
    {
        const float elapsed{ projectile->m_flSimulationTime( ) - previous };
        return elapsed > 0.0f ? ( projectile->m_vecOrigin( ) - projectile->m_vecOldOrigin( ) ) / elapsed
                              : vec3{ };
    }

    return family == family_t::grenade
               ? projectile->As< C_TFGrenadePipebombProjectile * >( )->m_vInitialVelocity( )
               : projectile->As< C_TFProjectile_Rocket * >( )->m_vInitialVelocity( );
}

bool air_blast::find_projectile( threat_t &best, vec3 &angles ) const
{
    const vec3 view{ angles };
    float      best_distance{ FLT_MAX };

    for ( const auto *cached : g_entity_cache.projectiles( ) )
    {
        threat_t threat{ };
        if ( !cached->networked( ) || !this->locate( cached->m_entity, cached->m_class_id, threat ) )
        {
            continue;
        }

        // a crit outranks whatever is nearer
        const float distance{ threat.m_to.dist_to_sqr( this->m_eye ) };
        if ( threat.m_critical != best.m_critical ? best.m_critical : distance >= best_distance )
        {
            continue;
        }

        vec3 aim{ view };
        if ( !this->aim_for( threat.m_from, threat.m_to, aim ) )
        {
            continue;
        }

        best          = threat;
        angles        = aim;
        best_distance = distance;
    }

    if ( !best.m_projectile )
    {
        return false;
    }

    DBGOUT( "air blast: projectile, lead %d ticks, %.0f units out%s",
            this->m_command_tick - TIME_TO_TICKS( best.m_projectile->m_flSimulationTime( ) ),
            math::sqrt( best_distance ), best.m_critical ? ", crit" : "" );

    return true;
}

bool air_blast::find_player( vec3 &angles ) const
{
    config::air_blast_t &cfg{ g_cfg.m_air_blast };
    if ( !cfg.m_extinguish && cfg.m_push == config::air_blast_t::PUSH_OFF )
    {
        return false;
    }

    const vec3  view{ angles };
    const float reach{ this->m_radius * 3.0f }; // twice the radius, and room for a teammate to walk into it
    C_TFPlayer *nearest{ };
    float       nearest_distance{ FLT_MAX };

    for ( const auto *cached : g_entity_cache.players( ) )
    {
        C_TFPlayer *player{ cached->m_entity->As< C_TFPlayer * >( ) };
        if ( player == g_cl.m_local || player->IsDormant( ) || !player->IsAlive( ) || player->IsGhost( ) ||
             player->m_vecOrigin( ).dist_to_sqr( this->m_eye ) > reach * reach )
        {
            continue;
        }

        const bool teammate{ player->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) };
        if ( teammate ? !cfg.m_extinguish || !player->InCond( TF_COND_BURNING ) : !pushable( player ) )
        {
            continue;
        }

        vec3 center{ };
        vec3 eye{ };
        stand( player, teammate, center, eye );

        const float distance{ center.dist_to_sqr( this->m_eye ) };
        if ( distance >= nearest_distance )
        {
            continue;
        }

        vec3 aim{ view };
        if ( !this->aim_for( center, center, aim ) )
        {
            continue;
        }

        if ( !teammate && !in_push_cone( center - this->m_eye, aim ) )
        {
            if ( !cfg.m_aim )
            {
                continue;
            }

            math::vector_angles( center - this->m_eye, aim );
        }

        // CBaseEntity::FVisible runs eye to eye
        if ( !g_cl.trace_line( g_cl.m_local, player, this->m_eye, eye, MASK_SOLID ) )
        {
            continue;
        }

        nearest          = player;
        angles           = aim;
        nearest_distance = distance;
    }

    if ( !nearest )
    {
        return false;
    }

    const bool push{ nearest->m_iTeamNum( ) != g_cl.m_local->m_iTeamNum( ) };
    if ( push )
    {
        g_cl.m_cmd->tick_count = TIME_TO_TICKS( nearest->m_flSimulationTime( ) + lag_compensation::lerp_time( ) );
    }

    DBGOUT( "air blast: %s, %.0f units out", push ? "push" : "extinguish", math::sqrt( nearest_distance ) );

    return true;
}

bool air_blast::locate( C_BaseEntity *projectile, ETFClassID class_id, threat_t &out ) const
{
    const family_t family{ family_of( class_id ) };
    if ( family == family_t::none || projectile->IsDormant( ) ||
         projectile->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) ||
         projectile->m_MoveType( ) == MOVETYPE_NONE ||
         ( family == family_t::grenade && spent( projectile, class_id ) ) )
    {
        return false;
    }

    const vec3 origin{ projectile->m_vecOrigin( ) };
    const i32  seen{ TIME_TO_TICKS( projectile->m_flSimulationTime( ) ) };
    const vec3 velocity{ this->m_snapshot_tick - seen > REST_TICKS ? vec3{ } : velocity_of( projectile, family ) };
    const i32  lead{ this->m_command_tick - seen };

    out.m_projectile = projectile;
    out.m_family     = family;
    out.m_heading    = velocity.normalized( );
    out.m_speed      = velocity.length( );
    out.m_critical   = g_cfg.m_air_blast.m_prefer_crits && critical( projectile, class_id );
    out.m_from       = origin + velocity * TICKS_TO_TIME( math::bigger( lead - 1 - LEAD_SLACK, 0 ) );
    out.m_to         = origin + velocity * TICKS_TO_TIME( lead + LEAD_SLACK );

    const float reach{ this->m_radius * 2.0f };
    if ( out.m_from.dist_to_sqr( this->m_eye ) > reach * reach )
    {
        return false;
    }

    trace_t                trace{ };
    Ray_t                  ray{ };
    CTraceFilterProjectile filter{ };

    filter.m_pSkip         = projectile;
    filter.m_players_block = true;
    filter.m_skip_team     = projectile->m_iTeamNum( );

    ray.Init( origin, out.m_from + out.m_heading * ( reach * 2.0f ) );
    g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );

    out.m_runway = FLT_MAX;

    if ( trace.fraction < 1.0f )
    {
        out.m_runway = ( trace.endpos - out.m_from ).dot( out.m_heading );

        if ( out.m_runway < 0.0f )
        {
            return false;
        }

        if ( out.m_runway < out.m_from.dist_to( out.m_to ) )
        {
            out.m_to = trace.endpos;
        }
    }

    return g_cl.trace_line( g_cl.m_local, projectile, this->m_eye, out.m_from, MASK_SOLID ) &&
           g_cl.trace_line( g_cl.m_local, projectile, this->m_eye, out.m_to, MASK_SOLID );
}

bool air_blast::covers( const vec3 &from, const vec3 &to, const vec3 &angles ) const
{
    vec3 forward{ };
    math::angle_vectors( angles, &forward );

    const vec3  center{ this->m_eye + forward * this->m_radius };
    const float radius_sqr{ this->m_radius * this->m_radius };

    return from.dist_to_sqr( center ) <= radius_sqr && to.dist_to_sqr( center ) <= radius_sqr;
}

bool air_blast::aim_for( const vec3 &from, const vec3 &to, vec3 &angles ) const
{
    if ( this->covers( from, to, angles ) )
    {
        return true;
    }

    math::vector_angles( ( from + to ) * 0.5f - this->m_eye, angles );
    return g_cfg.m_air_blast.m_aim && this->covers( from, to, angles );
}

bool air_blast::will_cover( const threat_t &threat, const vec3 &angles ) const
{
    vec3 forward{ };
    math::angle_vectors( angles, &forward );

    const vec3  to_center{ this->m_eye + forward * this->m_radius - threat.m_from };
    const float middle{ to_center.dot( threat.m_heading ) };
    const float half_sqr{ this->m_radius * this->m_radius - to_center.length_sqr( ) + middle * middle };
    if ( half_sqr <= 0.0f )
    {
        return false;
    }

    const float half{ math::sqrt( half_sqr ) };
    const float window{ threat.m_from.dist_to( threat.m_to ) };
    const float spare{ threat.m_speed * TICKS_TO_TIME( LAST_CALL ) };

    return math::bigger( middle - half, 0.0f ) + window <= math::smaller( middle + half, threat.m_runway - spare );
}

bool air_blast::redirect( const threat_t &threat, vec3 &angles )
{
    const vec3 source{ ( threat.m_from + threat.m_to ) * 0.5f };
    float      shortest{ FLT_MAX };
    bool       waiting{ };

    for ( const auto *cached : g_entity_cache.players( ) )
    {
        C_TFPlayer *player{ cached->m_entity->As< C_TFPlayer * >( ) };
        if ( player->IsDormant( ) || player->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) ||
             !player->IsAlive( ) || player->IsGhost( ) || player->IsUbered( ) || player->IsBonked( ) )
        {
            continue;
        }

        vec3 target{ };
        if ( !intercept( player, source, threat.m_speed, target ) )
        {
            continue;
        }

        const float distance{ target.dist_to_sqr( source ) };
        if ( distance >= shortest )
        {
            continue;
        }

        trace_t                trace{ };
        Ray_t                  ray{ };
        CTraceFilterDeflection filter{ };

        ray.Init( source, source + ( target - source ).normalized( ) * MAX_TRACE_LENGTH );
        g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );

        const vec3 landing{ trace.endpos };
        if ( landing.dist_to_sqr( source ) < distance )
        {
            continue;
        }

        ray.Init( this->m_eye, landing );
        g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );
        if ( trace.endpos.dist_to_sqr( landing ) > 1.0f )
        {
            continue;
        }

        vec3 aim{ };
        math::vector_angles( landing - this->m_eye, aim );
        if ( !this->covers( threat.m_from, threat.m_to, aim ) )
        {
            waiting |= this->will_cover( threat, aim );
            continue;
        }

        angles   = aim;
        shortest = distance;

#if defined( ATLAS_DEV )
        this->m_claim = claim_t{ g_entity_cache.find( threat.m_projectile )->m_handle, target };
#endif
    }

    const bool aimed{ shortest != FLT_MAX };

    DBGOUT( "air blast: redirect %s", aimed ? "aimed" : ( waiting ? "waits" : "has no line" ) );

    return aimed || !waiting;
}

#if defined( ATLAS_DEV )
void air_blast::grade( )
{
    claim_t      &claim{ this->m_claim };
    C_BaseEntity *projectile{ g_entity_cache.resolve( claim.m_projectile ) };

    if ( projectile )
    {
        if ( projectile->m_iTeamNum( ) != g_cl.m_local->m_iTeamNum( ) )
        {
            return;
        }

        if ( !claim.m_turned )
        {
            claim.m_turned = true;
            claim.m_start  = projectile->m_vecOrigin( );
        }

        claim.m_last = projectile->m_vecOrigin( );
        return;
    }

    if ( claim.m_projectile == INVALID_EHANDLE_INDEX )
    {
        return;
    }

    const vec3  flight{ claim.m_last - claim.m_start };
    const vec3  to_point{ claim.m_aim_point - claim.m_start };
    const vec3  heading{ flight.normalized( ) };
    const float miss{ ( to_point - heading * to_point.dot( heading ) ).length( ) };

    if ( claim.m_turned )
    {
        DBGOUT( "air blast: redirect flew %.0f units, its line passes %.0f units from the aimed point, %.0f out",
                flight.length( ), miss, to_point.length( ) );
    } else
    {
        DBGOUT( "air blast: the redirect blast did not turn the projectile" );
    }

    claim = claim_t{ };
}
#endif
