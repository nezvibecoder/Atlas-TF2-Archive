#include "crits.hpp"
#include "entity_cache.hpp"

#include "gui/gui.hpp"
#include "gui/indicator.hpp"

#include "prediction.hpp"
#include "render/render.hpp"
#include "util/format.hpp"

ATLAS_NO_DESTROY crits g_crits{ };

i32 crit_totals_t::total( ) const
{
    return this->m_ranged + this->m_melee;
}

float crit_totals_t::observed( ) const
{
    const float normalized{ static_cast< float >( this->m_crit ) * ( 1.0f / 3.0f ) };
    return this->m_ranged > 0 ? normalized / ( normalized + static_cast< float >( this->m_ranged - this->m_crit ) ) : 0.0f;
}

bool crit_totals_t::banned( float threshold ) const
{
    return this->observed( ) > threshold;
}

i32 crit_totals_t::damage_required( float threshold ) const
{
    if ( !this->banned( threshold ) )
    {
        return 0;
    }

    // D >= (C/3)/k - C/3 - (T-C).
    const double  normalized{ static_cast< double >( static_cast< float >( this->m_crit ) / 3.0f ) };
    const double  solution{ normalized / threshold - normalized - ( this->m_ranged - this->m_crit ) };
    const i32     limit{ 2147483647 - this->m_ranged };
    i32           low{ };
    i32           high{ limit };
    const i32     candidate{ solution < limit ? math::bigger( static_cast< i32 >( solution ), 0 ) : limit };
    crit_totals_t trial{ *this };
    trial.m_ranged += candidate;
    if ( trial.banned( threshold ) )
    {
        low = candidate < limit ? candidate + 1 : limit;
    } else
    {
        high = candidate;
    }

    while ( low < high )
    {
        const i32 middle{ low + ( high - low ) / 2 };
        trial.m_ranged = this->m_ranged + middle;
        if ( trial.banned( threshold ) )
        {
            low = middle + 1;
        } else
        {
            high = middle;
        }
    }

    return low;
}

constexpr float HISTORY_SETTLE{ 1.25f };

void crit_history::reset( i32 previous_server, float now )
{
    const i32 round{ this->m_round + 1 };
    *this                = crit_history{ };
    this->m_round        = round;
    this->m_reset_server = previous_server;
    this->m_reset_time   = now;
    this->m_wait_server  = previous_server > 0;
}

void crit_history::settle( float now )
{
    while ( this->m_count > 0 )
    {
        const event_t &event{ this->m_pending[ this->m_head ] };
        if ( now - event.m_time < HISTORY_SETTLE && this->m_count < 256 )
        {
            break;
        }

        this->m_base.m_ranged += event.m_damage.m_ranged;
        this->m_base.m_crit += event.m_damage.m_crit;
        this->m_base.m_melee += event.m_damage.m_melee;
        this->m_head = ( this->m_head + 1 ) % 256;
        --this->m_count;
    }
}

void crit_history::add( i32 damage, bool melee, bool counted_crit, float now )
{
    if ( damage <= 0 )
    {
        return;
    }

    this->settle( now );
    const crit_totals_t delta{ melee ? 0 : damage, !melee && counted_crit ? damage : 0, melee ? damage : 0 };
    const i32           last{ ( this->m_head + this->m_count + 255 ) % 256 };
    if ( this->m_count > 0 && this->m_pending[ last ].m_time == now )
    {
        this->m_pending[ last ].m_damage.m_ranged += delta.m_ranged;
        this->m_pending[ last ].m_damage.m_crit += delta.m_crit;
        this->m_pending[ last ].m_damage.m_melee += delta.m_melee;
    } else
    {
        this->m_pending[ ( this->m_head + this->m_count ) % 256 ] = event_t{ delta, now };
        ++this->m_count;
    }
    this->m_events.m_ranged += delta.m_ranged;
    this->m_events.m_crit += delta.m_crit;
    this->m_events.m_melee += delta.m_melee;

    this->m_residual = math::bigger( this->m_residual - damage, 0 );
    this->record( );
}

void crit_history::reconcile( i32 server, float now )
{
    this->m_previous_update_start = this->m_update_start;
    this->m_update_start          = this->m_recorded;
    if ( server < 0 )
    {
        return;
    }

    if ( this->m_wait_server )
    {
        if ( server == this->m_reset_server ||
             ( server > this->m_reset_server && now - this->m_reset_time < HISTORY_SETTLE ) )
        {
            return;
        }
        this->m_wait_server = false;
    } else if ( server < this->m_server )
    {
        // ResetPlayerStats (team changes/reconnect) has no stats_resetround event;
        this->reset( 0, now );
    }

    this->m_server = server;
    this->settle( now );
    const i32 old_total{ this->m_base.total( ) };
    this->m_residual = math::smaller( this->m_residual, math::bigger( server - old_total, 0 ) );
    if ( old_total > server )
    {
        const crit_totals_t old{ this->m_base };
        this->m_base.m_melee  = static_cast< i32 >( static_cast< i64 >( old.m_melee ) * server / old_total );
        this->m_base.m_ranged = server - this->m_base.m_melee;
        this->m_base.m_crit   = old.m_ranged > 0
                                    ? static_cast< i32 >( static_cast< i64 >( old.m_crit ) * this->m_base.m_ranged / old.m_ranged )
                                    : 0;
        this->m_events.m_melee += this->m_base.m_melee - old.m_melee;
        this->m_events.m_ranged += this->m_base.m_ranged - old.m_ranged;
        this->m_events.m_crit += this->m_base.m_crit - old.m_crit;
        this->m_crit_correction = 0;

        for ( sample_t &sample : this->m_samples )
        {
            sample = sample_t{ };
        }
        this->m_previous_update_start = this->m_recorded + 1;
        this->m_update_start          = this->m_recorded + 1;
    }

    this->m_residual += math::bigger( server - this->m_events.total( ) - this->m_residual, 0 );
    this->record( );
}

void crit_history::push_state( const crit_totals_t &state )
{
    const crit_totals_t &last{ this->m_states[ static_cast< usize >( this->m_recorded + 255 ) % 256 ] };
    if ( this->m_recorded > 0 && last.m_ranged == state.m_ranged && last.m_crit == state.m_crit )
    {
        return;
    }

    this->m_states[ static_cast< usize >( this->m_recorded ) % 256 ] = state;
    ++this->m_recorded;
}

void crit_history::record( )
{
    if ( this->m_residual > 0 )
    {
        crit_totals_t events{ this->m_events };
        events.m_crit = math::smaller( math::bigger( events.m_crit + this->m_crit_correction, 0 ), events.m_ranged );
        this->push_state( events );
    }

    this->push_state( this->totals( ) );
}

crit_totals_t crit_history::totals( ) const
{
    crit_totals_t out{ this->m_events };
    out.m_ranged += this->m_residual;
    out.m_crit = math::smaller( math::bigger( out.m_crit + this->m_crit_correction, 0 ), out.m_ranged );
    return out;
}

i32 crit_totals_t::crit_for_observed( float observed ) const
{
    const double  inferred{ 3.0 * observed * this->m_ranged / ( 1.0 + 2.0 * observed ) };
    i32           best{ math::smaller( static_cast< i32 >( inferred ), this->m_ranged ) };
    crit_totals_t trial{ *this };
    trial.m_crit = best;
    const float error{ trial.observed( ) - observed };
    if ( best < this->m_ranged )
    {
        ++trial.m_crit;
        const float next{ trial.observed( ) - observed };
        if ( ( next < 0.0f ? -next : next ) < ( error < 0.0f ? -error : error ) )
        {
            ++best;
        }
    }

    return best;
}

void crit_history::sample( float observed, float now )
{
    sample_t *slot{ this->m_samples };
    for ( sample_t &entry : this->m_samples )
    {
        if ( entry.m_value < 0.0f )
        {
            slot = &entry;
            break;
        }

        if ( entry.m_time < slot->m_time )
        {
            slot = &entry;
        }
    }

    *slot = sample_t{ observed, now, math::bigger( this->m_previous_update_start - 1, 0 ) };
}

bool crit_history::explains( const sample_t &sample ) const
{
    for ( i32 index{ math::bigger( sample.m_from, this->m_recorded - 256 ) }; index < this->m_recorded; ++index )
    {
        const crit_totals_t &state{ this->m_states[ static_cast< usize >( index ) % 256 ] };
        if ( state.m_ranged > 0 && state.observed( ) == sample.m_value )
        {
            return true;
        }
    }

    return false;
}

i32 crit_history::exact_correction( const sample_t &sample ) const
{
    i32 best{ };
    for ( i32 index{ math::bigger( sample.m_from, this->m_recorded - 256 ) }; index < this->m_recorded; ++index )
    {
        const crit_totals_t &state{ this->m_states[ static_cast< usize >( index ) % 256 ] };
        crit_totals_t        fitted{ state };
        fitted.m_crit = state.crit_for_observed( sample.m_value );
        const i32 correction{ fitted.m_crit - state.m_crit };
        if ( state.m_ranged > 0 && fitted.observed( ) == sample.m_value &&
             ( best == 0 || ( correction < 0 ? -correction : correction ) < ( best < 0 ? -best : best ) ) )
        {
            best = correction;
        }
    }

    return best;
}

