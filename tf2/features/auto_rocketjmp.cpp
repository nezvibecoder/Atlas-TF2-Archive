#include "auto_rocketjmp.hpp"
#include "entity_cache.hpp"

#include "gui/gui.hpp"

#include "move_sim.hpp"
#include "movement_recorder.hpp"
#include "networking.hpp"
#include "proj_sim.hpp"
#include "ragebot.hpp"

ATLAS_NO_DESTROY auto_rocketjmp g_auto_rocketjmp{ };

constexpr float BLAST_FALLOFF{ 0.5f };
constexpr float DUCKED_FORCE_RATIO{ 82.0f / 55.0f };
constexpr float MAX_DAMAGE_FORCE{ 1000.0f };
constexpr float PUSH_SOURCE_DROP{ 10.0f };
constexpr i32   SIM_HORIZON{ 132 };
constexpr i32   AIM_PASSES{ 3 };

constexpr i32    DIAL_TICKS{ 21 };
constexpr i32    DIAL_RING_SEGMENTS{ 48 };
constexpr float  DIAL_LIT_LENGTH{ 16.0f };
constexpr float  DIAL_DIM_LENGTH{ 8.0f };
constexpr double DIAL_TURNS_PER_SECOND{ 0.04 };
constexpr float  DIAL_CHASE{ 16.0f };
constexpr float  DIAL_FADE_IN{ 0.12f };
constexpr float  DIAL_FADE_OUT{ 0.2f };

ATLAS_FORCE_INLINE bool blast_jump_weapon( C_TFWeaponBase *weapon )
{
    switch ( weapon->GetWeaponID( ) )
    {
        case TF_WEAPON_ROCKETLAUNCHER:
        case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
        case TF_WEAPON_PARTICLE_CANNON: break;
        default: return false;
    }

    // the beggars loads on IN_ATTACK instead of firing
    return g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "auto_fires_full_clip" ).get( ) ) == 0.0f;
}

ATLAS_FORCE_INLINE bool blast_reaches( const vec3 &blast, const vec3 &origin, const vec3 &mins, const vec3 &maxs )
{
    const vec3 nearest{ math::clamp( blast.x, origin.x + mins.x, origin.x + maxs.x ),
                        math::clamp( blast.y, origin.y + mins.y, origin.y + maxs.y ),
                        math::clamp( blast.z, origin.z + mins.z, origin.z + maxs.z ) };

    return blast.dist_to( nearest ) <= TF_ROCKET_RADIUS_FOR_RJS;
}

ATLAS_FORCE_INLINE vec3 blast_push( const vec3 &blast, const vec3 &origin, const vec3 &mins, const vec3 &maxs,
                                    float base_damage )
{
    const vec3  center{ origin + ( mins + maxs ) * 0.5f };
    const float distance{ math::smaller( math::smaller( blast.dist_to( center ), blast.dist_to( origin ) ), TF_ROCKET_RADIUS_FOR_RJS ) };
    const float damage{ base_damage * ( 1.0f - BLAST_FALLOFF * distance / TF_ROCKET_RADIUS_FOR_RJS ) };
    const float force{ math::smaller( 0.6f * damage * DUCKED_FORCE_RATIO * 10.0f, MAX_DAMAGE_FORCE ) };

    vec3 direction{ center - blast + vec3{ 0.0f, 0.0f, PUSH_SOURCE_DROP } };
    direction.normalize( );
    return direction * force;
}

ATLAS_FORCE_INLINE float jump_range( const vec3 &velocity, float height, float gravity )
{
    const float airtime{ ( velocity.z + math::sqrt( velocity.z * velocity.z + 2.0f * gravity * height ) ) / gravity };
    return velocity.length2d( ) * airtime;
}

