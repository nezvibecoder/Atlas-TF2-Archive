#include "player_data.hpp"
#include "entity_cache.hpp"
#include "strafe-prediction.hpp"

#include "runtime/memory.hpp"

ATLAS_NO_DESTROY player_data g_player_data{ };

constexpr float BUILDING_EXTRA_WINDOW{ 2.5f };
constexpr float PUSH_NOISE_FLOOR{ 6.0f }; // matches recover_wish_input's NOISE_FLOOR

constexpr i32 TIMESTAMP_BASE{ 100 };
constexpr i32 TIMESTAMP_WINDOW{ 32 };

// 1 << SIMULATION_TIME_WINDOW_BITS
constexpr i32 WINDOW_TICKS{ 256 };
constexpr i32 WINDOW_HALF{ 127 };

// g_RageBuffTypes[ ].m_nMaxPulses is 10 for every banner and for the phlog; the mod_buff_duration attribute can
// stretch it and is not read here. The owner pulses itself once a second, so a gap past this ends the run
constexpr float  BUFF_DURATION{ 10.0f };
constexpr double BUFF_PULSE_GAP{ 1.5 };

constexpr ETFCond VACC_CONDS[ 3 ]{
    TF_COND_MEDIGUN_UBER_BULLET_RESIST,
    TF_COND_MEDIGUN_UBER_BLAST_RESIST,
    TF_COND_MEDIGUN_UBER_FIRE_RESIST,
};

static double vacc_stamp( float simulation_time )
{
    const i32   age_ticks{ g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick - TIME_TO_TICKS( simulation_time ) };
    const float age{ TICKS_TO_TIME( age_ticks ) };
    const float duration{ g_tf2.weapon_vaccinator_resist_duration->GetFloat( ) };
    return g_tf2.m_engine->Plat_FloatTime( ) - ( age_ticks >= 0 && age <= duration ? age : 0.0f );
}

static netvar_hook s_simulation_hook{ };
static netvar_hook s_anim_hook{ };

static i32 network_base( i32 tick, i32 entity_index )
{
    return TIMESTAMP_BASE * ( ( tick - entity_index % TIMESTAMP_WINDOW ) / TIMESTAMP_BASE );
}

static i32 decoded_tick( const CRecvProxyData *data )
{
    const i32 server_tick{ g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick };
    i32       tick{ network_base( server_tick, data->m_ObjectID ) + data->m_Value.m_Int };

    while ( tick < server_tick - WINDOW_HALF )
    {
        tick += WINDOW_TICKS;
    }

    while ( tick > server_tick + WINDOW_HALF )
    {
        tick -= WINDOW_TICKS;
    }

    return tick;
}

static void repair( const CRecvProxyData *data, void *out )
{
    float    &field{ *static_cast< float * >( out ) };
    const i32 engine_tick{ TIME_TO_TICKS( field ) };
    const i32 corrected{ decoded_tick( data ) };
    const i32 slip{ corrected - engine_tick };

    if ( slip == 0 || slip % WINDOW_TICKS != 0 )
    {
        return;
    }

    DBGOUT( "player_data: entity %d decoded %d windows off, tick %d -> %d", data->m_ObjectID,
            slip / WINDOW_TICKS, engine_tick, corrected );

    field = TICKS_TO_TIME( corrected );
}

static void simulation_time_proxy( const CRecvProxyData *data, void *entity, void *out )
{
    const float previous{ *static_cast< float * >( out ) };
    s_simulation_hook.call_original( data, entity, out );
    repair( data, out );
    g_player_data.on_simulation_time( data->m_ObjectID, previous,
                                      *static_cast< float * >( out ) );
}

static void anim_time_proxy( const CRecvProxyData *data, void *entity, void *out )
{
    s_anim_hook.call_original( data, entity, out );
    repair( data, out );
}

bool player_data::install_netvar_hooks( )
{
    return s_simulation_hook.init( HASH_CT( "DT_BaseEntity" ), HASH_CT( "m_flSimulationTime" ),
                                   simulation_time_proxy ) &&
           s_anim_hook.init( HASH_CT( "DT_BaseEntity" ), HASH_CT( "m_flAnimTime" ),
                             anim_time_proxy );
}