bool crit_history::resolve( float now )
{
    bool corrected{ };
    for ( sample_t &sample : this->m_samples )
    {
        if ( sample.m_value < 0.0f )
        {
            continue;
        }

        if ( this->explains( sample ) )
        {
            sample = sample_t{ };
            continue;
        }

        if ( now - sample.m_time < HISTORY_SETTLE )
        {
            continue;
        }

        const i32 correction{ this->exact_correction( sample ) };
        sample = sample_t{ };
        if ( correction == 0 )
        {
            continue;
        }

        for ( i32 index{ math::bigger( this->m_recorded - 256, 0 ) }; index < this->m_recorded; ++index )
        {
            crit_totals_t &state{ this->m_states[ static_cast< usize >( index ) % 256 ] };
            state.m_crit = math::smaller( math::bigger( state.m_crit + correction, 0 ), state.m_ranged );
        }

        this->m_crit_correction += correction;
        this->record( );
        corrected = true;
    }

    return corrected;
}

i32 crit_health_loss( i32 amount, i32 before, i32 after, bool feign )
{
    if ( before >= 0 && ( after <= 0 || ( feign && before >= after ) ) )
    {
        return math::smaller( math::bigger( before - math::bigger( after, 0 ), 0 ), amount );
    }
    return math::bigger( amount, 0 );
}

static netvar_hook s_observed_hook{ };
static netvar_hook s_health_hook{ };
static i32         s_round_state_offset{ };
static i32         s_mvm_offset{ };
static i32         s_next_melee_crit_offset{ };

static void observed_proxy( const CRecvProxyData *data, void *entity, void *out )
{
    s_observed_hook.call_original( data, entity, out );
    if ( g_cl.m_is_running && g_cl.m_local )
    {
        g_crits.on_observed_crit_chance( static_cast< C_TFWeaponBase * >( entity ), data->m_Value.m_Float );
    }
}

static void health_proxy( const CRecvProxyData *data, void *entity, void *out )
{
    s_health_hook.call_original( data, entity, out );
    if ( g_cl.m_is_running )
    {
        g_crits.on_health( static_cast< C_BaseEntity * >( entity ), data->m_Value.m_Int );
    }
}

bool crits::install( )
{
    s_round_state_offset     = g_netvars.get_offset( HASH_CT( "DT_TeamplayRoundBasedRulesProxy" ), HASH_CT( "m_iRoundState" ) );
    s_mvm_offset             = g_netvars.get_offset( HASH_CT( "DT_TFGameRulesProxy" ), HASH_CT( "m_bPlayingMannVsMachine" ) );
    s_next_melee_crit_offset = g_netvars.get_offset( HASH_CT( "DT_TFPlayer" ), HASH_CT( "m_iNextMeleeCrit" ) );
    if ( !s_round_state_offset || !s_mvm_offset || !s_next_melee_crit_offset )
    {
#if defined( ATLAS_DEV )
        __debugbreak( );
#endif
        return false;
    }
    return s_observed_hook.init( HASH_CT( "DT_TFWeaponBase" ), HASH_CT( "m_flObservedCritChance" ), observed_proxy ) &&
           s_health_hook.init( HASH_CT( "DT_BasePlayer" ), HASH_CT( "m_iHealth" ), health_proxy );
}

void crits::reset_round_history( )
{
    const i32 server{ g_cl.m_local && g_cl.m_resource
                          ? g_cl.m_resource->m_iDamage( )[ g_cl.m_local->entindex( ) ]
                          : 0 };
    this->m_history.reset( server, g_tf2.m_globals->realtime );
    this->m_hurt_count  = 0;
    this->m_resynced_at = -10.0f;
    for ( observed_t &sample : this->m_observed )
    {
        sample.m_pending = false;
        sample.m_known   = false;
    }
#if defined( ATLAS_DEV )
    if ( g_cfg.m_crits.m_debug )
    {
        DBGOUT( "crit history: reset round=%d previous_server=%d", this->m_history.m_round, server );
    }
#endif
}

void crits::on_player_life_change( i32 index )
{
    if ( index > 0 && index < MAX_TRACKED_PLAYERS )
    {
        this->m_health[ index ] = health_t{ };
    }
}

void crits::on_health( C_BaseEntity *entity, i32 value )
{
    const i32 index{ entity->entindex( ) };
    if ( index <= 0 || index >= MAX_TRACKED_PLAYERS )
    {
        return;
    }

    health_t &health{ this->m_health[ index ] };
    const u32 handle{ entity->GetRefEHandle( ).GetIndex( ) };
    if ( health.m_handle != handle )
    {
        health          = health_t{ };
        health.m_handle = handle;
    }
    if ( health.m_current != value )
    {
        health.m_previous = health.m_current;
        health.m_current  = value;
    }
}

void crits::on_player_hurt( IGameEvent *event )
{
    const i32 victim{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ATLAS_XORSTR( "userid" ).get( ) ) ) };
    const i32 attacker{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ATLAS_XORSTR( "attacker" ).get( ) ) ) };
    if ( victim <= 0 || victim >= MAX_TRACKED_PLAYERS )
    {
        return;
    }

    health_t &health{ this->m_health[ victim ] };
    const i32 tick{ g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick };
    const i32 after{ math::bigger( event->GetInt( ATLAS_XORSTR( "health" ).get( ) ), 0 ) };
    const i32 before{ health.m_event_tick == tick ? health.m_event_health
                      : health.m_current == after ? health.m_previous
                                                  : health.m_current };
    health.m_event_tick   = tick;
    health.m_event_health = after;
    if ( health.m_current != after )
    {
        health.m_previous = health.m_current;
        health.m_current  = after;
    }

    // Maintain health for every attacker. The previous hit on this victim may
    // have come from a teammate, a sentry, or the world.
    if ( !g_cl.m_local || attacker != g_cl.m_local->entindex( ) || victim == attacker )
    {
        return;
    }

    IClientEntity *client{ g_tf2.m_entity_list->GetClientEntity( victim ) };
    C_TFPlayer    *player{ client ? client->GetBaseEntity( )->As< C_TFPlayer * >( ) : nullptr };
    const i32      amount{ event->GetInt( ATLAS_XORSTR( "damageamount" ).get( ) ) };
    if ( amount <= 0 || !player )
    {
        return;
    }

    if ( this->m_hurt_count == 128 )
    {
        this->prepare_damage( g_tf2.m_globals->realtime );
        this->apply_damage( g_tf2.m_globals->realtime );
    }
    this->m_hurts[ this->m_hurt_count++ ] = hurt_t{
        victim, amount, before, after,
        event->GetInt( ATLAS_XORSTR( "weaponid" ).get( ) ),
        event->GetInt( ATLAS_XORSTR( "custom" ).get( ) ), player->WorldSpaceCenter( ),
        event->GetBool( ATLAS_XORSTR( "crit" ).get( ) ) || event->GetBool( ATLAS_XORSTR( "minicrit" ).get( ) ),
        player->m_bFeignDeathReady( ) || player->InCond( TF_COND_FEIGN_DEATH )
    };
}

crits::observed_t &crits::observed_for( u32 handle )
{
    observed_t *slot{ this->m_observed };
    for ( observed_t &sample : this->m_observed )
    {
        if ( sample.m_handle == handle )
        {
            return sample;
        }
        if ( sample.m_received_tick < slot->m_received_tick )
        {
            slot = &sample;
        }
    }
    *slot          = observed_t{ };
    slot->m_handle = handle;
    return *slot;
}

void crits::on_observed_crit_chance( C_TFWeaponBase *weapon, float value )
{
    if ( weapon->m_hOwnerEntity( ).Get( ) != g_cl.m_local )
    {
        return;
    }

    observed_t &sample{ this->observed_for( weapon->GetRefEHandle( ).GetIndex( ) ) };
    const i32   tick{ g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick };
    const bool  active{ g_cl.m_local->m_hActiveWeapon( ).Get( ) == weapon && this->m_active == sample.m_handle };
    sample.m_pending       = sample.m_known && sample.m_value != value && active &&
                             sample.m_round == this->m_history.m_round && tick > this->m_active_tick &&
                             value >= 0.0f && value <= 1.0f &&
                             g_tf2.m_cl->m_nDeltaTick >= 0;
    sample.m_active        = active;
    sample.m_known         = true;
    sample.m_value         = value;
    sample.m_received_tick = tick;
    sample.m_round         = this->m_history.m_round;
}

void crits::on_recreated( u32 handle )
{
    for ( observed_t &sample : this->m_observed )
    {
        if ( sample.m_handle == handle )
        {
            sample.m_pending = false;
        }
    }
}

