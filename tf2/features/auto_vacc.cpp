#include "auto_vacc.hpp"

#include "damage.hpp"
#include "steam.hpp"

#include "entity_cache.hpp"
#include "move_sim.hpp"
#include "networking.hpp"
#include "party_network.hpp"
#include "player_cloud.hpp"
#include "player_data.hpp"
#include "ragebot.hpp"
#include "splash.hpp"
#include "tickbase.hpp"

ATLAS_NO_DESTROY auto_vacc g_auto_vacc{ };

constexpr f32 CHARGE_CHUNK{ 1.0f / 4.0f };    // 1 / weapon_medigun_resist_num_chunks, a development-only cvar
constexpr f32 SENTRY_ROCKET_DAMAGE{ 100.0f }; // CObjectSentrygun::FireRocket
constexpr f32 SENTRY_DAMAGE{ 16.0f };         // tf_sentrygun_damage
constexpr f32 MINI_SENTRY_DAMAGE{ 8.0f };     // tf_sentrygun_mini_damage
constexpr f32 STICKY_SPREAD_AGE{ 5.0f };      // CTFGrenadePipebombProjectile::GetDamageType
constexpr f32 BOW_MIN_SPEED{ 1800.0f };       // CTFCompoundBow::GetProjectileSpeed
constexpr f32 BOW_MAX_SPEED{ 2600.0f };
constexpr f32 BOW_BASE_DAMAGE{ 50.0f }; // CTFCompoundBow::GetProjectileDamage
constexpr f32 BOW_MIN_GRAVITY{ 0.1f };  // CTFCompoundBow::GetProjectileGravity
constexpr f32 BOW_MAX_GRAVITY{ 0.5f };
// half a step of m_angEyeAngles[0] (8 bits over -90..90), the coarser of the two networked eye angles (yaw has 10 bits
// over 360); at a third of a degree the tangent is the angle itself
constexpr f32 EYE_ANGLE_HALF_STEP{ DEG2RAD( 0.5f * 180.0f / 255.0f ) };

constexpr ETFCond UBER_CONDS[ MEDIGUN_NUM_RESISTS ]{
    TF_COND_MEDIGUN_UBER_BULLET_RESIST,
    TF_COND_MEDIGUN_UBER_BLAST_RESIST,
    TF_COND_MEDIGUN_UBER_FIRE_RESIST,
};

ATLAS_FORCE_INLINE bool is_hitscan( i32 weapon_id )
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
        case TF_WEAPON_SNIPERRIFLE:
        case TF_WEAPON_SNIPERRIFLE_DECAP:
        case TF_WEAPON_SNIPERRIFLE_CLASSIC:
        case TF_WEAPON_MINIGUN:
        case TF_WEAPON_SMG:
        case TF_WEAPON_CHARGED_SMG:
        case TF_WEAPON_PISTOL:
        case TF_WEAPON_PISTOL_SCOUT:
        case TF_WEAPON_REVOLVER:
        case TF_WEAPON_FLAMETHROWER: return true;
        default: return false;
    }
}

ATLAS_FORCE_INLINE bool is_sniper_rifle( i32 weapon_id )
{
    switch ( weapon_id )
    {
        case TF_WEAPON_SNIPERRIFLE:
        case TF_WEAPON_SNIPERRIFLE_DECAP:
        case TF_WEAPON_SNIPERRIFLE_CLASSIC: return true;
        default: return false;
    }
}

// how far a flame particle travels before it dies, stepped like proj_sim's flame movetype
ATLAS_FORCE_INLINE f32 flame_reach( C_TFWeaponBase *weapon )
{
    const f32 drag{ g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "flame_drag" ).get( ) ) };
    const f32 keep{ math::bigger( 1.0f - drag * TICK_INTERVAL, 0.0f ) };
    const i32 ticks{ TIME_TO_TICKS( g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "flame_lifetime" ).get( ) ) ) };

    f32 step{ g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "flame_speed" ).get( ) ) * TICK_INTERVAL };
    f32 reach{ };
    for ( i32 tick{ }; tick < ticks; ++tick )
    {
        reach += step;
        step *= keep;
    }

    return reach;
}

// what a sniper rifle's shot deals at the given server time (CTFSniperRifle::GetProjectileDamage, before mult_dmg): its
// charge only goes to the sniper (DT_SniperRifleLocalData), but his dot networks to everyone when the charge started
// (CSniperDot::m_flChargeStartTime, reset on every zoom and shot) and ItemPostFrame grows it from there at the rifle's rate
ATLAS_FORCE_INLINE f32 sniper_damage( C_TFPlayer *player, C_TFWeaponBase *weapon, f32 time )
{
    for ( const auto *cached : g_entity_cache.networked( ) )
    {
        if ( cached->m_class_id != ETFClassID::CSniperDot || cached->m_entity->m_hOwnerEntity( ).Get( ) != player )
        {
            continue;
        }

        const f32 rate{ math::clamp( g_cl.attrib_hook( weapon, TF_WEAPON_SNIPERRIFLE_CHARGE_PER_SEC, ATLAS_XORSTR( "mult_sniper_charge_per_sec" ).get( ) ),
                                     0.0f, 2.0f * TF_WEAPON_SNIPERRIFLE_CHARGE_PER_SEC ) };
        const f32 charge{ ( time - cached->m_entity->As< C_SniperDot * >( )->m_flChargeStartTime( ) ) * rate };
        return math::clamp( charge, TF_WEAPON_SNIPERRIFLE_DAMAGE_MIN, TF_WEAPON_SNIPERRIFLE_DAMAGE_MAX );
    }

    return TF_WEAPON_SNIPERRIFLE_DAMAGE_MIN;
}

ATLAS_FORCE_INLINE i32 ticks_until( f32 time )
{
    const i32 ticks{ static_cast< i32 >( time / TICK_INTERVAL ) };
    return TICKS_TO_TIME( ticks ) < time ? ticks + 1 : ticks;
}

ATLAS_FORCE_INLINE bool medic_healing_local( u32 account )
{
    if ( !g_cl.m_local || !g_cl.m_local->IsAlive( ) )
    {
        return false;
    }

    for ( i32 index{ 1 }; index <= g_tf2.m_engine->GetMaxClients( ); ++index )
    {
        player_info_t info{ };
        if ( !g_tf2.m_engine->GetPlayerInfo( index, &info ) || info.friendsID != account )
        {
            continue;
        }

        IClientEntity *entity{ g_tf2.m_entity_list->GetClientEntity( index ) };
        if ( !entity || entity->IsDormant( ) )
        {
            return false;
        }

        ClientClass *client_class{ entity->GetClientClass( ) };
        if ( !client_class || client_class->GetClassID( ) != ETFClassID::CTFPlayer )
        {
            return false;
        }

        C_TFPlayer *medic{ entity->As< C_TFPlayer * >( ) };
        if ( !medic->IsAlive( ) || medic->m_iClass( ) != TF_CLASS_MEDIC )
        {
            return false;
        }

        C_BaseEntity *weapon{ medic->m_hActiveWeapon( ).Get( ) };
        if ( !weapon || weapon->As< C_TFWeaponBase * >( )->m_iItemDefinitionIndex( ) != Medic_s_TheVaccinator )
        {
            return false;
        }

        return weapon->As< C_WeaponMedigun * >( )->m_hHealingTarget( ).Get( ) == g_cl.m_local;
    }

    return false;
}

void auto_vacc::clear( )
{
    this->m_press_count        = 0;
    this->m_pop_count          = 0;
    this->m_last_buttons       = 0;
    this->m_last_wanted        = RESIST_NONE;
    this->m_last_pop           = false;
    this->m_sync_medic         = 0;
    this->m_sync_until         = 0.0;
    this->m_last_request_batch = 0;
    this->m_pending_pop_medic  = 0;
    this->m_pending_pop_until  = 0.0;
    this->m_status_patient     = 0;
    this->m_next_status_time   = 0.0;

    for ( i32 &until : this->m_crit_stream_until )
    {
        until = 0;
    }

    for ( sighting_t &sighting : this->m_sightings )
    {
        sighting = sighting_t{ };
    }

    this->clear_party_request( );
}

void auto_vacc::clear_party_request( )
{
    this->m_request_patient  = 0;
    this->m_requested_resist = RESIST_NONE;
    this->m_request_until    = 0.0;
    this->m_request_pop_sent = false;
}