void player_data::remove_netvar_hooks( )
{
    s_simulation_hook.shutdown( );
    s_anim_hook.shutdown( );
}

float player_data::window( )
{
    return static_cast< float >( g_cfg.m_esp.m_dormant_time );
}

static bool player_slot( i32 index )
{
    return index >= 1 && index <= g_tf2.m_globals->maxClients && index < MAX_TRACKED_PLAYERS;
}

player_record_t *player_data::player( i32 index )
{
    return player_slot( index ) ? &this->m_players[ index ] : nullptr;
}

const player_record_t *player_data::player( i32 index ) const
{
    return player_slot( index ) ? &this->m_players[ index ] : nullptr;
}

bool player_data::remembered_class( ETFClassID class_id )
{
    switch ( class_id )
    {
        case ETFClassID::CTFPlayer:
        case ETFClassID::CObjectSentrygun:
        case ETFClassID::CObjectDispenser:
        case ETFClassID::CObjectTeleporter: return true;
        default: return false;
    }
}

float player_data::class_window( ETFClassID class_id )
{
    return class_id == ETFClassID::CTFPlayer ? window( ) : window( ) + BUILDING_EXTRA_WINDOW;
}

void player_data::remember( C_BaseEntity *entity, const vec3 &origin, bool manual )
{
    const i32 index{ entity->entindex( ) };
    if ( index < 1 || index >= MAX_EDICTS )
    {
        return;
    }

    ClientClass *client_class{ entity->GetClientClass( ) };
    if ( !client_class )
    {
        return;
    }

    const ETFClassID class_id{ client_class->GetClassID( ) };
    if ( !remembered_class( class_id ) )
    {
        return;
    }

    const i32   serial{ entity->GetRefEHandle( ).GetSerialNumber( ) };
    const float expire{ g_tf2.m_globals->curtime + class_window( class_id ) };

    if ( class_id == ETFClassID::CTFPlayer )
    {
        player_record_t *record{ this->player( index ) };
        if ( !record || !this->connected( index ) || !this->alive( index ) )
        {
            return;
        }

        if ( record->m_serial != serial )
        {
            *record = player_record_t{ };
        }

        record->m_serial         = serial;
        record->m_dormant_origin = origin;
        record->m_dormant_expire = expire;
        record->m_dormant_manual = manual;
        return;
    }

    entity_record_t entry{ };
    entry.m_serial         = serial;
    entry.m_dormant_origin = origin;
    entry.m_dormant_expire = expire;
    entry.m_dormant_manual = manual;

    this->m_entities.assign( index, entry );
}

void player_data::clear_dormant( i32 index )
{
    if ( player_record_t * record{ this->player( index ) } )
    {
        record->m_dormant_expire = 0.0f;
        record->m_dormant_manual = false;
        return;
    }

    this->m_entities.erase( index );
}

void player_data::forget( i32 index )
{
    if ( player_record_t * record{ this->player( index ) } )
    {
        *record = player_record_t{ };
        return;
    }

    this->m_entities.erase( index );
}

void player_data::on_set_dormant( C_BaseEntity *entity, bool dormant )
{
    if ( !entity )
    {
        return;
    }

    if ( !dormant )
    {
        const i32 index{ entity->entindex( ) };
        const i32 serial{ entity->GetRefEHandle( ).GetSerialNumber( ) };

        bool manual{ };
        if ( const player_record_t *record{ this->player( index ) } )
        {
            manual = record->m_serial == serial && record->m_dormant_manual;
        } else if ( const entity_record_t *record{ this->m_entities.get( index ) } )
        {
            manual = record->m_serial == serial && record->m_dormant_manual;
        }

        if ( manual )
        {
            const vec3 origin{ entity->m_vecOrigin( ) };
            g_tf2.m_CBaseEntity_SetAbsOrigin.call_as< void, C_BaseEntity *, const vec3 * >(
                entity, &origin );
        }

        this->clear_dormant( index );
        return;
    }

    this->remember( entity, entity->m_vecOrigin( ) );
}