void crits::scan_projectiles( float now )
{
    for ( projectile_t &projectile : this->m_projectiles )
    {
        projectile.m_live = false;
    }

    for ( const auto *cached : g_entity_cache.projectiles( ) )
    {
        C_BaseEntity *entity{ cached->m_entity };
        if ( !cached->networked( ) || cached->m_index <= g_tf2.m_globals->maxClients || entity->IsDormant( ) )
        {
            continue;
        }
        const ETFClassID id{ cached->m_class_id };
        C_BaseEntity    *launcher{ };
        C_BaseEntity    *owner{ entity->m_hOwnerEntity( ).Get( ) };
        bool             critical{ };
        switch ( id )
        {
            case ETFClassID::CTFGrenadePipebombProjectile:
            case ETFClassID::CTFStunBall:
            case ETFClassID::CTFProjectile_Cleaver:
            {
                C_TFGrenadePipebombProjectile *pipe{ entity->As< C_TFGrenadePipebombProjectile * >( ) };
                owner    = pipe->m_hThrower( ).Get( );
                launcher = pipe->m_hLauncher( ).Get( );
                critical = pipe->m_bCritical( );
                break;
            }
            case ETFClassID::CTFProjectile_Rocket:
            case ETFClassID::CTFProjectile_BallOfFire:
                launcher = entity->As< C_TFProjectile_Rocket * >( )->m_hLauncher( ).Get( );
                critical = entity->As< C_TFProjectile_Rocket * >( )->m_bCritical( );
                break;
            case ETFClassID::CTFProjectile_EnergyBall:
                launcher = entity->As< C_TFProjectile_Rocket * >( )->m_hLauncher( ).Get( );
                break; // energy-ball critical state is not networked
            case ETFClassID::CTFProjectile_Flare:
                launcher = entity->As< C_TFProjectile_Flare * >( )->m_hLauncher( ).Get( );
                critical = entity->As< C_TFProjectile_Flare * >( )->m_bCritical( );
                break;
            case ETFClassID::CTFProjectile_Arrow:
            case ETFClassID::CTFProjectile_HealingBolt:
                launcher = entity->As< C_TFProjectile_Arrow * >( )->m_hLauncher( ).Get( );
                critical = entity->As< C_TFProjectile_Arrow * >( )->m_bCritical( );
                break;
            default: continue;
        }

        const u32 handle{ cached->m_handle };
        if ( owner != g_cl.m_local || !launcher )
        {
            // A reflected projectile no longer belongs to our damage history.
            for ( projectile_t &projectile : this->m_projectiles )
            {
                if ( projectile.m_handle == handle )
                {
                    projectile = projectile_t{ };
                }
            }
            continue;
        }

        projectile_t *slot{ this->m_projectiles };
        for ( projectile_t &projectile : this->m_projectiles )
        {
            if ( projectile.m_handle == handle )
            {
                slot = &projectile;
                break;
            }
            if ( projectile.m_seen < slot->m_seen )
            {
                slot = &projectile;
            }
        }

        C_TFWeaponBase *weapon{ launcher->As< C_TFWeaponBase * >( ) };
        const vec3      position{ entity->m_vecOrigin( ) };
        if ( slot->m_handle != handle )
        {
            *slot            = projectile_t{ };
            slot->m_handle   = handle;
            slot->m_launcher = launcher->GetRefEHandle( ).GetIndex( );
            slot->m_weapon   = weapon->GetWeaponID( );
            slot->m_class    = id;
            slot->m_round    = this->m_history.m_round;
            // The launcher timestamp is an estimate for a coalesced launch;
            // entity handle + serial, not this time, identifies the projectile.
            slot->m_fired    = weapon->m_flLastFireTime( );
            slot->m_origin   = position;
            slot->m_radius   = id == ETFClassID::CTFGrenadePipebombProjectile || id == ETFClassID::CTFProjectile_Rocket
                                   ? weapon->GetProjectileBlastRadius( )
                                   : 32.0f;
        }
        slot->m_previous = slot->m_origin;
        slot->m_origin   = position;
        slot->m_velocity = entity->GetAbsVelocity( );
        slot->m_seen     = now;
        slot->m_crit     = critical;
        slot->m_live     = true;
    }
}

const crits::projectile_t *crits::projectile_for( const hurt_t &hurt, float now ) const
{
    const projectile_t *best{ };
    float               best_distance{ 1.0f };
    for ( const projectile_t &projectile : this->m_projectiles )
    {
        const float age{ now - projectile.m_seen };
        if ( projectile.m_handle == INVALID_EHANDLE_INDEX || age > 0.25f ||
             ( projectile.m_crit && !hurt.m_crit ) )
        {
            continue;
        }

        const vec3  end{ projectile.m_origin + projectile.m_velocity * math::smaller( age, 0.1f ) };
        const vec3  along{ end - projectile.m_previous };
        const float length{ along.length_sqr( ) };
        const float fraction{ length > 0.0f ? math::clamp( ( hurt.m_position - projectile.m_previous ).dot( along ) / length, 0.0f, 1.0f ) : 0.0f };
        const float radius{ projectile.m_radius + 48.0f };
        const float distance{ ( hurt.m_position - ( projectile.m_previous + along * fraction ) ).length_sqr( ) / ( radius * radius ) };
        const float ranked{ distance + ( projectile.m_live ? 0.25f : 0.0f ) };
        if ( ranked < best_distance )
        {
            best          = &projectile;
            best_distance = ranked;
        }
    }
    return best;
}

ATLAS_FORCE_INLINE bool ranged_custom( i32 custom )
{
    switch ( custom )
    {
        case TF_DMG_CUSTOM_HEADSHOT:
        case TF_DMG_CUSTOM_HEADSHOT_DECAPITATION:
        case TF_DMG_CUSTOM_BURNING:
        case TF_DMG_CUSTOM_BURNING_FLARE:
        case TF_DMG_CUSTOM_BURNING_ARROW:
        case TF_DMG_CUSTOM_FLYINGBURN:
        case TF_DMG_CUSTOM_BLEEDING:
        case TF_DMG_CUSTOM_STANDARD_STICKY:
        case TF_DMG_CUSTOM_AIR_STICKY_BURST:
        case TF_DMG_CUSTOM_DEFENSIVE_STICKY:
        case TF_DMG_CUSTOM_ROCKET_DIRECTHIT:
        case TF_DMG_CUSTOM_FLARE_EXPLOSION:
        case TF_DMG_CUSTOM_FLARE_PELLET:
        case TF_DMG_CUSTOM_CANNONBALL_PUSH:
        case TF_DMG_CUSTOM_CLEAVER:
        case TF_DMG_CUSTOM_CLEAVER_CRIT:
        case TF_DMG_CUSTOM_BASEBALL:
        case TF_DMG_CUSTOM_CHARGE_IMPACT: // shield bash is DMG_CLUB without DMG_MELEE
        case TF_DMG_CUSTOM_BOOTS_STOMP:
        case TF_DMG_CUSTOM_MINIGUN:
        case TF_DMG_CUSTOM_PLAYER_SENTRY:
        case TF_DMG_CUSTOM_SHOTGUN_REVENGE_CRIT:
        case TF_DMG_CUSTOM_PLASMA:
        case TF_DMG_CUSTOM_PLASMA_CHARGED:
        case TF_DMG_CUSTOM_PLASMA_GIB:
        case TF_DMG_CUSTOM_DRAGONS_FURY_IGNITE:
        case TF_DMG_CUSTOM_DRAGONS_FURY_BONUS_BURNING: return true;
        default: return false;
    }
}

bool crits::guaranteed_crit( C_TFWeaponBase *weapon ) const
{
    return g_cl.m_local->IsCritBoosted( ) ||
           ( this->classify( weapon ) == crit_type::melee &&
             g_tf2.tf_weapon_criticals_melee->GetInt( ) != 0 && weapon->AreRandomCritsEnabled( ) &&
             *reinterpret_cast< const i32 * >( reinterpret_cast< const u8 * >( g_cl.m_local ) + s_next_melee_crit_offset ) == 2 );
}

void crits::prepare_damage( float now )
{
    if ( this->m_hurt_count == 0 )
    {
        return;
    }

    const C_TFGameRules *rules{ TFGameRules( ) };
    const i32            match{ TFGameRules( )->m_nMatchGroupType( ) };
    const i32            state{ *reinterpret_cast< const i32 * >( reinterpret_cast< const u8 * >( rules ) + s_round_state_offset ) };
    const bool           mvm{ *reinterpret_cast< const bool * >( reinterpret_cast< const u8 * >( rules ) + s_mvm_offset ) };
    // @::IncrementStat suppresses matchmaking stats outside GR_STATE_RND_RUNNING.
    if ( match >= k_eTFMatchGroup_Ladder_First && match <= k_eTFMatchGroup_Casual_Last && state != 4 )
    {
        this->m_hurt_count = 0;
        return;
    }
    const bool boosted{ g_cl.m_local->IsCritBoosted( ) };
    const i32  tick{ g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick };
    for ( i32 index{ }; index < this->m_hurt_count; ++index )
    {
        hurt_t             &hurt{ this->m_hurts[ index ] };
        const projectile_t *projectile{ this->projectile_for( hurt, now ) };
        C_TFWeaponBase     *weapon{ g_cl.m_local->Weapon_OwnsThisID( hurt.m_weapon ) };
        const bool          custom{ ranged_custom( hurt.m_custom ) };
        const bool          melee_custom{ hurt.m_custom == TF_DMG_CUSTOM_BACKSTAB ||
                                          hurt.m_custom == TF_DMG_CUSTOM_STICKBOMB_EXPLOSION ||
                                          hurt.m_custom == TF_DMG_CUSTOM_DECAPITATION ||
                                          hurt.m_custom == TF_DMG_CUSTOM_PICKAXE ||
                                          hurt.m_custom == TF_DMG_CUSTOM_FISH_KILL ||
                                          hurt.m_custom == TF_DMG_CUSTOM_COMBO_PUNCH ||
                                          hurt.m_custom == TF_DMG_CUSTOM_AXTINGUISHER_BOOSTED ||
                                          hurt.m_custom == TF_DMG_CUSTOM_SLAP_KILL };
        const bool          swung{ hurt.m_weapon == this->m_last_melee_weapon && now - this->m_last_melee_time <= 1.0f };
        const bool          melee{ melee_custom || ( !custom && !projectile && swung && weapon && weapon->GetWeaponInfo( )->m_bMeleeWeapon ) };
        IClientEntity      *victim{ g_tf2.m_entity_list->GetClientEntity( hurt.m_victim ) };
        C_TFPlayer         *player{ victim ? victim->GetBaseEntity( )->As< C_TFPlayer * >( ) : nullptr };
        const bool          feign{ hurt.m_feign || ( player && ( player->m_bFeignDeathReady( ) || player->InCond( TF_COND_FEIGN_DEATH ) ) ) };
        const i32           damage{ crit_health_loss( hurt.m_amount, hurt.m_before, hurt.m_after, feign ) };
        hurt.m_projectile = melee_custom ? nullptr : projectile;
        if ( hurt.m_crit && !projectile )
        {
            for ( crit_shot_t &shot : this->m_crit_shots )
            {
                if ( shot.m_weapon == hurt.m_weapon && shot.m_tick <= tick )
                {
                    shot.m_tick = -1;
                }
            }
        }
        if ( damage <= ( mvm ? 5000 : 1500 ) )
        {
            hurt.m_damage = crit_totals_t{ melee ? 0 : damage, !melee && hurt.m_crit && !boosted ? damage : 0, melee ? damage : 0 };
        }
    }
}