void auto_vacc::on_party_status( u32 medic, u32 patient )
{
    if ( patient != g_steam.local_account_id( ) || !medic_healing_local( medic ) )
    {
        return;
    }

    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    if ( this->m_sync_medic != medic || now >= this->m_sync_until )
    {
        push_notification( notify_kind_t::information, ATLAS_XORSTR_STATIC( "Vacc sync available" ),
                           ATLAS_XORSTR_STATIC( "Your medic can receive charge requests." ) );
    }

    this->m_sync_medic = medic;
    this->m_sync_until = now + 15.0;
}

void auto_vacc::on_party_request( u32 patient, u32 medic, party_vacc_request request )
{
    if ( !g_cfg.m_auto_vacc.m_enabled || medic != g_steam.local_account_id( ) ||
         !g_party_network.has_peer( patient ) || this->m_request_pop_sent )
    {
        return;
    }

    C_WeaponMedigun *medigun{ this->active_vaccinator( ) };
    if ( !medigun )
    {
        return;
    }

    C_TFPlayer   *target{ this->patient_of( medigun ) };
    player_info_t info{ };
    if ( !target || !g_tf2.m_engine->GetPlayerInfo( target->entindex( ), &info ) || info.friendsID != patient )
    {
        return;
    }

    if ( medigun->m_flChargeLevel( ) < CHARGE_CHUNK )
    {
        g_party_network.send_vacc_result( patient, party_vacc_result::no_charge );
        push_notification( notify_kind_t::warning, ATLAS_XORSTR_STATIC( "Patient requested pop" ),
                           ATLAS_XORSTR_STATIC( "No Vaccinator charges left." ) );
        this->clear_party_request( );
        return;
    }

    this->m_request_patient  = patient;
    this->m_requested_resist = static_cast< i32 >( request );
    this->m_request_until    = g_tf2.m_engine->Plat_FloatTime( ) + 5.0;
    this->m_request_pop_sent = false;

    const char *name{ request == party_vacc_request::bullet  ? ATLAS_XORSTR_STATIC( "Bullet resistance pop" )
                      : request == party_vacc_request::blast ? ATLAS_XORSTR_STATIC( "Blast resistance pop" )
                                                             : ATLAS_XORSTR_STATIC( "Fire resistance pop" ) };
    push_notification( notify_kind_t::information, ATLAS_XORSTR_STATIC( "Patient requested" ), name );
}

void auto_vacc::on_party_result( u32 medic, u32 patient, party_vacc_result result )
{
    if ( patient != g_steam.local_account_id( ) || medic != this->m_pending_pop_medic ||
         g_tf2.m_engine->Plat_FloatTime( ) >= this->m_pending_pop_until )
    {
        return;
    }

    this->m_pending_pop_medic = 0;

    switch ( result )
    {
        case party_vacc_result::popped:
            push_notification( notify_kind_t::success, ATLAS_XORSTR_STATIC( "Medic popped" ),
                               ATLAS_XORSTR_STATIC( "Your requested charge was activated." ) );
            break;
        case party_vacc_result::no_charge:
            push_notification( notify_kind_t::warning, ATLAS_XORSTR_STATIC( "Medic has no charges left!" ),
                               ATLAS_XORSTR_STATIC( "Careful!" ) );
            break;
        case party_vacc_result::unavailable:
            push_notification( notify_kind_t::warning, ATLAS_XORSTR_STATIC( "Charge request failed" ),
                               ATLAS_XORSTR_STATIC( "The medic could not activate it." ) );
            break;
        default: break;
    }
}

void auto_vacc::on_network_update( )
{
    if ( !this->m_request_patient || !g_cfg.m_misc.m_party_network )
    {
        return;
    }

    if ( g_tf2.m_engine->Plat_FloatTime( ) >= this->m_request_until )
    {
        g_party_network.send_vacc_result( this->m_request_patient, party_vacc_result::unavailable );
        this->clear_party_request( );
        return;
    }

    if ( !this->m_request_pop_sent )
    {
        return;
    }

    // the Vaccinator drops m_bChargeRelease inside the same SecondaryAttack, so the deployed condition is the proof
    C_WeaponMedigun *medigun{ this->active_vaccinator( ) };
    if ( !medigun || !g_cl.m_local->InCond( UBER_CONDS[ this->m_requested_resist ] ) )
    {
        return;
    }

    g_party_network.send_vacc_result( this->m_request_patient, party_vacc_result::popped );
    push_notification( notify_kind_t::success, ATLAS_XORSTR_STATIC( "Popped due to request" ),
                       ATLAS_XORSTR_STATIC( "Vaccinator charge activated." ) );
    this->clear_party_request( );
}

C_WeaponMedigun *auto_vacc::active_vaccinator( ) const
{
    C_TFPlayer *local{ g_cl.m_local };
    if ( !local || !local->IsAlive( ) || local->m_iClass( ) != TF_CLASS_MEDIC )
    {
        return nullptr;
    }

    C_TFWeaponBase *weapon{ g_cl.m_weapon };
    if ( !weapon || weapon->m_iItemDefinitionIndex( ) != Medic_s_TheVaccinator )
    {
        return nullptr;
    }

    return weapon->As< C_WeaponMedigun * >( );
}

C_TFPlayer *auto_vacc::patient_of( C_WeaponMedigun *medigun ) const
{
    C_BaseEntity *target{ medigun->m_hHealingTarget( ).Get( ) };
    if ( !target || target == g_cl.m_local || target->GetClientClass( )->GetClassID( ) != ETFClassID::CTFPlayer )
    {
        return nullptr;
    }

    C_TFPlayer *patient{ target->As< C_TFPlayer * >( ) };
    return patient->IsAlive( ) ? patient : nullptr;
}

i32 auto_vacc::weapon_resist_type( i32 weapon_id, i32 item )
{
    switch ( item )
    {
        case Soldier_m_RocketJumper:
        case Demoman_s_StickyJumper:
        case Spy_m_TheEnforcer: return RESIST_NONE;
        default: break;
    }

    switch ( weapon_id )
    {
        case TF_WEAPON_SCATTERGUN:
        case TF_WEAPON_SODA_POPPER:
        case TF_WEAPON_PEP_BRAWLER_BLASTER:
        case TF_WEAPON_HANDGUN_SCOUT_PRIMARY:
        case TF_WEAPON_HANDGUN_SCOUT_SECONDARY:
        case TF_WEAPON_SHOTGUN_PRIMARY:
        case TF_WEAPON_SHOTGUN_SOLDIER:
        case TF_WEAPON_SHOTGUN_HWG:
        case TF_WEAPON_SHOTGUN_PYRO:
        case TF_WEAPON_SHOTGUN_BUILDING_RESCUE:
        case TF_WEAPON_SENTRY_REVENGE:
        case TF_WEAPON_PISTOL:
        case TF_WEAPON_PISTOL_SCOUT:
        case TF_WEAPON_SMG:
        case TF_WEAPON_CHARGED_SMG:
        case TF_WEAPON_MINIGUN:
        case TF_WEAPON_REVOLVER:
        case TF_WEAPON_SNIPERRIFLE:
        case TF_WEAPON_SNIPERRIFLE_DECAP:
        case TF_WEAPON_SNIPERRIFLE_CLASSIC:
        case TF_WEAPON_SYRINGEGUN_MEDIC:
        case TF_WEAPON_NAILGUN:
        case TF_WEAPON_COMPOUND_BOW:
        case TF_WEAPON_CROSSBOW: return MEDIGUN_BULLET_RESIST;

        case TF_WEAPON_ROCKETLAUNCHER:
        case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
        case TF_WEAPON_PARTICLE_CANNON:
        case TF_WEAPON_GRENADELAUNCHER:
        case TF_WEAPON_PIPEBOMBLAUNCHER:
        case TF_WEAPON_CANNON: return MEDIGUN_BLAST_RESIST;

        case TF_WEAPON_FLAMETHROWER:
        case TF_WEAPON_FLAME_BALL:
        case TF_WEAPON_FLAREGUN:
        case TF_WEAPON_FLAREGUN_REVENGE: return MEDIGUN_FIRE_RESIST;

        default: return RESIST_NONE;
    }
}