void player_data::on_start_sound( const StartSoundParams_t &params )
{
    if ( !params.fromserver || params.origin.is_zero( ) )
    {
        return;
    }

    if ( params.soundsource <= 0 || params.soundsource == g_tf2.m_engine->GetLocalPlayer( ) )
    {
        return;
    }

    IClientEntity *client_entity{ g_tf2.m_entity_list->GetClientEntity( params.soundsource ) };
    if ( !client_entity || !client_entity->IsDormant( ) )
    {
        return;
    }

    C_BaseEntity *base_entity{ client_entity->GetBaseEntity( ) };
    if ( !base_entity )
    {
        return;
    }

    ClientClass *client_class{ base_entity->GetClientClass( ) };
    if ( !client_class )
    {
        return;
    }

    const bool is_player{ client_class->GetClassID( ) == ETFClassID::CTFPlayer };
    if ( is_player && ( !this->connected( params.soundsource ) || !this->alive( params.soundsource ) ) )
    {
        return;
    }

    const vec3 origin{ is_player ? params.origin : base_entity->m_vecOrigin( ) };

    this->remember( base_entity, origin, true );
    this->push_origin( base_entity->entindex( ), base_entity->GetRefEHandle( ).GetSerialNumber( ),
                       origin );
}

void player_data::on_life_change( i32 index, bool dead )
{
    this->forget( index );
    if ( player_record_t *record{ this->player( index ) } )
    {
        record->m_dead = dead;
    }
}

void player_data::on_level_shutdown( )
{
    atl::memset( this->m_players, 0, sizeof( this->m_players ) );
    this->m_entities.reset( );
}

void player_data::push_origin( i32 index, i32 serial, const vec3 &origin ) const
{
    IClientEntity *client_entity{ g_tf2.m_entity_list->GetClientEntity( index ) };
    if ( !client_entity || !client_entity->IsDormant( ) )
    {
        return;
    }

    C_BaseEntity *base_entity{ client_entity->GetBaseEntity( ) };
    if ( !base_entity || base_entity->GetRefEHandle( ).GetSerialNumber( ) != serial )
    {
        return;
    }

    g_tf2.m_CBaseEntity_SetAbsOrigin.call_as< void, C_BaseEntity *, const vec3 * >( base_entity,
                                                                                   &origin );
}

i32 player_data::velocity_samples( i32 index ) const
{
    const player_record_t *record{ this->player( index ) };
    return record ? record->m_sample_count : 0;
}

const velocity_sample_t *player_data::velocity_sample( i32 index, i32 age ) const
{
    const player_record_t *record{ this->player( index ) };

    if ( !record || age < 0 || age >= record->m_sample_count )
    {
        return nullptr;
    }

    return &record->m_samples[ ( record->m_sample_head - age + VELOCITY_SAMPLES ) %
                               VELOCITY_SAMPLES ];
}

void player_data::forget_velocity( i32 index )
{
    if ( player_record_t * record{ this->player( index ) } )
    {
        record->m_sample_head  = 0;
        record->m_sample_count = 0;
        record->m_hop_tick     = -1; // a new life, a new rhythm
    }
}

void player_data::forget_vaccinator( i32 index )
{
    if ( player_record_t *record{ this->player( index ) } )
    {
        record->m_vacc_serial  = -1;
        record->m_vacc_mask    = 0;
        record->m_vacc_weapon  = INVALID_EHANDLE_INDEX;
        record->m_vacc_target  = INVALID_EHANDLE_INDEX;
        record->m_vacc_charge  = 0.0f;
        record->m_vacc_sampled = false;
        for ( double &stamp : record->m_vacc_stamp )
        {
            stamp = 0.0;
        }
    }
}