ATLAS_FORCE_INLINE float farthest_offset( const vec3 &origin, const vec3 &velocity, const vec3 &heading, float floor,
                                          const vec3 &mins, const vec3 &maxs, float base_damage, float gravity )
{
    float best_offset{ };
    float best_range{ -1.0f };

    for ( i32 offset{ }; static_cast< float >( offset ) <= TF_ROCKET_RADIUS_FOR_RJS; ++offset )
    {
        const vec3 blast{ vec3{ origin.x, origin.y, floor } - heading * static_cast< float >( offset ) };
        if ( !blast_reaches( blast, origin, mins, maxs ) )
        {
            break;
        }

        const float range{ jump_range( velocity + blast_push( blast, origin, mins, maxs, base_damage ), origin.z - floor, gravity ) };
        if ( range > best_range )
        {
            best_range  = range;
            best_offset = static_cast< float >( offset );
        }
    }

    return best_offset;
}

ATLAS_FORCE_INLINE float fall_damage( C_TFPlayer *player, float fall_speed )
{
    if ( fall_speed <= TF_PLAYER_MAX_SAFE_FALL_SPEED || !g_cl.m_resource )
    {
        return 0.0f;
    }

    const float max_health{ static_cast< float >( g_cl.m_resource->m_iMaxHealth( )[ player->entindex( ) ] ) };
    return 5.0f * ( fall_speed / 300.0f ) * ( max_health / 100.0f ) * 1.2f;
}

void auto_rocketjmp::run( )
{
    if ( !this->should_run( ) )
    {
        this->m_aim_ticks     = 0;
        this->m_fire_lead     = -1;
        this->m_coast_until   = -1;
        this->m_key_held      = false;
        this->m_jump_pending  = false;
        this->m_blast_duck    = false;
        this->m_shot_duck     = false;
        this->m_shot_stand    = false;
        this->m_land_live     = false;
        this->m_fire_base     = -1;
        this->m_rocket_entity = -1;
        this->m_skip_logged   = false;
        return;
    }

    bind_t &key{ g_cfg.m_rjump.m_key };

    this->m_key_held     = gui::bind_active( key );
    this->m_fire_lead    = this->fire_lead( );
    this->m_jump_pending = false;

    const bool grounded{ g_cl.m_local->IsOnGround( ) };

    if ( grounded || g_cl.m_local->m_vecVelocity( ).z <= 0.0f )
    {
        this->m_blast_duck = false;
    }

    if ( grounded )
    {
        this->m_shot_duck  = false;
        this->m_shot_stand = false;
    }

    if ( this->m_blast_duck || this->m_shot_duck )
    {
        g_cl.m_cmd->buttons |= IN_DUCK;
    } else if ( this->m_shot_stand )
    {
        g_cl.m_cmd->buttons &= ~IN_DUCK;
    }

    if ( !grounded && g_cl.m_cmd->command_number <= this->m_coast_until )
    {
        g_cl.m_cmd->forwardmove = 0.0f;
        g_cl.m_cmd->sidemove    = 0.0f;
    }

    if ( !grounded )
    {
        this->m_air_fall_speed = -g_cl.m_local->m_vecVelocity( ).z;
    }

    if ( this->steer_pending( ) )
    {
        return;
    }

    if ( !grounded )
    {
        this->track_rocket( );
        return;
    }

    this->m_land_live   = false;
    this->m_skip_logged = false;
    this->land_outcome( );

    if ( this->m_key_held && this->m_fire_lead >= 0 )
    {
        this->ground_jump( );
    }
}

void auto_rocketjmp::run_predicted( )
{
    if ( !this->should_run( ) || this->attacked_this_command( ) )
    {
        return;
    }

    if ( this->m_jump_pending )
    {
        this->blast_jump( );
        return;
    }

    if ( g_cl.m_local->IsOnGround( ) )
    {
        return;
    }

    const bool rocket_jumper{ g_cl.m_weapon->m_iItemDefinitionIndex( ) == Soldier_m_RocketJumper };
    const bool pogo{ rocket_jumper && g_cfg.m_rjump.m_pogo && this->m_key_held };

    if ( pogo || ( rocket_jumper && g_cfg.m_rjump.m_land_shot ) )
    {
        this->land_shot( pogo );
    } else
    {
        this->m_land_live  = false;
        this->m_shot_duck  = false;
        this->m_shot_stand = false;
    }
}