void crits::apply_damage( float now )
{
    for ( i32 index{ }; index < this->m_hurt_count; ++index )
    {
        const hurt_t       &hurt{ this->m_hurts[ index ] };
        const projectile_t *projectile{ hurt.m_projectile };
        const i32           damage{ hurt.m_damage.total( ) };
        const bool          melee{ hurt.m_damage.m_melee != 0 };
#if defined( ATLAS_DEV )
        const i32 previous_residual{ this->m_history.m_residual };
#endif
        this->m_history.add( damage, melee, hurt.m_damage.m_crit != 0, now );
#if defined( ATLAS_DEV )
        if ( g_cfg.m_crits.m_debug )
        {
            DBGOUT( "crit damage: victim=%d event=%d corrected=%d hp=%d/%d weapon=%d custom=%d projectile=%u launcher=%u class=%d fire=%.3f epoch=%d stored_crit=%d melee=%d counted=%d source=%s residual=%d reconcile_delta=%d",
                    hurt.m_victim, hurt.m_amount, damage, hurt.m_before, hurt.m_after,
                    projectile ? projectile->m_weapon : hurt.m_weapon, hurt.m_custom,
                    projectile ? projectile->m_handle : INVALID_EHANDLE_INDEX,
                    projectile ? projectile->m_launcher : INVALID_EHANDLE_INDEX,
                    projectile ? static_cast< i32 >( projectile->m_class ) : -1,
                    projectile ? projectile->m_fired : 0.0f, projectile ? projectile->m_round : this->m_history.m_round,
                    projectile && projectile->m_crit, melee, !melee && hurt.m_damage.m_crit > 0,
                    ranged_custom( hurt.m_custom ) || ( melee && hurt.m_custom != 0 ) ? "custom" : projectile ? "projectile"
                                                                                                              : "event_weapon",
                    this->m_history.m_residual, this->m_history.m_residual - previous_residual );
        }
#endif
    }
    this->m_hurt_count = 0;
}

void crits::on_network_update( )
{
    if ( !g_cl.m_local || !g_cl.m_resource )
    {
        return;
    }

    const float now{ g_tf2.m_globals->realtime };
    const i32   tick{ g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick };
    const u32   active{ g_cl.m_local->m_hActiveWeapon( ).GetIndex( ) };
    if ( active != this->m_active )
    {
        this->m_active                         = active;
        this->m_active_tick                    = tick;
        this->observed_for( active ).m_pending = false;
    }

    this->scan_projectiles( now );
    this->prepare_damage( now );
#if defined( ATLAS_DEV )
    const crit_totals_t previous{ this->m_history.totals( ) };
    const i32           previous_server{ this->m_history.m_server };
#endif
    this->m_history.reconcile( g_cl.m_resource->m_iDamage( )[ g_cl.m_local->entindex( ) ], now );
#if defined( ATLAS_DEV )
    if ( g_cfg.m_crits.m_debug && ( previous_server != this->m_history.m_server ||
                                    previous.total( ) != this->m_history.totals( ).total( ) ) )
    {
        const crit_totals_t current{ this->m_history.totals( ) };
        DBGOUT( "crit reconcile: round=%d server=%d->%d T=%d->%d C=%d->%d melee=%d residual=%d delta=%d",
                this->m_history.m_round, previous_server, this->m_history.m_server,
                previous.m_ranged, current.m_ranged, previous.m_crit, current.m_crit, current.m_melee,
                this->m_history.m_residual, current.total( ) - previous.total( ) );
    }
#endif
    observed_t &sample{ this->observed_for( active ) };
    if ( sample.m_pending && sample.m_active && sample.m_round == this->m_history.m_round &&
         sample.m_received_tick == tick )
    {
        this->m_history.sample( sample.m_value, now );
    }
    for ( observed_t &entry : this->m_observed )
    {
        entry.m_pending = false;
    }

    this->apply_damage( now );
    if ( this->m_history.resolve( now ) )
    {
        this->m_resynced_at = now;
    }
    for ( i32 index{ 1 }; index <= g_tf2.m_globals->maxClients && index < MAX_TRACKED_PLAYERS; ++index )
    {
        IClientEntity *client{ g_tf2.m_entity_list->GetClientEntity( index ) };
        if ( client && !client->IsDormant( ) && this->m_health[ index ].m_handle == INVALID_EHANDLE_INDEX )
        {
            C_TFPlayer *player{ client->GetBaseEntity( )->As< C_TFPlayer * >( ) };
            this->on_health( player, player->m_iHealth( ) );
        }
    }
    if ( !g_cl.m_local->IsAlive( ) || !g_cl.m_weapon )
    {
        this->m_indicator = indicator_t{ };
    }
}

// @ src/game/shared/tf/tf_shareddefs.h and basecombatweapon_shared.cpp
// TODO: MOVE ME TO SDK.HPP
constexpr float TF_DAMAGE_CRIT_CHANCE{ 0.02f };
constexpr float TF_DAMAGE_CRIT_CHANCE_RAPID{ 0.02f };
constexpr float TF_DAMAGE_CRIT_CHANCE_MELEE{ 0.15f };
constexpr i32   WEAPON_RANDOM_RANGE{ 10000 };

constexpr i32 SEED_ATTEMPTS{ 4096 };
constexpr i32 PIPE_SPIN_TOLERANCE{ 1 };

// RemapValClamped( m_iCritMult, 0, 255, 1.0, 4.0 )
constexpr float CRIT_MULT_MIN{ 1.0f };
constexpr float CRIT_MULT_MAX{ 4.0f };

constexpr i32 BASE_COST_CHECKS{ 1 << 20 };

ATLAS_FORCE_INLINE bucket_t read_bucket( C_TFWeaponBase *weapon )
{
    return bucket_t{ weapon->m_flCritTokenBucket( ), weapon->m_nCritChecks( ), weapon->m_nCritSeedRequests( ) };
}

// @ CBaseCombatWeapon::AddToCritBucket; a full bucket takes nothing
ATLAS_FORCE_INLINE void feed_bucket( bucket_t &bucket, float damage )
{
    const float cap{ g_tf2.tf_weapon_criticals_bucket_cap->GetFloat( ) };
    if ( bucket.m_tokens < cap )
    {
        bucket.m_tokens = math::smaller( bucket.m_tokens + damage, cap );
    }
}

ATLAS_FORCE_INLINE bool withdraw_bucket( bucket_t &bucket, float damage, bool melee )
{
    ++bucket.m_requests;

    const float ratio{ static_cast< float >( bucket.m_requests ) / static_cast< float >( bucket.m_checks ) };
    const float mult{ melee ? 0.5f : math::remap_clamped( ratio, 0.1f, 1.0f, 1.0f, 3.0f ) };
    const float cost{ damage * TF_DAMAGE_CRIT_MULTIPLIER * mult };

    if ( cost > bucket.m_tokens )
    {
        return false;
    }

    bucket.m_tokens = math::bigger( bucket.m_tokens - cost, g_tf2.tf_weapon_criticals_bucket_bottom->GetFloat( ) );
    return true;
}

struct crc32_table_t
{
    u32 m_values[ 256 ]{ };

    constexpr crc32_table_t( )
    {
        for ( u32 index{ }; index < 256; ++index )
        {
            u32 value{ index };
            for ( i32 bit{ }; bit < 8; ++bit )
            {
                value = ( value >> 1 ) ^ ( ( value & 1u ) ? 0xEDB88320u : 0u );
            }

            this->m_values[ index ] = value;
        }
    }
};

constexpr crc32_table_t CRC32_TABLE{ };

ATLAS_FORCE_INLINE u32 crc32_byte( u32 crc, u32 byte )
{
    return CRC32_TABLE.m_values[ ( crc ^ byte ) & 0xFFu ] ^ ( crc >> 8 );
}

ATLAS_FORCE_INLINE i32 seed_file_line_hash( i32 seed, const char *shared_name, i32 additional )
{
    u32       crc{ 0xFFFFFFFFu };
    const u32 words[ 2 ]{ static_cast< u32 >( seed ), static_cast< u32 >( additional ) };

    for ( const u32 word : words )
    {
        for ( i32 shift{ }; shift < 32; shift += 8 )
        {
            crc = crc32_byte( crc, word >> shift );
        }
    }

    for ( ; *shared_name; ++shared_name )
    {
        crc = crc32_byte( crc, static_cast< u8 >( *shared_name ) );
    }

    return static_cast< i32 >( crc ^ 0xFFFFFFFFu );
}