void player_data::sample_vaccinator( i32 index, C_TFPlayer *player )
{
    player_record_t *record{ this->player( index ) };
    if ( !record )
    {
        return;
    }

    const i32 serial{ player->GetRefEHandle( ).GetSerialNumber( ) };
    if ( record->m_vacc_serial != serial )
    {
        this->forget_vaccinator( index );
        record->m_vacc_serial = serial;
    }

    const u8    previous{ record->m_vacc_mask };
    const double stamp{ vacc_stamp( player->m_flSimulationTime( ) ) };
    u8          active{ };
    for ( i32 resist{ }; resist < 3; ++resist )
    {
        const u8 bit{ static_cast< u8 >( 1u << resist ) };
        if ( player->InCond( VACC_CONDS[ resist ] ) )
        {
            active |= bit;
            if ( !( previous & bit ) )
            {
                record->m_vacc_stamp[ resist ] = stamp;
            }
        } else
        {
            record->m_vacc_stamp[ resist ] = 0.0;
        }
    }
    record->m_vacc_mask = active;

    if ( player->m_iClass( ) != TF_CLASS_MEDIC )
    {
        record->m_vacc_sampled = false;
        return;
    }

    C_BaseEntity *weapon{ record->m_vacc_weapon == INVALID_EHANDLE_INDEX
                              ? nullptr
                              : g_entity_cache.resolve( record->m_vacc_weapon ) };
    if ( !weapon || weapon->As< C_TFWeaponBase * >( )->m_iItemDefinitionIndex( ) != Medic_s_TheVaccinator )
    {
        weapon = nullptr;
        EHANDLE *weapons{ player->m_hMyWeapons( ) };
        if ( weapons )
        {
            for ( i32 slot{ }; slot < MAX_WEAPONS; ++slot )
            {
                C_BaseEntity *candidate{ weapons[ slot ].Get( ) };
                if ( candidate && candidate->As< C_TFWeaponBase * >( )->m_iItemDefinitionIndex( ) == Medic_s_TheVaccinator )
                {
                    weapon = candidate;
                    break;
                }
            }
        }
    }

    if ( !weapon )
    {
        record->m_vacc_sampled = false;
        return;
    }

    C_WeaponMedigun *vaccinator{ weapon->As< C_WeaponMedigun * >( ) };
    const u32        weapon_handle{ static_cast< u32 >( weapon->GetRefEHandle( ).ToInt( ) ) };
    const bool       active_weapon{ player->m_hActiveWeapon( ).Get( ) == weapon };
    if ( C_BaseEntity *target{ vaccinator->m_hHealingTarget( ).Get( ) } )
    {
        if ( target->GetClientClass( )->GetClassID( ) == ETFClassID::CTFPlayer )
        {
            record->m_vacc_target = static_cast< u32 >( target->GetRefEHandle( ).ToInt( ) );
        } else
        {
            record->m_vacc_target = INVALID_EHANDLE_INDEX;
        }
    } else if ( active_weapon )
    {
        record->m_vacc_target = INVALID_EHANDLE_INDEX;
    }

    const float charge{ vaccinator->m_flChargeLevel( ) };
    const u8    added{ static_cast< u8 >( active & ~previous ) };
    if ( record->m_vacc_sampled && record->m_vacc_weapon == weapon_handle &&
         record->m_vacc_charge > charge &&
         added )
    {
        C_BaseEntity *target{ record->m_vacc_target == INVALID_EHANDLE_INDEX
                                  ? nullptr
                                  : g_entity_cache.resolve( record->m_vacc_target ) };
        if ( target && !target->IsDormant( ) &&
             target->GetClientClass( )->GetClassID( ) == ETFClassID::CTFPlayer )
        {
            C_TFPlayer *patient{ target->As< C_TFPlayer * >( ) };
            player_record_t *patient_record{ this->player( patient->entindex( ) ) };
            if ( patient_record && patient_record->m_vacc_serial == patient->GetRefEHandle( ).GetSerialNumber( ) )
            {
                for ( i32 resist{ }; resist < 3; ++resist )
                {
                    const u8 bit{ static_cast< u8 >( 1u << resist ) };
                    if ( ( added & bit ) && patient->InCond( VACC_CONDS[ resist ] ) )
                    {
                        patient_record->m_vacc_stamp[ resist ] = stamp;
                    }
                }
            }
        }
    }

    record->m_vacc_weapon  = weapon_handle;
    record->m_vacc_charge  = charge;
    record->m_vacc_sampled = true;
}

void player_data::remember_hop( i32 index, i32 stay, i32 tick )
{
    if ( player_record_t * record{ this->player( index ) } )
    {
        record->m_hop_stay = stay;
        record->m_hop_tick = tick;
    }
}

bool player_data::recall_hop( i32 index, i32 &stay, i32 &tick ) const
{
    const player_record_t *record{ this->player( index ) };

    if ( !record || record->m_hop_tick < 0 )
    {
        return false;
    }

    stay = record->m_hop_stay;
    tick = record->m_hop_tick;
    return true;
}