bool auto_vacc::build_ward( C_TFPlayer *player, ward_t &out ) const
{
    if ( player->IsUbered( ) || player->IsBonked( ) )
    {
        return false;
    }

    // inside prediction the local origin is already the one this command ends on
    out.m_player    = player;
    out.m_origin    = player->m_vecOrigin( );
    out.m_velocity  = player->m_vecVelocity( );
    out.m_mins      = player->m_vecMins( );
    out.m_maxs      = player->m_vecMaxs( );
    out.m_eye       = player->Weapon_ShootPosition( );
    out.m_health    = static_cast< f32 >( player->m_iHealth( ) );
    out.m_speed     = player->m_flMaxspeed( );
    out.m_minicrits = player->IsMarkedForDeath( ) || player->IsJarated( );

    for ( i32 resist{ }; resist < MEDIGUN_NUM_RESISTS; ++resist )
    {
        out.m_covered_until[ resist ] = this->covered_until( player, resist );
    }

    return true;
}

// the patient is drawn where the server had them a while ago; walk them to the frame the command runs in
void auto_vacc::place_patient( ward_t &ward ) const
{
    const i32      ticks{ g_ragebot.target_catch_up_ticks( ward.m_player, 0 ) };
    move_sim_ctx_t ctx{ ward.m_player };

    for ( i32 tick{ }; tick < ticks; ++tick )
    {
        if ( !g_move_sim.run_tick( &ctx ) )
        {
            break;
        }
    }

    ward.m_origin   = ctx.m_pred_origin;
    ward.m_velocity = ctx.m_pred_velocity;
    ward.m_mins     = ctx.m_pred_mins;
    ward.m_maxs     = ctx.m_pred_maxs;
    ward.m_eye      = splash::player_eye( ward.m_player, ctx.m_pred_origin, ctx.m_pred_flags );
}

i32 auto_vacc::covered_until( C_TFPlayer *player, i32 resist ) const
{
    const i32 duration{ TIME_TO_TICKS( g_tf2.weapon_vaccinator_resist_duration->GetFloat( ) ) };
    i32       until{ };

    if ( player->InCond( UBER_CONDS[ resist ] ) )
    {
        // player_data has not stamped a condition that arrived in this very update yet; it is fresh then
        const f32 remaining{ g_player_data.vacc_remaining( player, resist ) };
        until = g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick +
                ( remaining > 0.0f ? TIME_TO_TICKS( remaining ) : duration );
    }

    // our own pops cover both wards from the frame they run in, before any snapshot shows it
    for ( usize index{ }; index < this->m_pop_count; ++index )
    {
        if ( this->m_pops[ index ].m_type == resist )
        {
            until = math::bigger( until, this->m_pops[ index ].m_tick + duration );
        }
    }

    return until;
}

vec3 auto_vacc::track_velocity( C_BaseEntity *projectile, bool pipe, f32 gravity )
{
    const float now{ projectile->m_flSimulationTime( ) };
    vec3        from{ projectile->m_vecOldOrigin( ) };
    float       span{ now - projectile->m_flOldSimulationTime( ) };

    if ( !pipe )
    {
        C_TFProjectile_Rocket *rocket{ projectile->As< C_TFProjectile_Rocket * >( ) };
        sighting_t            &first{ this->m_sightings[ projectile->entindex( ) ] };
        const i32              serial{ projectile->GetRefEHandle( ).GetSerialNumber( ) };

        // a deflection starts a new line
        if ( first.m_time <= 0.0f || first.m_serial != serial || first.m_deflected != rocket->m_iDeflected( ) )
        {
            first = sighting_t{ serial, rocket->m_iDeflected( ), projectile->m_vecOrigin( ), now };
        }

        from = first.m_origin;
        span = now - first.m_time;
    }

    if ( span <= 0.0f || projectile->m_flOldSimulationTime( ) <= 0.0f )
    {
        return pipe ? projectile->As< C_TFGrenadePipebombProjectile * >( )->m_vInitialVelocity( )
                    : projectile->As< C_TFProjectile_Rocket * >( )->m_vInitialVelocity( );
    }

    // a falling projectile's chord is its velocity half the span back
    vec3 velocity{ ( projectile->m_vecOrigin( ) - from ) / span };
    velocity.z -= gravity * span * 0.5f;
    return velocity;
}

bool auto_vacc::read_flight( C_BaseEntity *projectile, ETFClassID class_id, flight_t &out )
{
    const bool pipe{ class_id == ETFClassID::CTFGrenadePipebombProjectile };

    out.m_projectile = projectile;
    out.m_origin     = projectile->m_vecOrigin( );
    out.m_mins       = projectile->m_vecMins( );
    out.m_maxs       = projectile->m_vecMaxs( );
    out.m_seen       = TIME_TO_TICKS( projectile->m_flSimulationTime( ) );
    out.m_edge       = 0.5f;

    if ( class_id == ETFClassID::CTFProjectile_SentryRocket )
    {
        out.m_type     = MEDIGUN_BLAST_RESIST;
        out.m_damage   = SENTRY_ROCKET_DAMAGE;
        out.m_radius   = TF_ROCKET_RADIUS;
        out.m_crit     = projectile->As< C_TFProjectile_Rocket * >( )->m_bCritical( );
        out.m_contact  = true;
        out.m_velocity = this->track_velocity( projectile, false, 0.0f );
        return true;
    }

    C_BaseEntity *launcher_entity{ pipe ? projectile->As< C_TFGrenadePipebombProjectile * >( )->m_hLauncher( ).Get( )
                                        : projectile->As< C_TFProjectile_Rocket * >( )->m_hLauncher( ).Get( ) };
    if ( !launcher_entity )
    {
        return false;
    }

    C_TFWeaponBase *launcher{ launcher_entity->As< C_TFWeaponBase * >( ) };
    CTFWeaponInfo  *info{ launcher->GetWeaponInfo( ) };
    out.m_weapon_id = launcher->GetWeaponID( );
    out.m_type      = weapon_resist_type( out.m_weapon_id, launcher->m_iItemDefinitionIndex( ) );
    if ( out.m_type == RESIST_NONE || !info )
    {
        return false;
    }

    out.m_attacker = launcher->m_hOwnerEntity( ).Get( );
    out.m_damage   = g_cl.attrib_hook( launcher, static_cast< f32 >( info->GetWeaponData( 0 ).m_nDamage ),
                                       ATLAS_XORSTR( "mult_dmg" ).get( ) );
    out.m_spread   = distance_spread( out.m_weapon_id );

    const f32 falloff_mod{ g_cl.attrib_hook( launcher, 1.0f, ATLAS_XORSTR( "mult_dmg_falloff" ).get( ) ) };
    if ( falloff_mod != 1.0f )
    {
        out.m_edge += falloff_mod;
    }

    switch ( class_id )
    {
        case ETFClassID::CTFProjectile_Rocket:
        case ETFClassID::CTFProjectile_EnergyBall:
            out.m_radius  = g_cl.attrib_hook( launcher, TF_ROCKET_RADIUS, ATLAS_XORSTR( "mult_explosion_radius" ).get( ) );
            out.m_crit    = class_id == ETFClassID::CTFProjectile_Rocket &&
                            projectile->As< C_TFProjectile_Rocket * >( )->m_bCritical( );
            out.m_contact = true;
            break;

        case ETFClassID::CTFProjectile_BallOfFire:
            out.m_crit    = projectile->As< C_TFProjectile_Rocket * >( )->m_bCritical( );
            out.m_contact = true;
            break;

        case ETFClassID::CTFProjectile_Flare:
            out.m_radius       = launcher->GetProjectileBlastRadius( );
            out.m_crit         = projectile->As< C_TFProjectile_Flare * >( )->m_bCritical( );
            out.m_crit_burning = ( static_cast< i32 >( g_cl.attrib_hook( launcher, 0.0f, ATLAS_XORSTR( "or_crit_vs_playercond" ).get( ) ) ) & 1 ) != 0;
            out.m_gravity      = g_tf2.sv_gravity->GetFloat( ) * FLARE_GRAVITY;
            out.m_contact      = true;
            break;

        case ETFClassID::CTFProjectile_Arrow:
        case ETFClassID::CTFProjectile_HealingBolt:
            out.m_crit    = projectile->As< C_TFProjectile_Arrow * >( )->m_bCritical( );
            out.m_contact = true;

            // the bow's charge sets the arrow's speed, damage and gravity alike
            if ( out.m_weapon_id == TF_WEAPON_COMPOUND_BOW )
            {
                const f32 speed{ projectile->As< C_TFProjectile_Rocket * >( )->m_vInitialVelocity( ).length( ) };
                const f32 charge{ math::remap_clamped( speed, BOW_MIN_SPEED, BOW_MAX_SPEED, 0.0f, 1.0f ) };
                out.m_damage    = g_cl.attrib_hook( launcher, BOW_BASE_DAMAGE, ATLAS_XORSTR( "mult_dmg" ).get( ) ) + out.m_damage * charge;
                out.m_gravity   = g_tf2.sv_gravity->GetFloat( ) * math::remap_clamped( charge, 0.0f, 1.0f, BOW_MAX_GRAVITY, BOW_MIN_GRAVITY );
                out.m_headshots = true;
            }
            break;

        case ETFClassID::CTFGrenadePipebombProjectile:
        {
            C_TFGrenadePipebombProjectile *grenade{ projectile->As< C_TFGrenadePipebombProjectile * >( ) };

            out.m_radius  = g_cl.attrib_hook( launcher, TF_ROCKET_RADIUS, ATLAS_XORSTR( "mult_explosion_radius" ).get( ) );
            out.m_crit    = grenade->m_bCritical( );
            out.m_gravity = g_tf2.sv_gravity->GetFloat( );
            out.m_contact = grenade->m_iType( ) == TF_GL_MODE_REGULAR && !grenade->m_bTouched( );

            if ( grenade->m_iType( ) != TF_GL_MODE_REMOTE_DETONATE ||
                 g_tf2.m_globals->curtime - grenade->m_flCreationTime( ) >= STICKY_SPREAD_AGE )
            {
                out.m_spread = 0.0f;
            }
            break;
        }

        default: return false;
    }

    out.m_velocity = this->track_velocity( projectile, pipe, out.m_gravity );
    return true;
}