void auto_rocketjmp::render( )
{
    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    const float  elapsed{ math::clamp( static_cast< float >( now - this->m_draw_time ), 0.0f, 0.1f ) };
    this->m_draw_time = now;

    const bool live{ this->m_land_live && g_cfg.m_rjump.m_enabled && g_tf2.m_engine->IsInGame( ) &&
                     !g_gui.open( ) };

    if ( live )
    {
        const float chase{ this->m_draw_fade > 0.0f ? math::clamp( elapsed * DIAL_CHASE, 0.0f, 1.0f ) : 1.0f };

        this->m_draw_point += ( this->m_land_point - this->m_draw_point ) * chase;
        this->m_draw_normal += ( this->m_land_normal - this->m_draw_normal ) * chase;
        this->m_draw_normal.normalize( );
        this->m_draw_ticks += ( static_cast< float >( this->m_land_tick ) - this->m_draw_ticks ) * chase;
    }

    const float fade_step{ elapsed / ( live ? DIAL_FADE_IN : DIAL_FADE_OUT ) };
    this->m_draw_fade = math::clamp( this->m_draw_fade + ( live ? fade_step : -fade_step ), 0.0f, 1.0f );

    if ( this->m_draw_fade <= 0.0f )
    {
        return;
    }

    const vec3 &normal{ this->m_draw_normal };
    const vec3  reference{ math::abs( normal.z ) < 0.9f ? vec3{ 0.0f, 0.0f, 1.0f } : vec3{ 1.0f, 0.0f, 0.0f } };

    vec3 u{ normal.cross( reference ) };
    u.normalize( );
    const vec3 v{ normal.cross( u ) };
    const vec3 centre{ this->m_draw_point + normal };

    const color_t &accent{ g_cfg.m_misc.m_accent };
    const auto     shade{ [ & ]( float strength )
    {
        return color_t{ accent.r, accent.g, accent.b,
                        static_cast< u8 >( 240.0f * strength * this->m_draw_fade ) };
    } };

    for ( i32 segment{ }; segment < DIAL_RING_SEGMENTS; ++segment )
    {
        const float from{ DEG2RAD( 360.0f / DIAL_RING_SEGMENTS * static_cast< float >( segment ) ) };
        const float to{ DEG2RAD( 360.0f / DIAL_RING_SEGMENTS * static_cast< float >( segment + 1 ) ) };

        ragebot::draw_path_segment( centre + ( u * math::cos( from ) + v * math::sin( from ) ) * TF_ROCKET_RADIUS_FOR_RJS,
                                    centre + ( u * math::cos( to ) + v * math::sin( to ) ) * TF_ROCKET_RADIUS_FOR_RJS,
                                    shade( 0.5f ) );
    }

    const float  progress{ this->m_land_total > 0
                               ? math::clamp( 1.0f - this->m_draw_ticks / static_cast< float >( this->m_land_total ),
                                              0.0f, 1.0f )
                               : 0.0f };
    const i32    lit{ static_cast< i32 >( progress * DIAL_TICKS ) };
    const double turns{ now * DIAL_TURNS_PER_SECOND };
    const float  spin{ static_cast< float >( ( turns - static_cast< f64 >( static_cast< i32 >( turns ) ) ) * 2.0 * PI ) };

    for ( i32 tick{ }; tick < DIAL_TICKS; ++tick )
    {
        const float angle{ spin + 2.0f * PI * static_cast< float >( tick ) / static_cast< float >( DIAL_TICKS ) };
        const vec3  outward{ u * math::cos( angle ) + v * math::sin( angle ) };
        const bool  on{ tick < lit };
        const float length{ on ? DIAL_LIT_LENGTH : DIAL_DIM_LENGTH };

        ragebot::draw_path_segment( centre + outward * TF_ROCKET_RADIUS_FOR_RJS,
                                    centre + outward * ( TF_ROCKET_RADIUS_FOR_RJS - length ), shade( on ? 1.0f : 0.35f ) );
    }
}