ATLAS_FORCE_INLINE i32 batch_lead( const player_record_t &record )
{
    const auto sample{ [ &record ]( i32 age ) -> const velocity_sample_t &
    {
        return record.m_samples[ ( record.m_sample_head - age + VELOCITY_SAMPLES ) % VELOCITY_SAMPLES ];
    } };

    if ( record.m_sample_count < 3 )
    {
        return sample( 0 ).m_lead;
    }

    i32 lowest{ sample( 0 ).m_lead };
    i32 highest{ sample( 0 ).m_lead };
    i32 total{ };
    for ( i32 age{ 1 }; age < record.m_sample_count; ++age )
    {
        const velocity_sample_t &older{ sample( age ) };
        const velocity_sample_t &newer{ sample( age - 1 ) };
        const i32                span{ newer.m_tick - newer.m_lead - ( older.m_tick - older.m_lead ) };
        lowest  = math::smaller( lowest, older.m_lead - span + 1 );
        highest = math::bigger( highest, older.m_lead );
        total += span;
    }

    for ( i32 value{ lowest }; value < highest; ++value )
    {
        i32 below{ };
        for ( i32 age{ 1 }; age < record.m_sample_count; ++age )
        {
            const velocity_sample_t &older{ sample( age ) };
            const velocity_sample_t &newer{ sample( age - 1 ) };
            const i32                span{ newer.m_tick - newer.m_lead - ( older.m_tick - older.m_lead ) };
            below += math::clamp( value - older.m_lead + span, 0, span );
        }

        if ( below > total / 2 )
        {
            return value;
        }
    }

    return highest;
}