bool auto_vacc::enters_ward( const flight_t &flight, const ward_t &ward, const vec3 &from, const vec3 &to,
                             f32 from_time, f32 to_time, f32 &entry ) const
{
    // in the ward's own frame the step is still a segment, against the hull grown by the projectile's box
    const f32  lead{ TICKS_TO_TIME( this->m_exec - flight.m_seen ) };
    const vec3 start{ from - ( ward.m_origin + ward.m_velocity * ( from_time - lead ) ) };
    const vec3 end{ to - ( ward.m_origin + ward.m_velocity * ( to_time - lead ) ) };
    const f32  fraction{ math::segment_box_fraction( start, end, ward.m_mins - flight.m_maxs, ward.m_maxs - flight.m_mins ) };
    if ( fraction == FLT_MAX )
    {
        return false;
    }

    entry = from_time + ( to_time - from_time ) * fraction;
    return true;
}

f32 auto_vacc::blow( const flight_t &flight, const ward_t &ward, f32 falloff, bool headshot, bool &crit ) const
{
    crit = flight.m_crit || headshot || ( flight.m_crit_burning && ward.m_player->IsBurning( ) );

    const f32 damage{ flight.m_damage * falloff };
    if ( crit )
    {
        return damage * TF_DAMAGE_CRIT_MULTIPLIER;
    }

    const vec3 center{ ward.m_origin + ( ward.m_mins + ward.m_maxs ) * 0.5f };
    const f32  distance{ flight.m_attacker ? center.dist_to( flight.m_attacker->WorldSpaceCenter( ) ) : OPTIMAL_DISTANCE };
    const f32  scale{ distance_scale( flight.m_weapon_id, flight.m_spread, distance, ward.m_minicrits ) };

    return damage * scale * ( ward.m_minicrits ? TF_DAMAGE_MINICRIT_MULTIPLIER : 1.0f );
}

void auto_vacc::add_hit( i32 tick, i32 type, i32 ward, i32 escape, f32 damage, bool crit, bool standing )
{
    if ( this->m_hit_count >= MAX_HITS )
    {
        return;
    }

    this->m_hits[ this->m_hit_count++ ] = hit_t{ tick, type, ward, escape, damage, crit, standing };
}

void auto_vacc::explode( const flight_t &flight, const vec3 &point, i32 tick, i32 direct, f32 scale )
{
    const f32 moved{ TICKS_TO_TIME( tick - this->m_exec ) };

    for ( usize index{ }; index < this->m_ward_count; ++index )
    {
        const ward_t &ward{ this->m_wards[ index ] };
        const vec3    origin{ ward.m_origin + ward.m_velocity * moved };
        const vec3    eye{ ward.m_eye + ward.m_velocity * moved };

        // CTFRadiusDamageInfo::ApplyToEntity gives the entity the rocket touched its full damage
        f32 falloff{ 1.0f };
        if ( static_cast< i32 >( index ) != direct )
        {
            if ( flight.m_radius <= 0.0f )
            {
                continue;
            }

            const splash_target_t target{ ward.m_player, origin, origin + ward.m_mins, origin + ward.m_maxs, eye };
            falloff = splash::damage( point, flight.m_radius, flight.m_edge, target );
            if ( falloff <= 0.0f )
            {
                continue;
            }
        }

        // how long the ward needs to walk out of the blast, or out of the projectile's way
        const f32 reach{ flight.m_radius > 0.0f ? flight.m_radius : ( ward.m_maxs.x - ward.m_mins.x ) * 0.5f };
        const i32 escape{ ward.m_speed > 0.0f ? TIME_TO_TICKS( reach / ward.m_speed ) : this->m_horizon - this->m_exec };

        // ArrowTouch traces hitboxes; the head sits between the eye and the top of the hull
        const bool headshot{ static_cast< i32 >( index ) == direct && flight.m_headshots &&
                             point.z >= eye.z - ( origin.z + ward.m_maxs.z - eye.z ) };

        bool      crit{ };
        const f32 damage{ this->blow( flight, ward, falloff * scale, headshot, crit ) };
        this->add_hit( tick, flight.m_type, static_cast< i32 >( index ), escape, damage, crit, flight.m_standing );
    }
}

void auto_vacc::fly_straight( const flight_t &flight )
{
    const f32 span{ TICKS_TO_TIME( this->m_horizon - flight.m_seen ) };
    if ( span <= 0.0f )
    {
        return;
    }

    const vec3 end{ flight.m_origin + flight.m_velocity * span };

    trace_t                trace{ };
    Ray_t                  ray{ };
    CTraceFilterProjectile filter{ };

    filter.m_pSkip = flight.m_projectile;
    ray.Init( flight.m_origin, end, flight.m_mins, flight.m_maxs );
    g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );

    // the wards are where they will be, not where their entities stand, so they are met analytically
    f32 first{ span * trace.fraction };
    i32 direct{ -1 };
    for ( usize index{ }; index < this->m_ward_count; ++index )
    {
        f32 entry{ };
        if ( this->enters_ward( flight, this->m_wards[ index ], flight.m_origin, end, 0.0f, span, entry ) && entry < first )
        {
            first  = entry;
            direct = static_cast< i32 >( index );
        }
    }

    if ( direct < 0 && trace.fraction >= 1.0f )
    {
        return;
    }

    // CTFBaseRocket::Explode pulls the blast a unit out of the surface it struck
    const vec3 point{ direct >= 0 ? flight.m_origin + flight.m_velocity * first : trace.endpos + trace.plane.normal };
    this->explode( flight, point, flight.m_seen + math::bigger( ticks_until( first ), 1 ), direct, 1.0f );
}

bool auto_vacc::fly_arc( const flight_t &flight, i32 last, landing_t &out )
{
    trace_t                trace{ };
    Ray_t                  ray{ };
    CTraceFilterProjectile filter{ };

    filter.m_pSkip = flight.m_projectile;

    vec3 from{ flight.m_origin };
    f32  from_time{ };

    for ( i32 tick{ flight.m_seen + 1 }; tick <= last; ++tick )
    {
        const f32  time{ TICKS_TO_TIME( tick - flight.m_seen ) };
        const vec3 to{ flight.m_origin + flight.m_velocity * time - vec3{ 0.0f, 0.0f, flight.m_gravity * time * time * 0.5f } };

        if ( flight.m_contact )
        {
            for ( usize index{ }; index < this->m_ward_count; ++index )
            {
                f32 entry{ };
                if ( !this->enters_ward( flight, this->m_wards[ index ], from, to, from_time, time, entry ) )
                {
                    continue;
                }

                this->explode( flight, from + ( to - from ) * ( ( entry - from_time ) / ( time - from_time ) ), tick,
                               static_cast< i32 >( index ), 1.0f );
                return true;
            }
        }

        ray.Init( from, to, flight.m_mins, flight.m_maxs );
        g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );

        out.m_tick     = tick;
        out.m_position = trace.endpos;
        if ( trace.fraction < 1.0f )
        {
            out.m_landed = true;
            return false;
        }

        from      = to;
        from_time = time;
    }

    return false;
}