void crits::clear( )
{
    this->m_indicator         = indicator_t{ };
    this->m_sim_input         = nullptr;
    this->m_sim_sequence      = -1;
    this->m_create_command    = -1;
    this->m_calc_frame        = -1;
    this->m_in_crit_calc      = false;
    this->m_ran_this_command  = false;
    this->m_history           = crit_history{ };
    this->m_resynced_at       = -10.0f;
    this->m_hurt_count        = 0;
    this->m_active            = INVALID_EHANDLE_INDEX;
    this->m_active_tick       = -1;
    this->m_last_melee_weapon = -1;
    this->m_last_melee_time   = -10.0f;
    for ( health_t &health : this->m_health )
    {
        health = health_t{ };
    }
    for ( projectile_t &projectile : this->m_projectiles )
    {
        projectile = projectile_t{ };
    }
    for ( crit_shot_t &shot : this->m_crit_shots )
    {
        shot = crit_shot_t{ };
    }
    for ( observed_t &sample : this->m_observed )
    {
        sample = observed_t{ };
    }

    for ( batch_slot_t &slot : this->m_batches )
    {
        slot = batch_slot_t{ };
    }

    for ( weapon_state_t &state : this->m_weapon_states )
    {
        state = weapon_state_t{ };
    }
}

crits::crit_type crits::classify( C_TFWeaponBase *weapon ) const
{
    CTFWeaponInfo *info{ weapon->GetWeaponInfo( ) };
    if ( info->m_bMeleeWeapon )
    {
        return crit_type::melee;
    }

    return info->GetWeaponData( this->weapon_mode( weapon ) ).m_bUseRapidFireCrits ? crit_type::rapid_fire
                                                                                   : crit_type::regular;
}

i32 crits::weapon_mode( C_TFWeaponBase *weapon ) const
{
    // every path that reaches the crit roll sets primary mode immediately before it
    // ( tf_weaponbase.cpp PrimaryAttack, tf_weaponbase_melee.cpp Swing, and the per-weapon
    // overrides ), while CTFWeaponBase::SecondaryAttack sets secondary and returns without
    // attacking. So a live read here can hand back a 1 left over from a right-click, and
    // GetWeaponData( 1 ) is usually zero-filled, which prices the whole withdrawal at nothing and
    // makes the mirror claim a free crit the server will charge for.
    //
    // the fists are the exception worth keeping: CTFFists::Punch calls Swing directly instead of
    // going through the melee PrimaryAttack that resets the mode, so their secondary punch really
    // does roll in secondary mode;
    if ( weapon->GetWeaponID( ) == TF_WEAPON_FISTS )
    {
        return math::clamp( weapon->m_iWeaponMode( ), 0, 1 );
    }

    return 0;
}

float crits::crit_chance( C_TFWeaponBase *weapon, float crit_mult ) const
{
    switch ( this->classify( weapon ) )
    {
        case crit_type::regular:
            return g_cl.attrib_hook( weapon, TF_DAMAGE_CRIT_CHANCE * crit_mult,
                                     ATLAS_XORSTR( "mult_crit_chance" ).get( ) );

        case crit_type::rapid_fire:
        {
            // the rapid-fire path does not roll a per-shot chance; it rolls a per-second chance to
            // START a fixed crit window, remapped so the average over time matches the base chance
            const float total{ math::clamp( TF_DAMAGE_CRIT_CHANCE_RAPID * crit_mult, 0.01f, 0.99f ) };
            const float non_crit{ ( TF_DAMAGE_CRIT_DURATION_RAPID / total ) - TF_DAMAGE_CRIT_DURATION_RAPID };

            return g_cl.attrib_hook( weapon, 1.0f / non_crit, ATLAS_XORSTR( "mult_crit_chance" ).get( ) );
        }

        case crit_type::melee:
            return g_cl.attrib_hook( weapon, TF_DAMAGE_CRIT_CHANCE_MELEE * crit_mult,
                                     ATLAS_XORSTR( "mult_crit_chance" ).get( ) );

        default: return 0.0f;
    }
}

float crits::shot_damage( C_TFWeaponBase *weapon, bool for_withdraw ) const
{
    const crit_type     type{ this->classify( weapon ) };
    const WeaponData_t &data{ weapon->GetWeaponInfo( )->GetWeaponData( this->weapon_mode( weapon ) ) };

    float damage{ g_cl.attrib_hook( weapon, static_cast< float >( data.m_nDamage ),
                                    ATLAS_XORSTR( "mult_dmg" ).get( ) ) };

    if ( type != crit_type::melee )
    {
        i32 projectiles{ data.m_nBulletsPerShot };
        projectiles = projectiles >= 1
                          ? static_cast< i32 >( g_cl.attrib_hook( weapon, static_cast< float >( projectiles ),
                                                                  ATLAS_XORSTR( "mult_bullets_per_shot" ).get( ) ) )
                          : 1;

        damage *= static_cast< float >( projectiles );
    }

    // a rapid-fire window is paid for up front: the whole 2 s of fire, capped at the bucket
    if ( for_withdraw && type == crit_type::rapid_fire )
    {
        const float cap{ static_cast< float >( g_tf2.tf_weapon_criticals_bucket_cap->GetInt( ) ) };

        damage *= TF_DAMAGE_CRIT_DURATION_RAPID / data.m_flTimeFireDelay;
        if ( damage * TF_DAMAGE_CRIT_MULTIPLIER > cap )
        {
            damage = cap / TF_DAMAGE_CRIT_MULTIPLIER;
        }
    }

    return damage;
}

bool crits::weapon_can_crit( C_TFWeaponBase *weapon ) const
{
    const bool melee{ this->classify( weapon ) == crit_type::melee };
    if ( melee )
    {
        const i32 melee_mode{ g_tf2.tf_weapon_criticals_melee->GetInt( ) };
        if ( melee_mode == 0 || ( melee_mode == 1 && !weapon->AreRandomCritsEnabled( ) ) )
        {
            return false;
        }
    } else if ( !weapon->AreRandomCritsEnabled( ) )
    {
        return false;
    }

    if ( g_cl.attrib_hook( weapon, 1.0f, ATLAS_XORSTR( "mult_crit_chance" ).get( ) ) <= 0.0f )
    {
        return false;
    }

    switch ( weapon->GetWeaponID( ) )
    {
        case TF_WEAPON_REVOLVER:
        case TF_WEAPON_SMG:
        case TF_WEAPON_CHARGED_SMG:
            if ( static_cast< i32 >( g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "set_weapon_mode" ).get( ) ) ) == 1 )
            {
                return false;
            }
            break;
        case TF_WEAPON_COMPOUND_BOW:
        case TF_WEAPON_KNIFE:
        case TF_WEAPON_LUNCHBOX:
        case TF_WEAPON_MEDIGUN:
        case TF_WEAPON_FLAME_BALL:
        case TF_WEAPON_PDA:
        case TF_WEAPON_PDA_ENGINEER_BUILD:
        case TF_WEAPON_PDA_ENGINEER_DESTROY:
        case TF_WEAPON_PDA_SPY:
        case TF_WEAPON_PDA_SPY_BUILD:
        case TF_WEAPON_BUILDER:
        case TF_WEAPON_LASER_POINTER:
        case TF_WEAPON_BUFF_ITEM:
        case TF_WEAPON_INVIS:
        case TF_WEAPON_JAR:
        case TF_WEAPON_JAR_MILK:
        case TF_WEAPON_JAR_GAS:
        case TF_WEAPON_SNIPERRIFLE:
        case TF_WEAPON_SNIPERRIFLE_DECAP:
        case TF_WEAPON_SNIPERRIFLE_CLASSIC:
        case TF_WEAPON_ROCKETPACK:
        case TF_WEAPON_PASSTIME_GUN:
        case TF_WEAPON_GRAPPLINGHOOK: return false;
        default: break;
    }

    return this->crit_chance( weapon, g_cl.m_local->GetCritMult( ) ) > 0.0f;
}

float crits::observed_chance( ) const
{
    return this->m_history.totals( ).observed( );
}

crit_totals_t crits::pending_totals( ) const
{
    crit_totals_t totals{ this->m_history.totals( ) };
    const i32     tick{ g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick };
    i32           pending{ };
    for ( const crit_shot_t &shot : this->m_crit_shots )
    {
        if ( shot.m_tick >= tick )
        {
            pending += shot.m_damage;
        }
    }

    totals.m_ranged += pending;
    totals.m_crit += pending;
    return totals;
}

bool crits::is_crit_banned( C_TFWeaponBase *weapon ) const
{
    return this->classify( weapon ) != crit_type::melee &&
           this->m_history.totals( ).banned( this->crit_chance( weapon, g_cl.m_local->GetCritMult( ) ) + 0.1f );
}

i32 crits::damage_to_lift_ban( C_TFWeaponBase *weapon ) const
{
    return this->m_history.totals( ).damage_required( this->crit_chance( weapon, g_cl.m_local->GetCritMult( ) ) + 0.1f );
}

bool crits::bucket_pays( C_TFWeaponBase *weapon ) const
{
    bucket_t bucket{ read_bucket( weapon ) };

    feed_bucket( bucket, this->shot_damage( weapon, false ) );
    ++bucket.m_checks;

    return withdraw_bucket( bucket, this->shot_damage( weapon, true ), this->classify( weapon ) == crit_type::melee );
}