void player_data::sample_velocity( i32 index, C_TFPlayer *player )
{
    player_record_t *record{ this->player( index ) };
    if ( !record )
    {
        return;
    }

    const i32                tick{ TIME_TO_TICKS( player->m_flSimulationTime( ) ) };
    const velocity_sample_t *newest{ this->velocity_sample( index, 0 ) };

    // only a tick the server actually simulated;
    if ( newest && tick <= newest->m_tick )
    {
        return;
    }

    const vec3 origin{ player->m_vecOrigin( ) };
    const f32 observed_maxspeed{ player->m_flMaxspeed( ) };
    const bool on_ground{ player->IsOnGround( ) };
    const bool charging{ player->InCond( TF_COND_SHIELD_CHARGE ) };
    const bool lost_footing{ player->InCond( TF_COND_LOST_FOOTING ) };
    const bool air_current{ player->InCond( TF_COND_AIR_CURRENT ) };
    const f32 maximum{ g_tf2.sv_maxvelocity->GetFloat( ) };
    const f32 air_cap{ charging ? g_tf2.tf_max_charge_speed->GetFloat( ) : 30.0f };
    const f32 ground_cap{ strafe_prediction::movement_max_speed(
        observed_maxspeed, lost_footing, charging, air_cap ) };
    f32 motion_limit{ math::bigger( math::sqrt( 3.0f * maximum * maximum + air_cap * air_cap ), ground_cap ) };
    if ( air_current || ( newest && newest->m_air_current ) )
    {
        const f32 redirect{ math::clamp( 1.0f - g_tf2.tf_movement_aircurrent_friction_mult->GetFloat( ), 0.0f, 1.0f ) };
        const f32 square{ 1.0f + redirect * redirect };
        motion_limit *= square * square; // at most four TryPlayerMove redirects
    }
    if ( newest )
    {
        const i32 gap{ tick - newest->m_tick };
        const f32 distance{ g_tf2.sv_lagcompensation_teleport_dist->GetFloat( ) };
        const f32 allowed{ !newest->m_on_ground || !on_ground
                               ? math::bigger( distance * gap, motion_limit * TICKS_TO_TIME( gap ) + 0.25f )
                               : distance };

        if ( gap > VELOCITY_SAMPLES ||
             ( origin - newest->m_origin ).length2d_sqr( ) > allowed * allowed )
        {
            this->forget_velocity( index ); // as if it never existed.
            newest = nullptr;
        }
    }

    const i32 slot{ record->m_sample_count > 0 ? ( record->m_sample_head + 1 ) % VELOCITY_SAMPLES
                                               : 0 };

    velocity_sample_t &entry{ record->m_samples[ slot ] };

    entry.m_origin    = origin;
    entry.m_mins      = player->m_vecMinsPreScaled( );
    entry.m_maxs      = player->m_vecMaxsPreScaled( );
    entry.m_hull      = entry.m_mins + entry.m_maxs;
    entry.m_tick      = tick;
    entry.m_gap       = newest ? tick - newest->m_tick : 0;
    entry.m_lead      = tick - g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick;
    entry.m_maxspeed  = observed_maxspeed;
    entry.m_on_ground = on_ground;
    entry.m_ducked    = player->IsDucked( );
    entry.m_water_level = static_cast< u8 >( player->m_nWaterLevel( ) );
    entry.m_swimming  = entry.m_water_level >= 2;
    entry.m_base_velocity         = vec3{ };
    entry.m_ground_index          = -1;
    entry.m_ground_serial         = -1;
    entry.m_ground_conveyor       = false;
    entry.m_base_velocity_applied = false;
    entry.m_transport_valid       = false;
    entry.m_surface_friction      = 1.0f;

    if ( entry.m_on_ground )
    {
        CTraceFilterSimple filter{ player, COLLISION_GROUP_PLAYER_MOVEMENT };
        Ray_t              ray{ };
        trace_t            support{ };
        ray.Init( entry.m_origin + vec3{ 0.0f, 0.0f, 0.125f },
                  entry.m_origin - vec3{ 0.0f, 0.0f, 2.0f },
                  entry.m_mins, entry.m_maxs );

        u32 mask{ MASK_PLAYERSOLID };
        if ( player->IsGhost( ) )
        {
            mask = MASK_PLAYERSOLID_BRUSHONLY;
        } else
        {
            switch ( player->m_iTeamNum( ) )
            {
                case TF_TEAM_RED: mask |= CONTENTS_TEAM2; break;
                case TF_TEAM_BLUE: mask |= CONTENTS_TEAM1; break;
                default: break;
            }
        }

        g_tf2.m_trace->TraceRay( ray, mask, &filter, &support );
        if ( !support.startsolid && !support.allsolid && support.fraction < 1.0f &&
             support.plane.normal.z >= 0.7f && support.m_pEnt && support.m_pEnt->entindex( ) == 0 )
        {
            entry.m_ground_index  = 0;
            entry.m_ground_serial = 0;

            if ( support.surface.surfaceProps >= 0 )
            {
                f32 material{ };
                g_tf2.m_physics_surface_props->GetPhysicsProperties( support.surface.surfaceProps, nullptr, nullptr,
                                                                     &material, nullptr );
                entry.m_surface_friction = math::smaller( material * 1.25f, 1.0f );
            }
        }
    }

    entry.m_air_current  = air_current;
    entry.m_lost_footing = lost_footing;
    entry.m_eye_yaw      = player->m_angEyeAngles( ).y;
    entry.m_eye_pitch    = player->m_angEyeAngles( ).x;
    entry.m_charging     = charging;
    if ( entry.m_charging && ( !newest || !newest->m_charging ) )
    {
        record->m_hop_tick = -1; // @::ChargeMove removes jump input..
    }

    vec3 corrected{ origin };
    if ( newest && !entry.m_on_ground && !newest->m_on_ground )
    {
        corrected += entry.m_hull - newest->m_hull;
    }

    entry.m_velocity = ( newest && entry.m_gap > 0 )
                           ? ( corrected - newest->m_origin ) / TICKS_TO_TIME( entry.m_gap )
                           : vec3{ };

    if ( newest && known_conveyor_transport_interval( entry, *newest ) )
    {
        entry.m_velocity = conveyor_relative_interval_velocity( entry, *newest );
        entry.m_transport_valid = true;
    }

    entry.m_wall = false;

    if ( newest && newest->m_velocity.length2d( ) > 60.0f )
    {
        Ray_t ray{ };
        ray.Init( newest->m_origin, newest->m_origin + newest->m_velocity * TICK_INTERVAL,
                  player->m_vecMinsPreScaled( ), player->m_vecMaxsPreScaled( ) );

        trace_t               trace{ };
        CTraceFilterWorldOnly filter{ };
        g_tf2.m_trace->TraceRay( ray, MASK_PLAYERSOLID_BRUSHONLY, &filter, &trace );

        entry.m_wall = trace.fraction < 1.0f && trace.plane.normal.z < 0.7f &&
                       newest->m_velocity.dot( trace.plane.normal ) < -PUSH_NOISE_FLOOR;

        if ( !entry.m_wall && entry.m_gap > 1 )
        {
            const vec3 chord{ entry.m_origin - newest->m_origin };
            if ( chord.length2d( ) > 0.125f )
            {
                ray.Init( newest->m_origin, entry.m_origin, player->m_vecMinsPreScaled( ),
                          player->m_vecMaxsPreScaled( ) );
                trace = trace_t{ };
                g_tf2.m_trace->TraceRay( ray, MASK_PLAYERSOLID_BRUSHONLY, &filter, &trace );
                entry.m_wall = trace.fraction < 1.0f && trace.plane.normal.z < 0.7f &&
                               chord.dot( trace.plane.normal ) < -0.125f;
            }
        }
    }

    const f32 quantization{ 1.73205081f * 0.125f / TICKS_TO_TIME( math::bigger( entry.m_gap, 1 ) ) };
    if ( entry.m_velocity.length( ) > motion_limit + quantization )
    {
        entry.m_velocity = vec3{ }; // velocity spike
    }

    record->m_sample_head  = slot;
    record->m_sample_count = math::smaller( record->m_sample_count + 1, VELOCITY_SAMPLES );
    entry.m_batch_lead     = batch_lead( *record );
}