void auto_vacc::watch_sticky( const flight_t &flight, C_TFGrenadePipebombProjectile *pipe )
{
    // a sticky goes off whenever its owner wants once it is armed, so it threatens from the first command it could
    C_BaseEntity *launcher{ pipe->m_hLauncher( ).Get( ) };
    const f32     arm{ g_cl.attrib_hook( launcher, g_tf2.tf_grenadelauncher_livetime->GetFloat( ),
                                         ATLAS_XORSTR( "sticky_arm_time" ).get( ) ) };
    const i32     armed{ TIME_TO_TICKS( pipe->m_flCreationTime( ) + arm ) };
    const i32     tick{ math::bigger( armed, this->m_exec ) };
    if ( tick > this->m_horizon )
    {
        return;
    }

    landing_t landing{ flight.m_origin, flight.m_seen, pipe->m_bTouched( ) };
    if ( !landing.m_landed )
    {
        this->fly_arc( flight, tick, landing );
    }

    // once armed it can go off on any command, a standing threat like a crit-boosted gun in sight
    flight_t burst{ flight };
    burst.m_standing = armed <= this->m_exec;

    // CTFGrenadePipebombProjectile::GetDamageRadius shrinks an air burst while the sticky is still in flight
    if ( !landing.m_landed )
    {
        burst.m_radius = splash::air_radius( flight.m_radius, TICKS_TO_TIME( tick ) - pipe->m_flCreationTime( ) );
    }

    this->explode( burst, landing.m_position, tick, -1, 1.0f );
}

void auto_vacc::watch_roller( const flight_t &flight, C_TFGrenadePipebombProjectile *pipe, const vec3 &position )
{
    // a grenade that touched anything but a player first only goes off on its fuse, for less damage
    C_BaseEntity *launcher{ pipe->m_hLauncher( ).Get( ) };
    const f32     fuse{ g_cl.attrib_hook( launcher, TF_WEAPON_GRENADE_DETONATE_TIME, ATLAS_XORSTR( "fuse_mult" ).get( ) ) };
    const i32     tick{ TIME_TO_TICKS( pipe->m_flCreationTime( ) + fuse ) };
    if ( tick > this->m_horizon )
    {
        return;
    }

    this->explode( flight, position, tick, -1, TF_WEAPON_PIPEBOMB_TIMER_DMG_REDUCTION );
}

void auto_vacc::scan_projectiles( )
{
    const i32 team{ g_cl.m_local->m_iTeamNum( ) };

    for ( const auto *cached : g_entity_cache.projectiles( ) )
    {
        C_BaseEntity *projectile{ cached->m_entity };
        if ( !cached->networked( ) || projectile->IsDormant( ) || projectile->m_iTeamNum( ) == team )
        {
            continue;
        }

        const ETFClassID class_id{ cached->m_class_id };
        if ( class_id != ETFClassID::CTFGrenadePipebombProjectile && projectile->m_MoveType( ) == MOVETYPE_NONE )
        {
            continue;
        }

        flight_t flight{ };
        if ( !this->read_flight( projectile, class_id, flight ) )
        {
            continue;
        }

        if ( class_id != ETFClassID::CTFGrenadePipebombProjectile )
        {
            if ( flight.m_gravity > 0.0f )
            {
                landing_t landing{ };
                this->fly_arc( flight, this->m_horizon, landing );
            } else
            {
                this->fly_straight( flight );
            }

            continue;
        }

        C_TFGrenadePipebombProjectile *pipe{ projectile->As< C_TFGrenadePipebombProjectile * >( ) };
        switch ( pipe->m_iType( ) )
        {
            case TF_GL_MODE_REMOTE_DETONATE: this->watch_sticky( flight, pipe ); break;
            case TF_GL_MODE_REMOTE_DETONATE_PRACTICE: break; // the sticky jumper
            default:
            {
                if ( pipe->m_bTouched( ) )
                {
                    this->watch_roller( flight, pipe, flight.m_origin );
                    break;
                }

                landing_t landing{ };
                if ( !this->fly_arc( flight, this->m_horizon, landing ) && landing.m_landed )
                {
                    this->watch_roller( flight, pipe, landing.m_position );
                }
                break;
            }
        }
    }
}

void auto_vacc::ask_pop( i32 type, i32 deadline, i32 escape, bool continuous, cause_t cause )
{
    need_t &need{ this->m_needs[ type ] };
    if ( need.m_wanted && deadline >= need.m_deadline )
    {
        need.m_continuous |= continuous;
        return;
    }

    need.m_wanted     = true;
    need.m_deadline   = deadline;
    need.m_escape     = escape;
    need.m_continuous = need.m_continuous || continuous;
    need.m_cause      = cause;
}

void auto_vacc::weigh_hits( )
{
    // in order of landing, so the running total per ward and type finds the blow that kills
    for ( usize index{ 1 }; index < this->m_hit_count; ++index )
    {
        const hit_t hit{ this->m_hits[ index ] };
        usize       slot{ index };

        while ( slot > 0 && this->m_hits[ slot - 1 ].m_tick > hit.m_tick )
        {
            this->m_hits[ slot ] = this->m_hits[ slot - 1 ];
            --slot;
        }

        this->m_hits[ slot ] = hit;
    }

    f32 taken[ MAX_WARDS ][ MEDIGUN_NUM_RESISTS ]{ };

    for ( usize index{ }; index < this->m_hit_count; ++index )
    {
        const hit_t  &hit{ this->m_hits[ index ] };
        const ward_t &ward{ this->m_wards[ hit.m_ward ] };

        // landed on the server before this command can run, or a charge already takes it
        if ( hit.m_tick < this->m_exec || hit.m_tick < ward.m_covered_until[ hit.m_type ] )
        {
            continue;
        }

        this->m_needs[ hit.m_type ].m_seen += hit.m_damage;

        f32 &total{ taken[ hit.m_ward ][ hit.m_type ] };
        total += hit.m_damage;

        if ( hit.m_crit )
        {
            this->ask_pop( hit.m_type, hit.m_tick, hit.m_escape, hit.m_standing, cause_crit );
        } else if ( total >= ward.m_health )
        {
            this->ask_pop( hit.m_type, hit.m_tick, hit.m_escape, hit.m_standing, cause_lethal );
        }
    }
}

bool auto_vacc::aims_at_head( C_TFPlayer *player, const vec3 &eye, const ward_t &ward ) const
{
    vec3 forward{ };
    math::angle_vectors( player->m_angEyeAngles( ), &forward );

    const vec3 to_eye{ ward.m_eye - eye };
    const f32  along{ to_eye.dot( forward ) };
    const f32  head{ ward.m_origin.z + ward.m_maxs.z - ward.m_eye.z };

    return along > 0.0f && ( to_eye - forward * along ).length( ) <= head + along * EYE_ANGLE_HALF_STEP;
}