i32 crits::crits_paid( C_TFWeaponBase *weapon, bucket_t bucket ) const
{
    const bool  melee{ this->classify( weapon ) == crit_type::melee };
    const float base{ this->shot_damage( weapon, false ) };
    const float withdraw{ this->shot_damage( weapon, true ) };

    i32 available{ };
    for ( ; available < 256; ++available )
    {
        feed_bucket( bucket, base );
        ++bucket.m_checks;

        if ( !withdraw_bucket( bucket, withdraw, melee ) )
        {
            break;
        }
    }

    return available;
}

float crits::last_crit_check( C_TFWeaponBase *weapon ) const
{
    return g_tf2.tf_weapon_criticals_nopred->GetBool( ) ? weapon->m_flLastRapidFireCritCheckTime( )
                                                        : weapon->m_flLastCritCheckTime( );
}

bool crits::seed_conflict( ) const
{
    return g_cfg.m_exploits.m_seed_prediction && g_tf2.sv_usercmd_custom_random_seed &&
           !g_tf2.sv_usercmd_custom_random_seed->GetBool( );
}

// the launch impulse is AngularImpulse( 600, RandomInt( -1200, 1200 ), 0 ) about the projectile's
// local axes, so x is roll along the pill's own length; invisible on a body that is nearly
// symmetric about it; while y is the end-over-end tumble that actually reads as spin; Only y is
// random, and it is drawn from the global vstdlib stream ( VEngineRandom001 forwards straight into
// the free RandomInt ); so a command whose seed rolls y near zero makes the *server* launch a pill
// that does not tumble.
//
// the anchor is not the crit roll: PlayWeaponShootSound runs first and picks its wave through
// SharedRandomInt( "SelectWeightedSequence" ), which reseeds the stream from
// m_nPredictionRandomSeed and wipes whatever the crit check left; from that reseed the server takes
// six draws inside the sound path, then the two velocity jitter floats, then the impulse;
// this also means our crithack unfortunately goes kaboom.
void crits::seed_launch_random( i32 command, bool skip_angles ) const
{
    const i32 seed{ g_tf2.m_MD5_PseudoRandom.call_as< i32 >( command ) & INT_MAX };
    g_tf2.m_random.SetSeed( seed_file_line_hash( seed, ATLAS_XORSTR( "SelectWeightedSequence" ).get( ), 0 ) );

    for ( i32 draw{ }; draw < 6; ++draw )
    {
        g_tf2.m_random.RandomFloat( 0.0f, 1.0f );
    }
    for ( i32 draw{ }; draw < ( skip_angles ? 3 : 0 ); ++draw )
    {
        g_tf2.m_random.RandomInt( 0, 0x7FFF );
    }
}

crits::pipe_launch_t crits::pipe_launch_for( i32 command, bool skip_angles ) const
{
    this->seed_launch_random( command, skip_angles );

    pipe_launch_t launch{ };
    launch.m_jitter_up    = g_tf2.m_random.RandomFloat( -10.0f, 10.0f );
    launch.m_jitter_right = g_tf2.m_random.RandomFloat( -10.0f, 10.0f );
    launch.m_spin         = g_tf2.m_random.RandomInt( -1200, 1200 );

    return launch;
}

i32 crits::find_crit_command( C_TFWeaponBase *weapon, i32 start, bool want_crit ) const
{
    const i32 weapon_index{ weapon->entindex( ) };
    const i32 local_index{ g_cl.m_local->entindex( ) };
    const i32 mask{ this->classify( weapon ) == crit_type::melee ? ( weapon_index << 16 ) | ( local_index << 8 )
                                                                 : ( weapon_index << 8 ) | local_index };

    const float threshold{ this->crit_chance( weapon, want_crit ? CRIT_MULT_MIN : CRIT_MULT_MAX ) *
                           static_cast< float >( WEAPON_RANDOM_RANGE ) };

    for ( i32 number{ start }; number < start + SEED_ATTEMPTS; ++number )
    {
        const i32 seed{ ( g_tf2.m_MD5_PseudoRandom.call_as< i32 >( number ) & INT_MAX ) ^ mask };
        if ( seed == weapon->m_iCurrentSeed( ) )
        {
            continue;
        }

        g_tf2.m_random.SetSeed( seed );

        const bool crit{ static_cast< float >( g_tf2.m_random.RandomInt( 0, WEAPON_RANDOM_RANGE - 1 ) ) < threshold };
        if ( crit == want_crit )
        {
            return number;
        }
    }

    return 0;
}

i32 crits::find_pipe_command( i32 start, bool avoid_crit ) const
{
    const bool angular{ g_cl.attrib_hook( g_cl.m_weapon, 0.0f,
                                          ATLAS_XORSTR( "projectile_spread_angle" ).get( ) ) > 0.0f };
    for ( i32 number{ start }; number < start + SEED_ATTEMPTS; ++number )
    {
        const i32 spin{ this->pipe_launch_for( number, angular ).m_spin };
        if ( spin < -PIPE_SPIN_TOLERANCE || spin > PIPE_SPIN_TOLERANCE )
        {
            continue;
        }

        if ( avoid_crit && this->find_crit_command( g_cl.m_weapon, number, false ) != number )
        {
            continue;
        }

        return number;
    }

    return 0;
}

bool crits::steer_pipe( )
{
    if ( !g_cfg.m_pills.m_seed_spin || !g_cl.m_local->IsAlive( ) ||
         g_cl.m_weapon->GetWeaponID( ) != TF_WEAPON_GRENADELAUNCHER ||
         g_cl.attrib_hook( g_cl.m_weapon, 0.0f, ATLAS_XORSTR( "grenade_no_spin" ).get( ) ) != 0.0f || // loch-n-load
         !g_cl.will_fire_this_tick( ) )
    {
        return false;
    }

    const bool avoid_crit{ g_cfg.m_crits.m_enabled && g_cfg.m_crits.m_avoid && !this->seed_conflict( ) &&
                           !this->guaranteed_crit( g_cl.m_weapon ) && this->weapon_can_crit( g_cl.m_weapon ) };

    const i32 command{ this->find_pipe_command( g_cl.m_cmd->command_number, avoid_crit ) };
    if ( command <= 0 )
    {
        return false;
    }

    g_cl.m_cmd->command_number = command;
    g_cl.m_cmd->random_seed    = g_tf2.m_MD5_PseudoRandom.call_as< i32 >( command ) & INT_MAX;
    g_tf2.m_CBaseEntity_SetPredictionRandomSeed.call_as< void >( g_cl.m_cmd );

    return true;
}

bool crits::steer_crit( )
{
    if ( this->seed_conflict( ) || !g_cl.m_local->IsAlive( ) || this->guaranteed_crit( g_cl.m_weapon ) ||
         !this->weapon_can_crit( g_cl.m_weapon ) || !g_cl.will_fire_this_tick( ) )
    {
        return false;
    }

    const bool key_held{ gui::bind_active( g_cfg.m_crits.m_key ) };
    const bool melee_swing{ g_cfg.m_melee.m_always_crit && this->classify( g_cl.m_weapon ) == crit_type::melee };
    const bool banned{ this->is_crit_banned( g_cl.m_weapon ) };
    const bool unsettled{ !banned && this->classify( g_cl.m_weapon ) != crit_type::melee &&
                          this->pending_totals( ).banned( this->crit_chance( g_cl.m_weapon, g_cl.m_local->GetCritMult( ) ) + 0.1f ) };
    const bool want_crit{ ( ( g_cfg.m_crits.m_force && key_held ) || melee_swing ) && !banned && !unsettled &&
                          this->bucket_pays( g_cl.m_weapon ) };

    if ( !want_crit && !g_cfg.m_crits.m_avoid && !unsettled )
    {
        return false;
    }

    const i32 command{ this->find_crit_command( g_cl.m_weapon, g_cl.m_cmd->command_number, want_crit ) };
    if ( command <= 0 )
    {
        return false;
    }

    g_cl.m_cmd->command_number = command;
    g_cl.m_cmd->random_seed    = g_tf2.m_MD5_PseudoRandom.call_as< i32 >( command ) & INT_MAX;
    g_tf2.m_CBaseEntity_SetPredictionRandomSeed.call_as< void >( g_cl.m_cmd );

#if defined( ATLAS_DEV )
    if ( g_cfg.m_crits.m_debug )
    {
        const crit_type type{ this->classify( g_cl.m_weapon ) };
        const i32       mask{ type == crit_type::melee
                                  ? ( g_cl.m_weapon->entindex( ) << 16 ) | ( g_cl.m_local->entindex( ) << 8 )
                                  : ( g_cl.m_weapon->entindex( ) << 8 ) | g_cl.m_local->entindex( ) };
        const i32       seed{ g_cl.m_cmd->random_seed ^ mask };
        g_tf2.m_random.SetSeed( seed );
        const i32           roll{ g_tf2.m_random.RandomInt( 0, WEAPON_RANDOM_RANGE - 1 ) };
        const crit_totals_t history{ this->m_history.totals( ) };
        DBGOUT( "crit attempt: cmd=%d weapon=%d path=%d force=%d seed=%d roll=%d bucket=%.3f checks=%d requests=%d T=%d C=%d melee=%d server=%d residual=%d mult=%.6f threshold=%.9f observed=%.9f banned=%d owed=%d round=%d frame=%d source=events+server+active_sample",
                command, g_cl.m_weapon->GetWeaponID( ), static_cast< i32 >( type ), want_crit, seed, roll,
                g_cl.m_weapon->m_flCritTokenBucket( ), g_cl.m_weapon->m_nCritChecks( ), g_cl.m_weapon->m_nCritSeedRequests( ),
                history.m_ranged, history.m_crit, history.m_melee, this->m_history.m_server, this->m_history.m_residual,
                g_cl.m_local->GetCritMult( ), this->crit_chance( g_cl.m_weapon, g_cl.m_local->GetCritMult( ) ) + 0.1f,
                history.observed( ), banned, banned ? this->damage_to_lift_ban( g_cl.m_weapon ) : 0,
                this->m_history.m_round, g_tf2.m_globals->framecount );
    }
#endif

    return want_crit;
}