bool auto_rocketjmp::should_run( ) const
{
    if ( !g_cfg.m_rjump.m_enabled )
    {
        return false;
    }

    if ( !g_cl.m_cmd || !g_cl.m_local || !g_cl.m_weapon )
    {
        return false;
    }

    if ( !g_cl.m_local->IsAlive( ) || g_cl.m_local->IsTaunting( ) ||
         g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        return false;
    }

    if ( g_gui.open( ) )
    {
        return false;
    }

    if ( g_recorder.replaying( ) )
    {
        return false;
    }

    return blast_jump_weapon( g_cl.m_weapon );
}

i32 auto_rocketjmp::fire_lead( ) const
{
    C_TFWeaponBase *weapon{ g_cl.m_weapon };

    if ( weapon->m_bLowered( ) || weapon->m_iClip1( ) == 0 )
    {
        return -1;
    }

    const float curtime{ TICKS_TO_TIME( g_cl.m_local->m_nTickBase( ) ) };
    if ( weapon->m_iReloadMode( ) != TF_RELOAD_START )
    {
        if ( weapon->m_flReloadPriorNextFire( ) > curtime )
        {
            return -1;
        }

        return g_cl.m_local->m_flNextAttack( ) > curtime ? 1 : 0;
    }

    if ( weapon->m_flNextPrimaryAttack( ) > curtime || g_cl.m_local->m_flNextAttack( ) > curtime )
    {
        return -1;
    }

    return 0;
}

bool auto_rocketjmp::steer_pending( )
{
    if ( this->m_aim_ticks <= 0 )
    {
        return false;
    }

    --this->m_aim_ticks;

    const float curtime{ TICKS_TO_TIME( g_cl.m_local->m_nTickBase( ) ) };

    if ( g_cl.m_weapon->m_flNextPrimaryAttack( ) > curtime )
    {
        this->m_aim_ticks = 0; // the shot left; the fire delay is running
        return false;
    }

    this->press( this->m_aim );
    return true;
}

void auto_rocketjmp::ground_jump( )
{
    if ( g_cl.m_local->m_nButtons( ) & IN_JUMP )
    {
        g_cl.m_cmd->buttons &= ~IN_JUMP;
        return;
    }

    if ( g_cl.m_local->IsDucked( ) )
    {
        g_cl.m_cmd->buttons &= ~IN_DUCK;
        return;
    }

    if ( this->m_fire_lead > 0 )
    {
        g_cl.m_cmd->buttons |= IN_ATTACK;
        return;
    }

    g_cl.m_cmd->buttons |= IN_JUMP | IN_DUCK;
    this->m_jump_pending = true;
}

void auto_rocketjmp::blast_jump( )
{
    if ( g_cl.m_local->IsOnGround( ) )
    {
        return;
    }

    vec3 aim{ };
    i32  blast_commands{ };
    if ( !this->aim_blast( aim, blast_commands ) )
    {
        return;
    }

    this->fire( aim );
    this->m_blast_duck  = true;
    this->m_coast_until = g_cl.m_cmd->command_number + blast_commands;
}