void player_data::update( )
{
    if ( !g_tf2.m_engine->IsInGame( ) )
    {
        return;
    }

    const i32 highest{ math::smaller( g_tf2.m_globals->maxClients, MAX_TRACKED_PLAYERS - 1 ) };
    const i32 local{ g_cl.m_local ? g_cl.m_local->entindex( ) : -1 };

    for ( i32 index{ 1 }; index <= highest; ++index )
    {
        IClientEntity *client_entity{ g_tf2.m_entity_list->GetClientEntity( index ) };
        C_BaseEntity  *base_entity{ client_entity ? client_entity->GetBaseEntity( ) : nullptr };
        ClientClass   *client_class{ base_entity ? base_entity->GetClientClass( ) : nullptr };

        if ( !client_class || client_class->GetClassID( ) != ETFClassID::CTFPlayer )
        {
            this->clear_dormant( index );
            this->forget_velocity( index );
            this->forget_vaccinator( index );
            continue;
        }

        C_TFPlayer *player{ base_entity->As< C_TFPlayer * >( ) };

        const bool dormant{ base_entity->IsDormant( ) };
        if ( dormant ? ( !this->connected( index ) || !this->alive( index ) ) : !player->IsAlive( ) )
        {
            this->clear_dormant( index );
            this->forget_velocity( index );
            this->forget_vaccinator( index );
            continue;
        }

        if ( !dormant )
        {
            this->sample_vaccinator( index, player );
            if ( index != local )
            {
                this->sample_velocity( index, player );
            }
        } else
        {
            this->forget_vaccinator( index );
        }
    }
}

bool player_data::dormant_origin( C_BaseEntity *entity, vec3 &origin, float &remaining )
{
    const i32   index{ entity->entindex( ) };
    const i32   serial{ entity->GetRefEHandle( ).GetSerialNumber( ) };
    const float now{ g_tf2.m_globals->curtime };

    if ( const player_record_t *record{ this->player( index ) } )
    {
        if ( !this->connected( index ) || !this->alive( index ) ||
             record->m_serial != serial || record->m_dormant_expire <= now )
        {
            return false;
        }

        origin    = record->m_dormant_origin;
        remaining = record->m_dormant_expire - now;
        return true;
    }

    entity_record_t *entry{ this->m_entities.get( index ) };
    if ( !entry )
    {
        return false;
    }

    if ( entry->m_serial != serial || entry->m_dormant_expire <= now )
    {
        this->m_entities.erase( index );
        return false;
    }

    origin    = entry->m_dormant_origin;
    remaining = entry->m_dormant_expire - now;
    return true;
}