void crits::guard_attack_buttons( )
{
    if ( !g_cfg.m_crits.m_enabled || !g_cl.m_local->IsAlive( ) || !( g_cl.m_cmd->buttons & IN_ATTACK ) )
    {
        return;
    }

    if ( g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_MINIGUN )
    {
        g_cl.m_cmd->buttons &= ~IN_ATTACK2;
    }

    if ( g_cl.m_weapon->m_iClip1( ) == 0 &&
         g_cl.attrib_hook( g_cl.m_weapon, 0.0f, ATLAS_XORSTR( "auto_fires_full_clip" ).get( ) ) == 0.0f )
    {
        g_cl.m_cmd->buttons &= ~IN_ATTACK;
    }
}

void crits::run( )
{
    if ( this->m_ran_this_command )
    {
        return;
    }

    this->m_ran_this_command = true;
    this->guard_attack_buttons( );

    if ( g_cfg.m_crits.m_enabled && g_cfg.m_crits.m_indicator )
    {
        this->update_indicator( );
    }

    const bool crit_first{ g_cfg.m_crits.m_enabled && g_cfg.m_pills.m_crit_priority == PILL_CRIT_PREFER_CRIT };
    if ( !crit_first && this->steer_pipe( ) )
    {
        return;
    }

    if ( g_cfg.m_crits.m_enabled && !this->steer_crit( ) && crit_first )
    {
        this->steer_pipe( );
    }
}

void crits::on_create_move( i32 command )
{
    const i32  frame{ g_tf2.m_globals->framecount };
    const bool follows{ command == this->m_create_command || command == this->m_create_command + 1 };
    if ( !follows || ( g_tf2.m_cl->chokedcommands == 0 && frame != this->m_create_frame ) )
    {
        this->m_open_batch = command;
    }

    this->m_ran_this_command                                                = false;
    this->m_create_command                                                  = command;
    this->m_create_frame                                                    = frame;
    this->m_batches[ static_cast< usize >( command ) % MULTIPLAYER_BACKUP ] = batch_slot_t{ command, this->m_open_batch };
}

i32 crits::batch_of( i32 sequence ) const
{
    const batch_slot_t &slot{ this->m_batches[ static_cast< usize >( sequence ) % MULTIPLAYER_BACKUP ] };
    return slot.m_sequence == sequence ? slot.m_batch : sequence;
}

void crits::on_run_simulation( i32 sequence, CUserCmd *command )
{
    this->m_sim_input    = sequence > 0 ? command : nullptr;
    this->m_sim_sequence = this->m_sim_input ? sequence : -1;
}

void crits::begin_crit_calc( C_TFWeaponBase *weapon )
{
    const i32 frame{ g_tf2.m_globals->framecount };
    const i32 sequence{ g_prediction.m_is_active ? this->m_create_command : this->m_sim_sequence };

    this->m_in_crit_calc = true;
    this->m_calc_frame   = sequence > 0 ? this->batch_of( sequence ) : -frame;

#if defined( ATLAS_DEV )
    if ( g_cfg.m_crits.m_debug )
    {
        this->m_log_checks    = weapon->m_nCritChecks( );
        this->m_log_seed      = weapon->m_iCurrentSeed( );
        this->m_log_inherited = weapon->m_iLastCritCheckFrame( ) == this->m_calc_frame;
    }
#endif

    weapon->m_iLastCritCheckFrame( )  = weapon->m_iLastCritCheckFrame( ) == this->m_calc_frame ? frame : frame ^ 1;
    weapon->m_flObservedCritChance( ) = this->observed_chance( );
}

void crits::end_crit_calc( C_TFWeaponBase *weapon )
{
    weapon->m_iLastCritCheckFrame( ) = this->m_calc_frame;
    this->m_in_crit_calc             = false;

    if ( !g_prediction.m_is_active )
    {
        this->save_weapon_state( weapon );
        if ( this->m_sim_input && g_tf2.m_prediction->m_bFirstTimePredicted && this->classify( weapon ) == crit_type::melee )
        {
            this->m_last_melee_weapon = weapon->GetWeaponID( );
            this->m_last_melee_time   = g_tf2.m_globals->realtime;
        }
        if ( this->m_sim_input && g_tf2.m_prediction->m_bFirstTimePredicted && weapon->m_bCurrentAttackIsCrit( ) &&
             this->classify( weapon ) != crit_type::melee && !this->guaranteed_crit( weapon ) &&
             weapon->GetWeaponInfo( )->GetWeaponData( this->weapon_mode( weapon ) ).m_iProjectile == TF_PROJECTILE_BULLET )
        {
            const i32 damage{ static_cast< i32 >( this->shot_damage( weapon, false ) * TF_DAMAGE_CRIT_MULTIPLIER ) };
            this->m_crit_shots[ this->m_crit_shot_next++ % 64 ] = crit_shot_t{ TIME_TO_TICKS( g_tf2.m_globals->curtime ), damage, weapon->GetWeaponID( ) };
        }
    }
#if defined( ATLAS_DEV )
    if ( g_cfg.m_crits.m_debug && !g_prediction.m_is_active && g_tf2.m_prediction->m_bFirstTimePredicted )
    {
        i32 roll{ -1 };
        if ( weapon->m_nCritChecks( ) != this->m_log_checks && weapon->m_iCurrentSeed( ) != this->m_log_seed )
        {
            g_tf2.m_random.SetSeed( weapon->m_iCurrentSeed( ) );
            roll = g_tf2.m_random.RandomInt( 0, WEAPON_RANDOM_RANGE - 1 );
        }
        const crit_totals_t history{ this->m_history.totals( ) };
        const float         threshold{ this->crit_chance( weapon, g_cl.m_local->GetCritMult( ) ) + 0.1f };
        const bool          banned{ this->is_crit_banned( weapon ) };
        DBGOUT( "crit native: cmd=%d weapon=%d path=%d inherited=%d seed=%d roll=%d result=%d bucket=%.3f checks=%d requests=%d T=%d C=%d server=%d mult=%.6f threshold=%.9f observed=%.9f banned=%d owed=%d source=round_estimate",
                this->m_sim_input ? this->m_sim_input->command_number : -1,
                weapon->GetWeaponID( ), static_cast< i32 >( this->classify( weapon ) ), this->m_log_inherited,
                weapon->m_iCurrentSeed( ), roll, weapon->m_bCurrentAttackIsCrit( ), weapon->m_flCritTokenBucket( ),
                weapon->m_nCritChecks( ), weapon->m_nCritSeedRequests( ), history.m_ranged, history.m_crit,
                this->m_history.m_server, g_cl.m_local->GetCritMult( ), threshold, history.observed( ),
                banned, banned ? history.damage_required( threshold ) : 0 );
    }
#endif
}

bool crits::wants_check_increment( ) const
{
    return this->m_in_crit_calc && ( g_prediction.m_is_active || this->m_sim_input );
}

void crits::save_weapon_state( C_TFWeaponBase *weapon )
{
    const u32       handle{ weapon->GetRefEHandle( ).GetIndex( ) };
    weapon_state_t *slot{ this->m_weapon_states };

    for ( weapon_state_t &state : this->m_weapon_states )
    {
        if ( state.m_handle == handle )
        {
            slot = &state;
            break;
        }

        if ( state.m_saved_tick < slot->m_saved_tick )
        {
            slot = &state;
        }
    }

    *slot = weapon_state_t{ handle,
                            g_tf2.m_globals->tickcount,
                            weapon->m_flCritTokenBucket( ),
                            weapon->m_nCritChecks( ),
                            weapon->m_nCritSeedRequests( ),
                            weapon->m_iCurrentSeed( ),
                            weapon->m_flCritTime( ),
                            weapon->m_flLastRapidFireCritCheckTime( ),
                            weapon->m_iLastCritCheckFrame( ),
                            weapon->m_bCurrentAttackIsCrit( ) };
}

// @::InitPredictable just ran on a rebuilt entity, which starts from the constructor's bucket while the
// server keeps the old one; the same index and serial means the server entity never went away.
void crits::on_init_predictable( C_BaseEntity *entity )
{
    const u32 handle{ entity->GetRefEHandle( ).GetIndex( ) };
    this->on_recreated( handle );

    for ( const weapon_state_t &state : this->m_weapon_states )
    {
        if ( state.m_handle != handle )
        {
            continue;
        }

        C_TFWeaponBase *weapon{ entity->As< C_TFWeaponBase * >( ) };
        weapon->m_flCritTokenBucket( )            = state.m_bucket;
        weapon->m_nCritChecks( )                  = state.m_checks;
        weapon->m_nCritSeedRequests( )            = state.m_requests;
        weapon->m_iCurrentSeed( )                 = state.m_seed;
        weapon->m_flCritTime( )                   = state.m_crit_time;
        weapon->m_flLastRapidFireCritCheckTime( ) = state.m_last_rapid_check;
        weapon->m_iLastCritCheckFrame( )          = state.m_last_frame;
        weapon->m_bCurrentAttackIsCrit( )         = state.m_attack_crit;
        return;
    }
}