bool auto_rocketjmp::aim_blast( vec3 &out, i32 &blast_commands ) const
{
    C_TFPlayer *local{ g_cl.m_local };

    const vec3  eye{ local->Weapon_ShootPosition( ) };
    const vec3  origin{ local->GetAbsOrigin( ) };
    const vec3  velocity{ local->m_vecVelocity( ) };
    const float gravity{ g_tf2.sv_gravity->GetFloat( ) };
    const float scale{ local->m_flModelScale( ) };
    const vec3  mins{ VEC_DUCK_HULL_MIN * scale };
    const vec3  maxs{ VEC_DUCK_HULL_MAX * scale };

    const float lift{ local->IsDucked( ) ? 0.0f : local->m_vecMaxs( ).z - maxs.z };

    vec3 heading{ };
    if ( g_cfg.m_rjump.m_priority == config::RJUMP_IMPULSE )
    {
        vec3 forward{ };
        vec3 right{ };
        math::angle_vectors( vec3{ 0.0f, g_cl.m_cmd->viewangles.y, 0.0f }, &forward, &right );

        heading = forward * g_cl.m_cmd->forwardmove + right * g_cl.m_cmd->sidemove;
        if ( heading.is_zero( ) )
        {
            heading = vec3{ velocity.x, velocity.y, 0.0f };
        }

        heading.z = 0.0f;
        heading.normalize( );
    }

    const float base_damage{ static_cast< float >( g_cl.m_weapon->GetWeaponInfo( )->GetWeaponData( 0 ).m_nDamage ) };

    const i32 hold{ g_networking.send_delay( 1 ).ticks( ) };

    vec3  aim{ 89.0f, g_cl.m_cmd->viewangles.y, 0.0f };
    float best{ FLT_MAX };

    for ( i32 pass{ }; pass < AIM_PASSES; ++pass )
    {
        projectile_setup_t setup{ };
        if ( !g_proj_sim.gather( local, g_cl.m_weapon, aim, setup ) )
        {
            break;
        }

        vec3      path[ SIM_HORIZON ]{ };
        impact_t  impact{ };
        const i32 flight{ g_proj_sim.simulate( setup, path, SIM_HORIZON, 1, impact ) };

        vec3 forward{ };
        math::angle_vectors( aim, &forward );

        if ( !impact.m_hit || forward.z >= 0.0f )
        {
            break;
        }

        const float time{ TICKS_TO_TIME( hold + flight ) };
        const vec3  blast_origin{ origin + velocity * time +
                                  vec3{ 0.0f, 0.0f, lift - 0.5f * gravity * time * time } };
        const vec3  blast_velocity{ velocity.x, velocity.y, velocity.z - gravity * time };

        const float back{ heading.is_zero( ) ? 0.0f
                                             : farthest_offset( blast_origin, blast_velocity, heading, impact.m_position.z,
                                                                mins, maxs, base_damage, gravity ) };

        vec3 miss{ impact.m_position - ( blast_origin - heading * back ) };
        miss.z = 0.0f;

        const float distance{ miss.length2d( ) };
        if ( distance < best && blast_reaches( impact.m_position, blast_origin, mins, maxs ) )
        {
            best           = distance;
            out            = aim;
            blast_commands = hold + flight;
        }

        const vec3 aimed{ eye + forward * ( ( impact.m_position.z - eye.z ) / forward.z ) };
        math::vector_angles( aimed - miss - eye, aim );
        math::clamp_angles( aim );
    }

    return best != FLT_MAX;
}