void auto_vacc::scan_players( )
{
    const i32 team{ g_cl.m_local->m_iTeamNum( ) };
    const f32 duration{ g_tf2.weapon_vaccinator_resist_duration->GetFloat( ) };

    for ( const auto *cached : g_entity_cache.players( ) )
    {
        C_TFPlayer *player{ cached->m_entity->As< C_TFPlayer * >( ) };
        if ( player->IsDormant( ) ||          // Not networked.
             player->m_iTeamNum( ) == team || // Ours.
             !player->IsAlive( ) ||           // Dead.
             player->IsGhost( ) ||            // Halloween ghost.
             player->IsBonked( ) ||           // Can't shoot.
             player->IsTaunting( ) ||         // Can't shoot.
             player->IsCloaked( ) )           // Can't shoot.
        {
            continue;
        }

        C_BaseEntity *weapon_entity{ player->m_hActiveWeapon( ).Get( ) };
        if ( !weapon_entity )
        {
            continue;
        }

        C_TFWeaponBase *weapon{ weapon_entity->As< C_TFWeaponBase * >( ) };
        const i32       weapon_id{ weapon->GetWeaponID( ) };
        const i32       type{ weapon_resist_type( weapon_id, weapon->m_iItemDefinitionIndex( ) ) };
        CTFWeaponInfo  *info{ weapon->GetWeaponInfo( ) };
        if ( type == RESIST_NONE || !info )
        {
            continue;
        }

        const WeaponData_t &data{ info->GetWeaponData( 0 ) };
        const f32           interval{ g_cl.attrib_hook( weapon, data.m_flTimeFireDelay, ATLAS_XORSTR( "mult_postfiredelay" ).get( ) ) };
        const f32           base{ is_sniper_rifle( weapon_id ) ? sniper_damage( player, weapon, TICKS_TO_TIME( this->m_exec ) )
                                                               : static_cast< f32 >( data.m_nDamage ) };
        const f32           volley{ g_cl.attrib_hook( weapon, base, ATLAS_XORSTR( "mult_dmg" ).get( ) ) *
                                    static_cast< f32 >( math::bigger( data.m_nBulletsPerShot, 1 ) ) };
        if ( interval <= 0.0f )
        {
            continue;
        }

        const vec3 eye{ splash::player_eye( player, player->GetAbsOrigin( ), player->m_fFlags( ) ) };
        const bool crit{ player->IsCritBoosted( ) };
        const bool hitscan{ is_hitscan( weapon_id ) };
        const f32  range{ weapon_id == TF_WEAPON_FLAMETHROWER ? flame_reach( weapon ) : data.m_flRange };
        f32        seen{ };
        f32        hidden{ };

        // the playerlist tag or the cloud's list, the test the watchdog and ESP make
        bool cheater{ };
        if ( is_sniper_rifle( weapon_id ) )
        {
            player_info_t info{ };
            char          alias[ 129 ]{ };
            cheater = g_tf2.m_engine->GetPlayerInfo( player->entindex( ), &info ) && info.friendsID &&
                      ( g_cfg.player_tag( info.friendsID ) == config::PLAYER_TAG_CHEATER || g_player_cloud.known( info.friendsID, alias ) );
        }

        for ( usize index{ }; index < this->m_ward_count; ++index )
        {
            const ward_t &ward{ this->m_wards[ index ] };
            const vec3    center{ ward.m_origin + ( ward.m_mins + ward.m_maxs ) * 0.5f };
            const f32     distance{ eye.dist_to( center ) };
            if ( range > 0.0f && distance > range )
            {
                continue;
            }

            const bool minicrit{ ward.m_minicrits || player->IsMiniCritBoosted( ) };
            f32        shot{ volley * TF_DAMAGE_CRIT_MULTIPLIER };
            if ( !crit )
            {
                shot = volley * distance_scale( weapon_id, distance_spread( weapon_id ), distance, minicrit ) *
                       ( minicrit ? TF_DAMAGE_MINICRIT_MULTIPLIER : 1.0f );
            }

            const f32 expected{ shot / interval * duration };
            if ( !g_cl.trace_line( ward.m_player, player, ward.m_eye, eye, MASK_SHOT | CONTENTS_GRATE ) )
            {
                hidden = math::bigger( hidden, expected );
                continue;
            }

            seen = math::bigger( seen, expected );

            // a projectile weapon's shots are timed one by one once they are in the air
            if ( !hitscan || ward.m_covered_until[ type ] > this->m_exec )
            {
                continue;
            }

            if ( crit )
            {
                this->ask_pop( type, this->m_exec, 0, true, cause_crit_boost );
            } else if ( shot >= ward.m_health )
            {
                this->ask_pop( type, this->m_exec, 0, true, cause_lethal );
            }

            // every headshot is a crit; a marked cheater's zoom and aim prove nothing, his sight of the ward is enough
            if ( cheater )
            {
                this->ask_pop( type, this->m_exec, 0, true, cause_cheater_sniper );
            } else if ( is_sniper_rifle( weapon_id ) && player->IsZoomed( ) && this->aims_at_head( player, eye, ward ) )
            {
                this->ask_pop( type, this->m_exec, 0, true, cause_headshot );
            }
        }

        this->m_needs[ type ].m_seen += seen;
        this->m_needs[ type ].m_hidden += hidden;
    }
}

void auto_vacc::scan_sentries( )
{
    const i32 team{ g_cl.m_local->m_iTeamNum( ) };
    const f32 duration{ g_tf2.weapon_vaccinator_resist_duration->GetFloat( ) };

    for ( const auto *cached : g_entity_cache.buildings( ) )
    {
        if ( cached->m_class_id != ETFClassID::CObjectSentrygun )
        {
            continue;
        }

        C_ObjectSentrygun *sentry{ cached->m_entity->As< C_ObjectSentrygun * >( ) };
        if ( sentry->IsDormant( ) ||          // Not networked.
             sentry->m_iTeamNum( ) == team || // Ours.
             sentry->m_bBuilding( ) ||        // Can't shoot yet.
             sentry->m_bPlacing( ) ||         // Can't shoot yet.
             sentry->m_bCarried( ) ||         // Can't shoot.
             sentry->m_bHasSapper( ) ||       // Can't shoot.
             sentry->m_bDisabled( ) )         // Can't shoot.
        {
            continue;
        }

        // CObjectSentrygun::Attack: level 1 fires every 0.2 s, the rest every 0.1 s, a mini a quarter slower
        f32 interval{ sentry->m_iUpgradeLevel( ) == 1 ? 0.2f : 0.1f };
        f32 damage{ SENTRY_DAMAGE };
        if ( sentry->m_bMiniBuilding( ) )
        {
            interval *= 0.75f;
            damage = MINI_SENTRY_DAMAGE;
        }

        const vec3 center{ sentry->WorldSpaceCenter( ) };
        const f32  expected{ damage / interval * duration };
        f32        seen{ };
        f32        hidden{ };

        for ( usize index{ }; index < this->m_ward_count; ++index )
        {
            const ward_t &ward{ this->m_wards[ index ] };
            if ( center.dist_to( ward.m_eye ) > SENTRY_MAX_RANGE )
            {
                continue;
            }

            if ( g_cl.trace_line( ward.m_player, sentry, ward.m_eye, center, MASK_SHOT | CONTENTS_GRATE ) )
            {
                seen = expected;
            } else
            {
                hidden = expected;
            }
        }

        this->m_needs[ MEDIGUN_BULLET_RESIST ].m_seen += seen;
        this->m_needs[ MEDIGUN_BULLET_RESIST ].m_hidden += hidden;
    }
}

void auto_vacc::sync_commands( C_WeaponMedigun *medigun )
{
    // the snapshot that acked a command already ran it, so the networked resist and charge include every press and
    // pop up to command_ack; only the later ones are still on their way
    const i32 ack{ g_tf2.m_cl->command_ack };

    usize kept{ };
    for ( usize index{ }; index < this->m_press_count; ++index )
    {
        if ( this->m_presses[ index ] > ack )
        {
            this->m_presses[ kept++ ] = this->m_presses[ index ];
        }
    }
    this->m_press_count = kept;

    kept = 0;
    for ( usize index{ }; index < this->m_pop_count; ++index )
    {
        if ( this->m_pops[ index ].m_command > ack )
        {
            this->m_pops[ kept++ ] = this->m_pops[ index ];
        }
    }
    this->m_pop_count = kept;

    this->m_effective = ( medigun->m_nChargeResistType( ) + static_cast< i32 >( this->m_press_count ) ) % MEDIGUN_NUM_RESISTS;
    this->m_charge    = medigun->m_flChargeLevel( ) - CHARGE_CHUNK * static_cast< f32 >( this->m_pop_count );
}

// commands spent before the pop can go out: CycleResistType runs on the IN_RELOAD press edge, after
// SecondaryAttack in ItemPostFrame, so the pop has to wait for the command after the last press
i32 auto_vacc::commands_to_reach( i32 resist ) const
{
    const i32 cycles{ ( resist - this->m_effective + MEDIGUN_NUM_RESISTS ) % MEDIGUN_NUM_RESISTS };
    if ( cycles == 0 )
    {
        return 0;
    }

    return cycles * 2 - ( this->m_last_buttons & IN_RELOAD ? 0 : 1 );
}