void player_data::on_simulation_time( i32 index, float previous, float current )
{
    if ( player_record_t * record{ this->player( index ) } )
    {
        record->m_old_simulation_time = previous;
        record->m_simulation_time     = current;
    }
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/baseentity.cpp#L140
void player_data::hold_stale_simulation_times( )
{
    const i32 highest{ math::smaller( g_tf2.m_globals->maxClients, MAX_TRACKED_PLAYERS - 1 ) };
    for ( i32 index{ 1 }; index <= highest; ++index )
    {
        IClientEntity *client_entity{ g_tf2.m_entity_list->GetClientEntity( index ) };
        C_BaseEntity  *entity{ client_entity ? client_entity->GetBaseEntity( ) : nullptr };
        if ( !entity || entity == g_cl.m_local || entity->IsDormant( ) )
        {
            continue;
        }

        float      &simulation_time{ entity->m_flSimulationTime( ) };
        const float previous{ entity->m_flOldSimulationTime( ) };
        if ( simulation_time <= previous || TIME_TO_TICKS( simulation_time ) % TIMESTAMP_BASE != 0 ||
             entity->m_vecOrigin( ) != entity->m_vecOldOrigin( ) )
        {
            continue;
        }

        simulation_time = previous;
        if ( player_record_t * record{ this->player( index ) } )
        {
            record->m_simulation_time = previous;
        }
    }
}

float player_data::simulation_time( i32 index ) const
{
    const player_record_t *record{ this->player( index ) };
    return record ? record->m_simulation_time : 0.0f;
}

float player_data::previous_simulation_time( i32 index ) const
{
    const player_record_t *record{ this->player( index ) };
    return record ? record->m_old_simulation_time : 0.0f;
}

float player_data::vacc_remaining( C_TFPlayer *player, i32 resist ) const
{
    Assert( resist >= 0 && resist < 3 );
    const player_record_t *record{ this->player( player->entindex( ) ) };
    if ( !record || record->m_vacc_serial != player->GetRefEHandle( ).GetSerialNumber( ) ||
         !( record->m_vacc_mask & ( 1u << resist ) ) )
    {
        return 0.0f;
    }

    const float duration{ g_tf2.weapon_vaccinator_resist_duration->GetFloat( ) };
    const float elapsed{ static_cast< float >( g_tf2.m_engine->Plat_FloatTime( ) -
                                             record->m_vacc_stamp[ resist ] ) };
    return math::bigger( duration - elapsed, 0.0f );
}

void player_data::on_player_buff( i32 index, i32 owner )
{
    player_record_t *record{ this->player( index ) };
    if ( !record )
    {
        return;
    }

    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    if ( index == owner )
    {
        if ( now - record->m_banner_last > BUFF_PULSE_GAP )
        {
            record->m_banner_start = now;
        }

        record->m_banner_last = now;
    }

    record->m_buff_owner = owner;
    record->m_buff_stamp = now;
}

float player_data::buff_remaining( i32 index ) const
{
    const player_record_t *record{ this->player( index ) };
    const player_record_t *owner{ record ? this->player( record->m_buff_owner ) : nullptr };
    if ( !owner )
    {
        return 0.0f;
    }

    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    if ( now - record->m_buff_stamp > BUFF_PULSE_GAP || now - owner->m_banner_last > BUFF_PULSE_GAP )
    {
        return 0.0f;
    }

    return math::bigger( BUFF_DURATION - static_cast< float >( now - owner->m_banner_start ), 0.0f );
}

bool player_data::resource_slot( i32 index ) const
{
    return g_cl.m_resource && index >= 1 && index <= g_tf2.m_engine->GetMaxClients( );
}

bool player_data::alive( i32 index ) const
{
    const player_record_t *record{ this->player( index ) };
    return record && !record->m_dead && this->resource_slot( index ) &&
           g_cl.m_resource->m_bAlive( )[ index ];
}

bool player_data::connected( i32 index ) const
{
    return this->resource_slot( index ) && g_cl.m_resource->m_bValid( )[ index ] &&
           g_cl.m_resource->m_bConnected( )[ index ];
}

i32 player_data::team( i32 index ) const
{
    return this->resource_slot( index ) ? g_cl.m_resource->m_iTeam( )[ index ] : 0;
}

i32 player_data::player_class( i32 index ) const
{
    return this->resource_slot( index ) ? g_cl.m_resource->m_iPlayerClass( )[ index ] : 0;
}