void auto_rocketjmp::land_shot( bool pogo )
{
    vec3            path[ SIM_HORIZON ]{ };
    rjump_landing_t landing{ };

    const bool was_live{ this->m_land_live };
    this->m_land_live = this->m_fire_base >= 0;

    if ( !this->predict_landing( path, SIM_HORIZON, landing ) )
    {
        this->m_land_live  = false;
        this->m_shot_duck  = false;
        this->m_shot_stand = false;
        return;
    }

    this->m_land_point  = landing.m_point;
    this->m_land_normal = landing.m_normal;
    this->m_land_tick   = landing.m_tick;

    const float health{ static_cast< float >( g_cl.m_local->m_iHealth( ) ) };

    const bool bounce{ pogo && health > fall_damage( g_cl.m_local, landing.m_speed ) };
    const bool rescue{ g_cfg.m_rjump.m_land_shot && landing.m_speed > TF_PLAYER_MAX_SAFE_FALL_SPEED };

    const bool crouch{ !bounce || g_cfg.m_rjump.m_pogo_crouch };

    this->m_shot_duck  = ( bounce || rescue ) && crouch;
    this->m_shot_stand = bounce && !crouch;
    if ( !this->m_shot_duck && !this->m_shot_stand )
    {
        return;
    }

    if ( ( ( g_cl.m_cmd->buttons & IN_DUCK ) != 0 ) != crouch )
    {
        return;
    }

    if ( this->m_fire_lead < 0 )
    {
        if ( !this->m_skip_logged && landing.m_tick <= 12 )
        {
            this->m_skip_logged = true;
            DBGOUT( "land skip: weapon not ready (touchdown %d reload %d clip %d)", landing.m_tick,
                    g_cl.m_weapon->m_iReloadMode( ), g_cl.m_weapon->m_iClip1( ) );
        }

        return;
    }

    const i32  lead{ this->m_fire_lead };
    const vec3 muzzle_shift{ lead > 0 ? path[ lead - 1 ] - g_cl.m_local->GetAbsOrigin( ) : vec3{ } };

    const i32   blast_index{ landing.m_tick - ( bounce ? 1 : 2 ) };
    const vec3 &over{ blast_index >= 0 ? path[ blast_index ] : landing.m_point };
    const vec3  target{ over.x, over.y, landing.m_point.z };

    vec3 aim{ };
    math::vector_angles( target - ( g_cl.m_local->Weapon_ShootPosition( ) + muzzle_shift ), aim );
    math::clamp_angles( aim );

    projectile_setup_t setup{ };
    if ( !g_proj_sim.gather( g_cl.m_local, g_cl.m_weapon, aim, setup ) )
    {
        return;
    }

    setup.m_position += muzzle_shift;

    vec3      rocket_path[ SIM_HORIZON ]{ };
    impact_t  rocket_impact{ };
    const i32 rocket_ticks{ g_proj_sim.simulate( setup, rocket_path, landing.m_tick, 1, rocket_impact ) };

    if ( !rocket_impact.m_hit )
    {
        return;
    }

    const i32 hold{ g_networking.send_delay( 1 ).ticks( ) };
    const i32 timed{ bounce ? hold : hold + 1 };
    const i32 gap{ landing.m_tick - lead - rocket_ticks };

    if ( gap < timed )
    {
        if ( !this->m_skip_logged )
        {
            this->m_skip_logged = true;
            DBGOUT( "land skip: window passed (gap %d hold %d lead %d touchdown %d)", gap, hold, lead,
                    landing.m_tick );
        }

        return;
    }

    const vec3 &blast_origin{ path[ lead + hold + rocket_ticks - 1 ] };
    if ( !blast_reaches( rocket_impact.m_position, blast_origin, g_cl.m_local->m_vecMins( ),
                         g_cl.m_local->m_vecMaxs( ) ) )
    {
        if ( !this->m_skip_logged )
        {
            this->m_skip_logged = true;
            DBGOUT( "land skip: blast out of reach (sep %.0f gap %d touchdown %d)",
                    rocket_impact.m_position.dist_to( blast_origin ), gap, landing.m_tick );
        }

        return;
    }

    this->m_land_total = was_live ? math::bigger( this->m_land_total, landing.m_tick ) : landing.m_tick;
    this->m_land_live  = true;

    if ( gap > timed )
    {
        return;
    }

    DBGOUT( "land shot: impact %d touchdown %d gap %d hold %d lead %d landing %.0f falling %.0f "
            "hspd %.0f sep %.0f",
            rocket_ticks, landing.m_tick, gap, hold, lead, landing.m_speed,
            -g_cl.m_local->m_vecVelocity( ).z, g_cl.m_local->m_vecVelocity( ).length2d( ),
            rocket_impact.m_position.dist_to( blast_origin ) );

    this->m_fire_base       = g_cl.m_cmd->command_number;
    this->m_pred_touchdown  = landing.m_tick;
    this->m_pred_impact     = rocket_ticks;
    this->m_pred_land_speed = landing.m_speed;
    this->m_rocket_entity   = -1;

    this->fire( aim );
    this->m_coast_until = g_cl.m_cmd->command_number + lead + hold + rocket_ticks;
}