bool auto_vacc::may_pop( i32 resist ) const
{
    const need_t &need{ this->m_needs[ resist ] };
    if ( !need.m_wanted || this->m_charge < CHARGE_CHUNK )
    {
        return false;
    }

    // a pop that lands after the blow spends the charge for nothing
    return need.m_continuous || this->m_exec + this->commands_to_reach( resist ) <= need.m_deadline;
}

bool auto_vacc::window_open( i32 resist ) const
{
    const need_t &need{ this->m_needs[ resist ] };
    if ( need.m_continuous )
    {
        return true;
    }

    const i32 slack{ need.m_deadline - this->m_exec - this->commands_to_reach( resist ) };

    switch ( g_cfg.m_auto_vacc.m_reactivity )
    {
        // send_frame knows a command's tick only to one either way and steps by two when it re-anchors on an ack
        // (vacc lab, 330 ms: waiting for slack 0 skipped the pop on 10 of 39 blows), so the last command that still
        // lands is the first one within a tick of the deadline
        case config::auto_vacc_t::REACTIVITY_CONSERVATIVE: return slack <= 1;
        case config::auto_vacc_t::REACTIVITY_BALANCED: return slack <= need.m_escape; // once the ward cannot get away
        default: return true;
    }
}

i32 auto_vacc::urgent_resist( ) const
{
    // the resistance already up pops first, it costs no cycling; the others go by deadline
    if ( this->may_pop( this->m_effective ) && this->window_open( this->m_effective ) )
    {
        return this->m_effective;
    }

    i32 best{ RESIST_NONE };
    for ( i32 resist{ }; resist < MEDIGUN_NUM_RESISTS; ++resist )
    {
        if ( !this->may_pop( resist ) )
        {
            continue;
        }

        if ( best == RESIST_NONE || this->m_needs[ resist ].m_deadline < this->m_needs[ best ].m_deadline )
        {
            best = resist;
        }
    }

    return best;
}

i32 auto_vacc::passive_resist( ) const
{
    // threats in sight decide; the ones out of sight only when nothing is in sight; a tie keeps the current one
    bool any_seen{ };
    for ( const need_t &need : this->m_needs )
    {
        any_seen |= need.m_seen > 0.0f;
    }

    i32 best{ this->m_effective };
    f32 best_value{ any_seen ? this->m_needs[ best ].m_seen : this->m_needs[ best ].m_hidden };
    for ( i32 resist{ }; resist < MEDIGUN_NUM_RESISTS; ++resist )
    {
        const f32 value{ any_seen ? this->m_needs[ resist ].m_seen : this->m_needs[ resist ].m_hidden };
        if ( value > best_value )
        {
            best       = resist;
            best_value = value;
        }
    }

    if ( best_value <= 0.0f && g_cfg.m_auto_vacc.m_idle_resist != 0 )
    {
        return static_cast< i32 >( g_cfg.m_auto_vacc.m_idle_resist ) - 1;
    }

    return best;
}

void auto_vacc::notify_pop( i32 resist, cause_t cause ) const
{
    const char *title{ };

    switch ( resist )
    {
        case MEDIGUN_BLAST_RESIST: title = ATLAS_XORSTR_STATIC( "Popped blast resist" ); break;
        case MEDIGUN_FIRE_RESIST: title = ATLAS_XORSTR_STATIC( "Popped fire resist" ); break;
        default: title = ATLAS_XORSTR_STATIC( "Popped bullet resist" ); break;
    }

    const char *reason{ };

    switch ( cause )
    {
        case cause_crit: reason = ATLAS_XORSTR_STATIC( "Crit damage incoming" ); break;
        case cause_lethal: reason = ATLAS_XORSTR_STATIC( "Next blow would kill" ); break;
        case cause_headshot: reason = ATLAS_XORSTR_STATIC( "Sniper aiming at a head" ); break;
        case cause_crit_boost: reason = ATLAS_XORSTR_STATIC( "Crit-boosted enemy in sight" ); break;
        case cause_cheater_sniper: reason = ATLAS_XORSTR_STATIC( "Cheater sniper in sight" ); break;
        default: reason = ATLAS_XORSTR_STATIC( "Enemy is rolling crits" ); break;
    }

    push_notification( notify_kind_t::warning, ICON_ZAP, title, reason );
}

void auto_vacc::drive( i32 wanted, bool pop, cause_t cause, bool requested )
{
    CUserCmd *cmd{ g_cl.m_cmd };
    cmd->buttons &= ~IN_RELOAD;

    if ( wanted != this->m_effective )
    {
        // a pop now would spend the charge on the resistance being cycled away from
        cmd->buttons &= ~IN_ATTACK2;

        if ( !( this->m_last_buttons & IN_RELOAD ) && this->m_press_count < MAX_PRESSES )
        {
            cmd->buttons |= IN_RELOAD;
            this->m_presses[ this->m_press_count++ ] = cmd->command_number;
        }

        return;
    }

    if ( !pop || this->m_pop_count >= MAX_POPS )
    {
        return;
    }

    cmd->buttons |= IN_ATTACK2;
    this->m_pops[ this->m_pop_count++ ] = pop_t{ cmd->command_number, this->m_exec, wanted };
    this->m_charge -= CHARGE_CHUNK;

    if ( !requested )
    {
        this->notify_pop( wanted, cause );
    }
}

void auto_vacc::request_party_charge( )
{
    if ( !g_cfg.m_misc.m_party_network || !g_cl.m_local || !g_cl.m_local->IsAlive( ) ||
         !this->m_sync_medic || g_tf2.m_engine->Plat_FloatTime( ) >= this->m_sync_until ||
         !g_party_network.has_peer( this->m_sync_medic ) || g_gui.open( ) ||
         this->m_last_request_batch == g_input.batch( ) )
    {
        return;
    }

    const auto &cfg{ g_cfg.m_auto_vacc };
    const u32   keys[]{ cfg.m_request_bullet_key.m_key, cfg.m_request_blast_key.m_key,
                        cfg.m_request_fire_key.m_key };

    for ( u8 index{ }; index < static_cast< u8 >( party_vacc_request::_COUNT ); ++index )
    {
        if ( !keys[ index ] || !g_input.pressed_batch( keys[ index ] ) )
        {
            continue;
        }

        this->m_last_request_batch = g_input.batch( );
        if ( !medic_healing_local( this->m_sync_medic ) )
        {
            this->m_sync_medic = 0;
            return;
        }

        g_party_network.send_vacc_request( this->m_sync_medic,
                                           static_cast< party_vacc_request >( index ) );
        this->m_pending_pop_medic = this->m_sync_medic;
        this->m_pending_pop_until = g_tf2.m_engine->Plat_FloatTime( ) + 6.0;
        return;
    }
}

void auto_vacc::sync_party_patient( C_WeaponMedigun *medigun, C_TFPlayer *patient )
{
    if ( !g_cfg.m_misc.m_party_network )
    {
        this->m_status_patient = 0;
        this->clear_party_request( );
        return;
    }

    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    u32          patient_account{ };
    if ( patient )
    {
        player_info_t info{ };
        if ( g_tf2.m_engine->GetPlayerInfo( patient->entindex( ), &info ) )
        {
            patient_account = info.friendsID;
        }
    }

    if ( this->m_request_patient &&
         ( patient_account != this->m_request_patient || now >= this->m_request_until ||
           ( medigun->m_flChargeLevel( ) < CHARGE_CHUNK && !this->m_request_pop_sent ) ) )
    {
        const party_vacc_result result{
            !this->m_request_pop_sent && medigun->m_flChargeLevel( ) < CHARGE_CHUNK
                ? party_vacc_result::no_charge
                : party_vacc_result::unavailable
        };
        g_party_network.send_vacc_result( this->m_request_patient, result );
        this->clear_party_request( );
    }

    if ( !patient_account || !g_party_network.has_peer( patient_account ) )
    {
        this->m_status_patient = 0;
        return;
    }

    if ( this->m_status_patient != patient_account )
    {
        this->m_status_patient = patient_account;
        push_notification( notify_kind_t::information, ATLAS_XORSTR_STATIC( "Vacc sync available" ),
                           ATLAS_XORSTR_STATIC( "Your patient can request Vaccinator charges." ) );
        this->m_next_status_time = 0.0;
    }

    if ( now >= this->m_next_status_time )
    {
        g_party_network.send_vacc_status( patient_account );
        this->m_next_status_time = now + 10.0;
    }
}