void crits::update_indicator( )
{
    indicator_t view{ };
    view.m_resynced = g_tf2.m_globals->realtime - this->m_resynced_at < 1.5f;

    if ( g_cl.m_local->IsAlive( ) && this->weapon_can_crit( g_cl.m_weapon ) )
    {
        const crit_type type{ this->classify( g_cl.m_weapon ) };
        const float     window{ g_cl.m_weapon->m_flCritTime( ) - TICKS_TO_TIME( g_cl.m_local->m_nTickBase( ) ) };

        const bucket_t live{ read_bucket( g_cl.m_weapon ) };
        const float    cap{ g_tf2.tf_weapon_criticals_bucket_cap->GetFloat( ) };

        const i32 at_ramp{ this->crits_paid( g_cl.m_weapon, bucket_t{ cap, live.m_checks, live.m_requests } ) };
        view.m_fill      = math::clamp( live.m_tokens / cap, 0.0f, 1.0f );
        view.m_potential = at_ramp > 0 ? at_ramp : this->crits_paid( g_cl.m_weapon, bucket_t{ cap, BASE_COST_CHECKS, 0 } );
        view.m_available = this->crits_paid( g_cl.m_weapon, live );

        if ( type == crit_type::rapid_fire && window > 0.0f )
        {
            view.m_status = crit_status::streaming_window;
            view.m_window = window;
            view.m_fill   = math::clamp( window / TF_DAMAGE_CRIT_DURATION_RAPID, 0.0f, 1.0f );
        } else if ( type != crit_type::melee && this->is_crit_banned( g_cl.m_weapon ) )
        {
            view.m_status     = crit_status::observed_limit;
            view.m_ban_damage = this->damage_to_lift_ban( g_cl.m_weapon );
        } else if ( type == crit_type::rapid_fire &&
                    TICKS_TO_TIME( g_cl.m_local->m_nTickBase( ) ) < this->last_crit_check( g_cl.m_weapon ) + 1.0f )
        {
            view.m_status = crit_status::streaming_wait;
            view.m_window = this->last_crit_check( g_cl.m_weapon ) + 1.0f - TICKS_TO_TIME( g_cl.m_local->m_nTickBase( ) );
        } else if ( view.m_available == 0 && this->crits_paid( g_cl.m_weapon, bucket_t{ live.m_tokens, BASE_COST_CHECKS, 0 } ) > 0 )
        {
            view.m_status = crit_status::too_expensive;
        } else
        {
            view.m_status = view.m_available > 0 ? crit_status::ready : crit_status::low_bucket;
        }
    }

    if ( g_cl.m_local->IsAlive( ) && this->guaranteed_crit( g_cl.m_weapon ) )
    {
        view.m_status = crit_status::crit_boosted;
    }

    if ( this->seed_conflict( ) )
    {
        view.m_status = crit_status::seed_conflict;
    }

    this->m_indicator = view;
}

static indicator_card_t s_indicator{ };
static u8               s_status_mix[ static_cast< usize >( crits::crit_status::count ) ]{ };

struct status_look_t
{
    const char *m_label{ };
    const char *m_glyph{ };
    color_t     m_text{ };
    color_t     m_ink{ };
};

static status_look_t status_look( crits::crit_status status )
{
    switch ( status )
    {
        case crits::crit_status::ready: return status_look_t{ ATLAS_XORSTR_STATIC( "ready" ), ICON_SPARKLES, g_theme.m_accent, g_theme.m_accent };
        case crits::crit_status::low_bucket: return status_look_t{ ATLAS_XORSTR_STATIC( "low bucket" ), ICON_SPARKLES, MENU_TEXT_DIM, g_theme.m_input_focus_border };
        case crits::crit_status::too_expensive: return status_look_t{ ATLAS_XORSTR_STATIC( "too expensive" ), ICON_SPARKLES, MENU_TEXT_DIM, g_theme.m_input_focus_border };
        case crits::crit_status::streaming_window: return status_look_t{ ATLAS_XORSTR_STATIC( "crit window" ), ICON_FLAME, g_theme.m_accent, g_theme.m_accent };
        case crits::crit_status::streaming_wait: return status_look_t{ ATLAS_XORSTR_STATIC( "waiting" ), ICON_SPARKLES, MENU_TEXT_DIM, g_theme.m_input_focus_border };
        case crits::crit_status::crit_boosted: return status_look_t{ ATLAS_XORSTR_STATIC( "boosted" ), ICON_ZAP, g_theme.m_accent, g_theme.m_accent };
        case crits::crit_status::observed_limit: return status_look_t{ ATLAS_XORSTR_STATIC( "banned" ), ICON_SPARKLES, STATUS_WARNING, STATUS_WARNING };
        case crits::crit_status::seed_conflict: return status_look_t{ ATLAS_XORSTR_STATIC( "seed pred" ), ICON_SETTINGS, CONTROL_VALUE, MENU_TEXT_DIM };
        default: return status_look_t{ ATLAS_XORSTR_STATIC( "no crits" ), ICON_CROSS, MENU_TEXT_DIM, MENU_TEXT_DIM };
    }
}

ATLAS_FORCE_INLINE u8 status_mix( crits::crit_status status )
{
    return gui::eased_mix( s_status_mix[ static_cast< usize >( status ) ] );
}

void crits::paint( ) const
{
    const bool wanted{ g_cfg.m_crits.m_enabled && g_cfg.m_crits.m_indicator && g_tf2.m_engine->IsInGame( ) &&
                       g_cl.m_local };

    if ( !wanted )
    {
        hide_indicator( s_indicator );

        for ( u8 &mix : s_status_mix )
        {
            mix = 0;
        }

        return;
    }

    const indicator_t view{ g_cl.m_weapon ? this->m_indicator : indicator_t{ } };
    s_indicator.m_at = g_cfg.m_misc.m_crit_indicator_position;
    const u32 delta{ step_indicator( s_indicator, view.m_fill, g_cl.m_screen_width, g_cl.m_screen_height ) };
    g_cfg.m_misc.m_crit_indicator_position = s_indicator.m_at;

    for ( usize index{ }; index < static_cast< usize >( crit_status::count ); ++index )
    {
        gui::step_mix( s_status_mix[ index ], index == static_cast< usize >( view.m_status ), delta,
                       WIDGET_TOGGLE_DURATION );
    }

    const u8 hot{ static_cast< u8 >( math::smaller( status_mix( crit_status::ready ) +
                                                        status_mix( crit_status::streaming_window ) +
                                                        status_mix( crit_status::crit_boosted ),
                                                    255 ) ) };
    const u8 banned{ status_mix( crit_status::observed_limit ) };

    const color_t edge{ gui::mix_color( gui::mix_color( g_theme.m_input_focus_border, g_theme.m_accent, hot ),
                                        STATUS_WARNING, banned ) };
    const color_t wash{ gui::mix_color( g_theme.m_selected_background, g_theme.m_row_selected_background, hot ) };
    const color_t amount_color{ gui::mix_color( gui::mix_color( MENU_TEXT_DIM, CHECKBOX_MARK_COLOR, hot ),
                                                STATUS_WARNING, banned ) };

    const indicator_layout_t layout{ draw_indicator( s_indicator, view.m_resynced ? ATLAS_XORSTR_STATIC( "resynced" ) : ATLAS_XORSTR_STATIC( "crits" ), wash, edge ) };
    const u8                 enter{ layout.m_enter };

    for ( usize index{ }; index < static_cast< usize >( crit_status::count ); ++index )
    {
        const crit_status status{ static_cast< crit_status >( index ) };
        const u8          mix{ status_mix( status ) };

        if ( !mix )
        {
            continue;
        }

        const status_look_t look{ status_look( status ) };
        const u8            pulse{ status == crit_status::ready ? indicator_glyph_alpha( 255 ) : u8{ 255 } };

        g_render.text( MENU_FONT_TINY( ), layout.m_state,
                       gui::with_alpha( gui::with_alpha( look.m_text, mix ), enter ), look.m_label, false,
                       render::text_align::right );
        gui::draw_icon_at( look.m_glyph, layout.m_glyph,
                           gui::with_alpha( gui::with_alpha( look.m_ink, gui::mix_channel( 0, pulse, mix ) ),
                                            enter ) );
    }

    char amount[ 16 ]{ };

    if ( view.m_status == crit_status::streaming_window || view.m_status == crit_status::streaming_wait )
    {
        str::format_to( amount, sizeof( amount ), ATLAS_XORSTR_STATIC( "{:.1f}s" ), view.m_window );
    } else if ( view.m_status == crit_status::observed_limit )
    {
        str::format_to( amount, sizeof( amount ), ATLAS_XORSTR_STATIC( "{} dmg" ), view.m_ban_damage );
    } else if ( view.m_potential > 0 )
    {
        str::format_to( amount, sizeof( amount ), ATLAS_XORSTR_STATIC( "{}/{}" ), view.m_available,
                        view.m_potential );
    }

    g_render.text( MENU_FONT_TINY( ), layout.m_amount, gui::with_alpha( amount_color, enter ), amount, false,
                   render::text_align::left );
}