bool auto_rocketjmp::predict_landing( vec3 *path, i32 capacity, rjump_landing_t &out ) const
{
    vec3 origin{ g_cl.m_local->GetAbsOrigin( ) };
    vec3 velocity{ g_cl.m_local->m_vecVelocity( ) };

    const float dt{ TICK_INTERVAL };
    const float gravity{ g_tf2.sv_gravity->GetFloat( ) };

    for ( i32 tick{ }; tick < capacity; ++tick )
    {
        const float fall{ -velocity.z };

        velocity.z -= gravity * 0.5f * dt;

        trace_t trace{ };
        g_move_sim.perform_player_move_trace( g_cl.m_local, origin, origin + velocity * dt, trace );

        origin       = trace.endpos;
        path[ tick ] = origin;

        if ( trace.fraction < 1.0f )
        {
            velocity = math::clip_velocity( velocity, trace.plane.normal );
        }

        if ( velocity.z <= 250.0f )
        {
            if ( trace.fraction >= 1.0f || trace.plane.normal.z < 0.7f )
            {
                g_move_sim.perform_player_move_trace( g_cl.m_local, origin, origin - vec3{ 0.0f, 0.0f, 2.0f },
                                                      trace );
            }

            if ( trace.fraction < 1.0f && trace.plane.normal.z >= 0.7f )
            {
                out.m_point  = trace.endpos;
                out.m_normal = trace.plane.normal;
                out.m_speed  = fall;
                out.m_tick   = tick + 1;
                return true;
            }
        }

        velocity.z -= gravity * 0.5f * dt;
    }

    return false;
}

void auto_rocketjmp::fire( const vec3 &angles )
{
    this->press( angles );

    this->m_aim       = angles;
    this->m_aim_ticks = 2;
}

void auto_rocketjmp::press( const vec3 &angles )
{
    g_cl.m_cmd->viewangles = angles;
    g_cl.m_cmd->buttons |= IN_ATTACK;
    g_cl.m_cmd->buttons &= ~IN_ATTACK2;

    g_cl.m_psilent_choked  = true;
    this->m_attack_command = g_cl.m_cmd->command_number;
}

void auto_rocketjmp::land_outcome( )
{
    if ( this->m_fire_base < 0 )
    {
        return;
    }

    DBGOUT( "land result: touchdown +%d cmds (predicted +%d), fall at landing %.0f (predicted %.0f)",
            g_cl.m_cmd->command_number - 1 - this->m_fire_base, this->m_pred_touchdown,
            this->m_air_fall_speed, this->m_pred_land_speed );

    this->m_fire_base     = -1;
    this->m_rocket_entity = -1;
}

void auto_rocketjmp::track_rocket( )
{
    if ( this->m_fire_base < 0 || this->m_rocket_entity == -2 )
    {
        return;
    }

    const i32 flew{ g_cl.m_cmd->command_number - this->m_fire_base };

    if ( this->m_rocket_entity < 0 )
    {
        for ( const auto *cached : g_entity_cache.projectiles( ) )
        {
            if ( !cached->networked( ) || cached->m_index <= g_tf2.m_engine->GetMaxClients( ) ||
                 cached->m_class_id != ETFClassID::CTFProjectile_Rocket )
            {
                continue;
            }
            C_BaseEntity *entity{ cached->m_entity };

            if ( entity->m_hOwnerEntity( ).Get( ) != g_cl.m_local )
            {
                continue;
            }

            this->m_rocket_entity = cached->m_index;
            this->m_rocket_handle = cached->m_handle;
            DBGOUT( "land rocket: up at +%d cmds", flew );
            return;
        }

        if ( flew > 12 )
        {
            DBGOUT( "land rocket: never appeared (+%d cmds)", flew );
            this->m_rocket_entity = -2;
        }

        return;
    }

    if ( !g_entity_cache.resolve( this->m_rocket_handle ) )
    {
        DBGOUT( "land rocket: down at +%d cmds (predicted impact %d)", flew,
                this->m_pred_impact );
        this->m_rocket_entity = -2;
    }
}