void auto_vacc::run( )
{
    if ( !g_cl.m_cmd )
    {
        return;
    }

    this->request_party_charge( );

    const auto &cfg{ g_cfg.m_auto_vacc };

    C_WeaponMedigun *medigun{ cfg.m_enabled ? this->active_vaccinator( ) : nullptr };
    if ( !medigun )
    {
        // outstanding presses and pops stop meaning anything once the gun is away
        this->m_press_count    = 0;
        this->m_pop_count      = 0;
        this->m_status_patient = 0;
        if ( this->m_request_patient && g_cfg.m_misc.m_party_network )
        {
            g_party_network.send_vacc_result( this->m_request_patient, party_vacc_result::unavailable );
        }
        this->clear_party_request( );
        this->m_last_buttons = g_cl.m_cmd->buttons;
        return;
    }

    C_TFPlayer *patient{ this->patient_of( medigun ) };
    this->sync_party_patient( medigun, patient );
    this->sync_commands( medigun );

    // the server frame this command runs in; every blow is timed against it
    const i32 frame{ g_tickbase.send_frame( ) };
    this->m_exec    = frame < 0 ? -1 : frame + g_networking.send_delay( ).ticks( );
    this->m_horizon = this->m_exec + TIME_TO_TICKS( g_tf2.weapon_vaccinator_resist_duration->GetFloat( ) );

    this->m_ward_count = 0;
    this->m_hit_count  = 0;
    for ( need_t &need : this->m_needs )
    {
        need = need_t{ };
    }

    if ( this->build_ward( g_cl.m_local, this->m_wards[ this->m_ward_count ] ) )
    {
        ++this->m_ward_count;
    }

    if ( patient && this->build_ward( patient, this->m_wards[ this->m_ward_count ] ) )
    {
        this->place_patient( this->m_wards[ this->m_ward_count ] );
        ++this->m_ward_count;
    }

    // without an acked command there is no frame to time projectiles against yet
    if ( this->m_ward_count && this->m_exec >= 0 )
    {
        this->scan_projectiles( );
        this->weigh_hits( );
    }

    if ( this->m_ward_count )
    {
        this->scan_players( );
        this->scan_sentries( );
    }

    for ( usize index{ }; index < this->m_ward_count; ++index )
    {
        if ( this->m_wards[ index ].m_player->IsBurning( ) )
        {
            this->m_needs[ MEDIGUN_FIRE_RESIST ].m_seen += TF_BURNING_DMG / TF_BURNING_FREQUENCY * g_tf2.weapon_vaccinator_resist_duration->GetFloat( );
        }
    }

    for ( i32 resist{ }; resist < MEDIGUN_NUM_RESISTS; ++resist )
    {
        if ( this->m_exec >= this->m_crit_stream_until[ resist ] )
        {
            continue;
        }

        bool covered{ true };
        for ( usize index{ }; index < this->m_ward_count; ++index )
        {
            covered &= this->m_wards[ index ].m_covered_until[ resist ] > this->m_exec;
        }

        if ( !covered )
        {
            this->ask_pop( resist, this->m_exec, 0, true, cause_crit_stream );
        }
    }

    const bool requested{ this->m_request_patient && this->m_requested_resist != RESIST_NONE };
    i32        wanted{ };
    bool       pop{ };
    cause_t    cause{ };

    if ( requested )
    {
        wanted = this->m_requested_resist;
        pop    = !this->m_request_pop_sent && this->m_charge >= CHARGE_CHUNK;
    } else
    {
        const i32 urgent{ cfg.m_auto_pop ? this->urgent_resist( ) : RESIST_NONE };
        if ( urgent != RESIST_NONE )
        {
            wanted = urgent;
            pop    = this->window_open( urgent );
            cause  = this->m_needs[ urgent ].m_cause;
        } else
        {
            wanted = this->passive_resist( );
        }
    }

    this->log_state( wanted, pop );
    this->drive( wanted, pop, cause, requested );

    if ( requested && ( g_cl.m_cmd->buttons & IN_ATTACK2 ) )
    {
        this->m_request_pop_sent = true;
    }

    this->m_last_buttons = g_cl.m_cmd->buttons;
}

void auto_vacc::on_player_hurt( IGameEvent *event )
{
    if ( !g_cfg.m_auto_vacc.m_enabled || !g_cfg.m_auto_vacc.m_auto_pop )
    {
        return;
    }

    C_WeaponMedigun *medigun{ this->active_vaccinator( ) };
    if ( !medigun || !event->GetBool( ATLAS_XORSTR( "crit" ).get( ) ) )
    {
        return;
    }

    const i32 attacker{ g_tf2.m_engine->GetPlayerForUserID(
        event->GetInt( ATLAS_XORSTR( "attacker" ).get( ) ) ) };
    const i32 victim{ g_tf2.m_engine->GetPlayerForUserID(
        event->GetInt( ATLAS_XORSTR( "userid" ).get( ) ) ) };

    if ( attacker <= 0 || attacker == victim )
    {
        return;
    }

    C_TFPlayer *patient{ this->patient_of( medigun ) };
    if ( victim != g_cl.m_local->entindex( ) && ( !patient || victim != patient->entindex( ) ) )
    {
        return;
    }

    // a crit-boosted attacker crits every shot and scan_players pops for the boost itself; the stream is a random roll's
    // window (CalcIsAttackCriticalHelper returns before it for crit-boosted players)
    IClientEntity *entity{ g_tf2.m_entity_list->GetClientEntity( attacker ) };
    if ( !entity || entity->GetClientClass( )->GetClassID( ) != ETFClassID::CTFPlayer || entity->As< C_TFPlayer * >( )->IsCritBoosted( ) )
    {
        return;
    }

    C_BaseEntity *weapon_entity{ entity->As< C_TFPlayer * >( )->m_hActiveWeapon( ).Get( ) };
    if ( !weapon_entity )
    {
        return;
    }

    // a rapid fire weapon that rolled a random crit keeps critting for TF_DAMAGE_CRIT_DURATION_RAPID
    C_TFWeaponBase *weapon{ weapon_entity->As< C_TFWeaponBase * >( ) };
    CTFWeaponInfo  *info{ weapon->GetWeaponInfo( ) };
    const i32       type{ weapon_resist_type( weapon->GetWeaponID( ), weapon->m_iItemDefinitionIndex( ) ) };
    if ( !info || !info->GetWeaponData( 0 ).m_bUseRapidFireCrits || type == RESIST_NONE )
    {
        return;
    }

    const i32 now{ g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick };
    if ( now >= this->m_crit_stream_until[ type ] )
    {
        this->m_crit_stream_until[ type ] = now + TIME_TO_TICKS( TF_DAMAGE_CRIT_DURATION_RAPID );
    }
}

void auto_vacc::log_state( i32 wanted, bool pop )
{
#if defined( ATLAS_DEV )
    if ( wanted == this->m_last_wanted && pop == this->m_last_pop )
    {
        return;
    }

    this->m_last_wanted = wanted;
    this->m_last_pop    = pop;

    static constexpr const char *NAMES[ MEDIGUN_NUM_RESISTS ]{ "bullet", "blast", "fire" };

    DBGOUT( "auto_vacc::run: tick %d cmd %d exec %d | want %s%s on %s | seen %.0f/%.0f/%.0f deadline %d/%d/%d | hits %d presses %d",
            g_tf2.m_globals->tickcount, g_cl.m_cmd->command_number, this->m_exec, NAMES[ wanted ], pop ? " POP" : "",
            NAMES[ this->m_effective ], this->m_needs[ 0 ].m_seen, this->m_needs[ 1 ].m_seen, this->m_needs[ 2 ].m_seen,
            this->m_needs[ 0 ].m_wanted ? this->m_needs[ 0 ].m_deadline : -1,
            this->m_needs[ 1 ].m_wanted ? this->m_needs[ 1 ].m_deadline : -1,
            this->m_needs[ 2 ].m_wanted ? this->m_needs[ 2 ].m_deadline : -1,
            static_cast< i32 >( this->m_hit_count ), static_cast< i32 >( this->m_press_count ) );
#else
    ATLAS_UNUSED_PARAM( wanted );
    ATLAS_UNUSED_PARAM( pop );
#endif
}
