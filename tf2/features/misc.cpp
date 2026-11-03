#include "misc.hpp"

#include "gui/gui.hpp"

#include "movement_recorder.hpp"

ATLAS_NO_DESTROY misc g_misc{ };

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_weapon_shotgun.cpp#L331
ATLAS_FORCE_INLINE f32 fan_jump_pitch( f32 speed, f32 rise )
{
    f32 best_pitch{ };
    f32 best_climb{ };

    for ( i32 degrees{ 1 }; degrees < 90; ++degrees )
    {
        const f32 pitch{ static_cast< f32 >( degrees ) };
        const f32 sine{ math::sin( DEG2RAD( pitch ) ) };
        const f32 cosine{ math::cos( DEG2RAD( pitch ) ) };
        const f32 climb{ FAN_KNOCKBACK_SPEED * sine + ( speed * sine + rise * cosine ) * cosine };

        if ( climb > best_climb )
        {
            best_climb = climb;
            best_pitch = pitch;
        }
    }

    return best_pitch;
}

void misc::on_pre_prediction( )
{
    freecam_input( );
    fast_stop( );
    bhop( );
    auto_strafe( );
    taunt_spin( );
    fan_jump( );

    this->m_edge_grounded = g_cl.m_local->IsOnGround( );
}

void misc::on_prediction( )
{
    edge_jump( );
}

void misc::on_post_prediction( )
{
    perform_taunt_cancel_exploit( );
    anti_afk( );
    noisemaker_spam( );
}

void misc::bhop( )
{
    if ( !g_cfg.m_misc.m_bhop || g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        return;
    }

    const bool held{ ( g_cl.m_cmd->buttons & IN_JUMP ) != 0 };
    const bool pressed{ held && !this->m_bhop_jump_held };

    this->m_bhop_jump_held = held;

    if ( !held )
    {
        return;
    }

    if ( pressed && ( g_cl.m_local->IsOnGround( ) ||
                      g_cl.m_local->m_iClass( ) == TF_CLASS_SCOUT ) )
    {
        return;
    }

    if ( !g_cl.m_local->IsOnGround( ) || g_cl.m_local->IsDucked( ) ||
         ( g_cl.m_local->m_afButtonLast( ) & IN_JUMP ) )
    {
        g_cl.m_cmd->buttons &= ~IN_JUMP;
    }
}

void misc::edge_jump( )
{
    if ( !g_cfg.m_misc.m_edge_jump )
    {
        return;
    }

    const bind_t &key{ g_cfg.m_misc.m_edge_jump_key };
    const bool    gated{ key.m_key != 0 || key.m_mode == bind_mode_t::always };

    if ( gated && !gui::bind_active( g_cfg.m_misc.m_edge_jump_key ) )
    {
        return;
    }

    if ( !this->m_edge_grounded || g_cl.m_local->IsOnGround( ) ||
         g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK || g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        return;
    }

    g_cl.m_cmd->buttons |= IN_JUMP;
}

void misc::fan_jump( )
{
    if ( !g_cfg.m_misc.m_fan_jump || g_gui.open( ) || g_recorder.replaying( ) ||
         !g_cl.has_live_weapon_command( ) )
    {
        return;
    }

    if ( !gui::bind_active( g_cfg.m_misc.m_fan_jump_key ) )
    {
        return;
    }

    C_TFPlayer     *local{ g_cl.m_local };
    C_TFWeaponBase *weapon{ g_cl.m_weapon };

    if ( local->m_MoveType( ) != MOVETYPE_WALK || local->m_nWaterLevel( ) > 1 ||
         local->m_bScattergunJump( ) )
    {
        return;
    }

    if ( g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "set_scattergun_has_knockback" ).get( ) ) != 1.0f )
    {
        return;
    }

    const f32 time{ TICKS_TO_TIME( local->m_nTickBase( ) ) };

    if ( weapon->m_iClip1( ) <= 0 || weapon->m_flNextPrimaryAttack( ) > time ||
         !g_cl.weapon_attack_allowed( time ) )
    {
        return;
    }

    const vec3 velocity{ local->m_vecVelocity( ) };
    f32        rise{ velocity.z };

    if ( local->IsOnGround( ) )
    {
        if ( local->m_nButtons( ) & IN_JUMP )
        {
            g_cl.m_cmd->buttons &= ~IN_JUMP;
            return;
        }

        if ( local->IsDucked( ) )
        {
            return;
        }

        const f32 gravity{ g_tf2.sv_gravity->GetFloat( ) * ( local->m_flGravity( ) != 0.0f ? local->m_flGravity( ) : 1.0f ) };
        const f32 jump_speed{ JUMP_SPEED * g_cl.attrib_hook( weapon,
                                                             g_cl.attrib_hook( local, 1.0f, ATLAS_XORSTR( "mod_jump_height" ).get( ) ),
                                                             ATLAS_XORSTR( "mod_jump_height_from_weapon" ).get( ) ) };

        g_cl.m_cmd->buttons |= IN_JUMP;
        rise = jump_speed - gravity * TICK_INTERVAL;
    }

    const f32 speed{ velocity.length2d( ) };
    const f32 yaw{ speed > 0.0f ? RAD2DEG( math::atan2( velocity.y, velocity.x ) ) : g_cl.m_cmd->viewangles.y };

    g_cl.m_cmd->viewangles = vec3{ fan_jump_pitch( speed, rise ), yaw, 0.0f };
    g_cl.m_cmd->buttons |= IN_ATTACK;
    g_cl.m_psilent_choked = true;
}

void misc::fast_stop( )
{
    if ( !g_cfg.m_misc.m_fast_stop )
    {
        return;
    }

    constexpr i32 MOVE_BUTTONS{ IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT };
    if ( g_cl.m_cmd->buttons & ( MOVE_BUTTONS | IN_JUMP ) )
    {
        return;
    }

    if ( g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK || !g_cl.m_local->IsOnGround( ) ||
         g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        return;
    }

    vec3 velocity{ g_cl.m_local->m_vecVelocity( ) };
    velocity.z = 0.0f;

    const float speed{ velocity.length2d( ) };
    const float surface{ g_cl.m_local->m_surfaceFriction( ) };
    const float friction{ g_tf2.sv_friction->GetFloat( ) * surface };
    const float accelerate{ g_tf2.sv_accelerate->GetFloat( ) };
    const float remaining{ speed - math::bigger( speed, g_tf2.sv_stopspeed->GetFloat( ) ) * friction * TICK_INTERVAL };
    const float brake{ remaining / ( accelerate * TICK_INTERVAL * surface ) };

    if ( brake < 100.0f * friction / accelerate )
    {
        g_cl.m_cmd->forwardmove = 0.0f;
        g_cl.m_cmd->sidemove    = 0.0f;
        return;
    }

    const float crop{ g_cl.m_local->IsDucked( ) ? 1.0f / 3.0f : 1.0f };
    const float push{ math::smaller( brake / crop, g_cl.m_local->m_flMaxspeed( ) ) };
    const float velocity_yaw{ RAD2DEG( math::atan2( velocity.y, velocity.x ) ) };
    const float push_delta{ DEG2RAD( math::normalize_yaw( velocity_yaw + 180.0f -
                                                          g_cl.m_cmd->viewangles.y ) ) };

    g_cl.m_cmd->forwardmove = push * math::cos( push_delta );
    g_cl.m_cmd->sidemove    = -push * math::sin( push_delta );
}

void misc::auto_strafe( )
{
    if ( !g_cfg.m_misc.m_auto_strafe || g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK ||
         g_cl.m_local->IsOnGround( ) || g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        return;
    }

    if ( ( g_cl.m_cmd->buttons & IN_JUMP ) && !( g_cl.m_local->m_nButtons( ) & IN_JUMP ) )
    {
        return;
    }

    const bool  follow_wasd{ g_cfg.m_misc.m_auto_strafe_mode == 1 };
    const float forward_move{ g_cl.m_cmd->forwardmove };
    const float side_move{ g_cl.m_cmd->sidemove };

    if ( follow_wasd && forward_move == 0.0f && side_move == 0.0f )
    {
        return;
    }

    vec3 velocity{ g_cl.m_local->m_vecVelocity( ) };
    velocity.z = 0.0f;

    const float speed{ velocity.length2d( ) };
    const float velocity_yaw{ RAD2DEG( math::atan2( velocity.y, velocity.x ) ) };
    const float view_yaw{ g_cl.m_cmd->viewangles.y };

    // ( sv_airaccelerate * wishspeed * frametime )
    const float wish_speed{ math::smaller( 450.f, g_cl.m_local->m_flMaxspeed( ) ) };
    const float max_air_accel{ math::smaller( 30.f, g_tf2.sv_airaccelerate->GetFloat( ) * wish_speed * TICK_INTERVAL ) };

    if ( max_air_accel <= 0.f )
    {
        return;
    }

    const float gain_offset{ RAD2DEG( math::asin( max_air_accel * 0.5f / speed ) ) };
    const float turn_max{ RAD2DEG( math::asin( max_air_accel / speed ) ) };

    float wish_yaw{ view_yaw };
    float turn_limit{ turn_max };

    if ( follow_wasd )
    {
        wish_yaw   = math::normalize_yaw( view_yaw + RAD2DEG( math::atan2( -side_move, forward_move ) ) );
        turn_limit = turn_max * math::clamp( g_cfg.m_misc.m_auto_strafe_retrack / 100.0f, 0.0f, 1.0f );
    }

    this->m_strafe_side = this->m_strafe_side > 0.0f ? -1.0f : 1.0f;

    const float wish_delta{ math::normalize_yaw( wish_yaw - velocity_yaw ) };

    float push_yaw{ };

    if ( math::abs( wish_delta ) > turn_limit )
    {
        push_yaw = velocity_yaw + ( wish_delta > 0.0f ? 1.0f : -1.0f ) * ( 90.0f + turn_limit );
    } else
    {
        push_yaw = wish_yaw + this->m_strafe_side * ( 90.0f - gain_offset );
    }

    const float push_delta{ DEG2RAD( math::normalize_yaw( push_yaw - view_yaw ) ) };

    g_cl.m_cmd->forwardmove = 450.f * math::cos( push_delta );
    g_cl.m_cmd->sidemove    = -450.f * math::sin( push_delta );
}

void misc::taunt_spin( )
{
    if ( !g_cfg.m_misc.m_taunt_spin || !g_cl.m_local->IsTaunting( ) )
    {
        return;
    }

    this->m_taunt_spin_yaw = math::normalize_yaw( this->m_taunt_spin_yaw +
                                                  static_cast< float >( g_cfg.m_misc.m_taunt_spin_speed ) );

    g_cl.m_cmd->viewangles.y = this->m_taunt_spin_yaw;
}

void misc::on_override_view( CViewSetup *setup, const vec3 &eye )
{
    if ( !setup || !g_cl.m_local )
    {
        return;
    }

    if ( setup && g_cl.m_local && g_cfg.m_misc.m_removals[ config::REMOVAL_VIEWPUNCH ] )
    {
        setup->angles -= g_cl.m_local->m_vecPunchAngle( );
    }

    this->fov( setup );
    this->freecam( setup, eye );
    this->thirdperson( setup, eye );
}

bool misc::overlay_removed( IMaterial *material ) const
{
    if ( !g_cfg.m_misc.m_removals[ config::REMOVAL_OVERLAYS ] || !material )
    {
        return false;
    }

    const char *name{ material->GetName( ) };

    if ( !name )
    {
        return false;
    }

    const char *const invuln{ ATLAS_XORSTR_STATIC( "effects/invuln_overlay" ) };
    if ( str::starts_with( name, str::length( name ), invuln, str::length( invuln ) ) )
    {
        return true;
    }

    const char *const names[]{ ATLAS_XORSTR_STATIC( "effects/jarate_overlay" ),
                               ATLAS_XORSTR_STATIC( "effects/bleed_overlay" ),
                               ATLAS_XORSTR_STATIC( "effects/stealth_overlay" ),
                               ATLAS_XORSTR_STATIC( "effects/dodge_overlay" ),
                               ATLAS_XORSTR_STATIC( "effects/gas_overlay" ),
                               ATLAS_XORSTR_STATIC( "effects/imcookin" ) };

    for ( const char *candidate : names )
    {
        if ( str::equals( name, candidate ) )
        {
            return true;
        }
    }

    return false;
}

bool misc::is_footstep( const char *name ) const
{
    if ( !name )
    {
        return false;
    }

    const usize       length{ str::length( name ) };
    const char *const waves{ ATLAS_XORSTR_STATIC( "footsteps" ) };
    if ( str::contains( name, length, waves, str::length( waves ) ) )
    {
        return true;
    }

    const usize dot{ str::rfind( name, length, '.' ) };

    if ( dot == str::npos )
    {
        return false;
    }

    const char *const tail{ name + dot + 1 };

    return str::equals_ignore_case( tail, ATLAS_XORSTR( "StepLeft" ).get( ) ) ||
           str::equals_ignore_case( tail, ATLAS_XORSTR( "StepRight" ).get( ) ) ||
           str::equals_ignore_case( tail, ATLAS_XORSTR( "FootstepLeft" ).get( ) ) ||
           str::equals_ignore_case( tail, ATLAS_XORSTR( "FootstepRight" ).get( ) );
}

bool misc::mutes_footstep( const char *name ) const
{
    return g_cfg.m_misc.m_removals[ config::REMOVAL_FOOTSTEPS ] && this->is_footstep( name );
}

bool misc::is_weapon_sound( const StartSoundParams_t &params ) const
{
    if ( params.entchannel == CHAN_WEAPON )
    {
        return true;
    }

    const char *name{ params.pSfx ? params.pSfx->getname( ) : nullptr };
    if ( !name )
    {
        return false;
    }

    while ( *name && !str::is_alpha( *name ) )
    {
        ++name;
    }

    const char *const folder{ ATLAS_XORSTR_STATIC( "weapons" ) };
    const usize       length{ str::length( folder ) };

    return str::equals_ignore_case( name, length, folder, length );
}

const char *misc::giant_shoot_sound( const char *sound ) const
{
    switch ( HASH_RT( sound ) )
    {
        case HASH_CT( "Weapon_GrenadeLauncher.Single" ):
        case HASH_CT( "Weapon_GrenadeLauncher.SingleCrit" ): return ATLAS_XORSTR_STATIC( "MVM.GiantDemoman_Grenadeshoot" );
        case HASH_CT( "Weapon_FlameThrower.Fire" ): return ATLAS_XORSTR_STATIC( "MVM.GiantPyro_FlameStart" );
        case HASH_CT( "Weapon_FlameThrower.FireLoop" ): return ATLAS_XORSTR_STATIC( "MVM.GiantPyro_FlameLoop" );
        default: break;
    }

    return sound;
}

void misc::noisemaker_spam( )
{
    if ( !g_cfg.m_misc.m_noisemaker_spam ||
         ( g_cfg.m_misc.m_noisemaker_spam_mode == config::NOISEMAKER_SPAM_ON_KILL && !m_should_hit_noisemaker ) )
    {
        return;
    }

    // no noisemaker; return
    if ( g_cl.attrib_hook( g_cl.m_local, 0.0f, ATLAS_XORSTR( "enable_misc2_noisemaker" ).get( ) ) == 0.0f )
    {
        m_should_hit_noisemaker = false;
        return;
    }

    const auto shared_data{ g_cl.m_local->GetSharedPlayerData( ) };
    if ( !shared_data )
    {
        return;
    }

    if ( shared_data->m_flNextNoiseMakerTime( ) > g_tf2.m_globals->curtime )
    {
        return;
    }

    // ServerCmdKeyValues calls SendNetMsg under the hood, CLCKeyValues inherits base CNetMessage which
    // submits every message into the reliable stream. If we send KeyValues while choking, we'll just break prediction.
    if ( !g_cl.m_send_packet )
    {
        return;
    }

    DBGOUT( "misc::noisemaker_spam: Sending to server..." );
    auto kv{ new KeyValues( ATLAS_XORSTR_STATIC( "use_action_slot_item_server" ) ) };
    g_tf2.m_engine->ServerCmdKeyValues( kv );
    m_should_hit_noisemaker = false; // Completed the noisemaker request for this event.
}

void misc::noisemaker_notify_event( IGameEvent *event )
{
    if ( g_cfg.m_misc.m_noisemaker_spam_mode != config::NOISEMAKER_SPAM_ON_KILL ||
         !g_cfg.m_misc.m_noisemaker_spam )
    {
        return;
    }

    i32 killer{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( "attacker" ) ) };
    i32 victim{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( "userid" ) ) };
    if ( killer == victim )
    {
        return;
    }

    if ( killer != g_tf2.m_engine->GetLocalPlayer( ) )
    {
        return;
    }

    if ( victim == g_tf2.m_engine->GetLocalPlayer( ) )
    {
        return;
    }

    DBGOUT( "misc::noisemaker_notify_event: Got event notification!" );
    m_should_hit_noisemaker = true;
}

bool misc::is_taunt_exploit_taking_place( )
{
    return this->m_is_taunt_exploit_taking_place;
}

void misc::on_enable_fog( bool skybox_pass )
{
    const config::fog_t &wanted{ g_cfg.m_fog };

    if ( wanted.m_mode == config::FOG_OFF || ( skybox_pass && !wanted.m_skybox ) )
    {
        return;
    }

    IMatRenderContext *render_context{ g_tf2.m_material_system->GetRenderContext( ) };

    if ( wanted.m_mode == config::FOG_REMOVE )
    {
        render_context->FogMode( MATERIAL_FOG_NONE );
    } else
    {
        const float color[ 3 ]{ static_cast< float >( wanted.m_color.r ) / 255.0f,
                                static_cast< float >( wanted.m_color.g ) / 255.0f,
                                static_cast< float >( wanted.m_color.b ) / 255.0f };

        render_context->FogMode( MATERIAL_FOG_LINEAR );
        render_context->FogColor3fv( color );
        render_context->FogStart( static_cast< float >( wanted.m_start ) );
        render_context->FogEnd( static_cast< float >( wanted.m_end ) );
        render_context->FogMaxDensity( static_cast< float >( wanted.m_density ) * 0.01f );
    }

    render_context->Release( );
}

void misc::fov( CViewSetup *setup )
{
    if ( !g_cfg.m_misc.m_fov_override && g_cfg.m_misc.m_removals[ config::REMOVAL_SCOPE_ZOOM ] == false )
    {
        return;
    }

    // Let the game handle FOV if we're zoomed without scope zoom removal.
    if ( g_cfg.m_misc.m_removals[ config::REMOVAL_SCOPE_ZOOM ] == false && g_cl.m_local->IsZoomed( ) )
    {
        return;
    }

    f32 fov_to_set = g_tf2.fov_desired->GetFloat( );
    if ( g_cfg.m_misc.m_fov_override )
    {
        fov_to_set = ( f32 )g_cfg.m_misc.m_fov;
    }

    // setting m_iDefaultFOV here as well prevents our fov from getting rescaled by the engine.
    g_cl.m_local->m_iFOV( ) = g_cl.m_local->m_iDefaultFOV( ) = setup->fov = fov_to_set;
}

// Rebuilds the check that is done in CTFPlayer::CheckForIdle.
// If the check for the respawn room wasn't in there, just sending IN_BULLRUSH or IN_GRENADE1 would work...
void misc::anti_afk( )
{
    if ( g_cl.m_local->InCond( TF_COND_TAUNTING ) ||
         g_cl.m_local->InCond( TF_COND_HALLOWEEN_KART ) )
    {
        return;
    }

    constexpr i32 MOVE_BUTTONS{ IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT };

    if ( !( g_cl.m_cmd->buttons & MOVE_BUTTONS ) &&
         g_cl.m_local->m_MoveType( ) == MOVETYPE_WALK )
    {
        g_cl.m_cmd->buttons |= IN_FORWARD;
    }

    i32 AWAY_TICKS{ TIME_TO_TICKS( 1.0f ) };
    if ( g_cl.m_cmd->command_number % ( AWAY_TICKS * 2 ) < AWAY_TICKS )
    {
        g_cl.m_cmd->buttons |= IN_BULLRUSH;
    }
}

void misc::perform_taunt_cancel_exploit( )
{
    if ( g_cl.m_weapon->GetWeaponID( ) != TF_WEAPON_FLAMETHROWER )
    {
        this->m_is_taunt_exploit_taking_place = false;
        return;
    }

    if ( !g_cl.m_local->IsTaunting( ) || g_cl.m_local->m_nWaterLevel( ) < 1 )
    {
        this->m_is_taunt_exploit_taking_place = false;
        return;
    }

    i32 charged_airblast{ 0 };
    charged_airblast = g_cl.attrib_hook( g_cl.m_weapon, charged_airblast, ATLAS_XORSTR( "set_charged_airblast" ).get( ) );
    if ( charged_airblast == 0 )
    {
        this->m_is_taunt_exploit_taking_place = false;
        return;
    }

    g_cl.m_cmd->buttons |= IN_DUCK;
    this->m_is_taunt_exploit_taking_place = true;
}

void misc::freecam( CViewSetup *setup, const vec3 &eye )
{
    const bind_t &key{ g_cfg.m_misc.m_freecam_key };
    const bool    gated{ key.m_key != 0 || key.m_mode == bind_mode_t::always };

    const bool wanted{ g_cfg.m_misc.m_freecam && g_cl.m_local->IsAlive( ) &&
                       ( !gated || gui::bind_active( g_cfg.m_misc.m_freecam_key ) ) };

    if ( wanted != this->m_freecam_active )
    {
        this->m_freecam_active = wanted;

        if ( wanted )
        {
            this->m_freecam_origin = eye;
        } else
        {
            g_tf2.m_engine->SetViewAngles( this->m_freecam_angles );
        }
    }

    if ( !wanted )
    {
        return;
    }

    vec3 angles{ };
    g_tf2.m_engine->GetViewAngles( angles );

    vec3 forward{ };
    vec3 right{ };
    math::angle_vectors( angles, &forward, &right );

    const i32  buttons{ this->m_freecam_buttons };
    const auto axis{ [ buttons ]( i32 positive, i32 negative )
    {
        return static_cast< f32 >( ( buttons & positive ) != 0 ) - static_cast< f32 >( ( buttons & negative ) != 0 );
    } };

    const vec3 wish{ forward * axis( IN_FORWARD, IN_BACK ) + right * axis( IN_MOVERIGHT, IN_MOVELEFT ) +
                     vec3{ 0.0f, 0.0f, axis( IN_JUMP, IN_DUCK ) } };

    this->m_freecam_origin += wish * ( static_cast< f32 >( g_cfg.m_misc.m_freecam_speed ) * g_tf2.m_globals->absoluteframetime );

    setup->origin = this->m_freecam_origin;
    setup->angles = angles;
}

void misc::freecam_input( )
{
    constexpr i32 MOVE_BUTTONS{ IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_JUMP | IN_DUCK };

    if ( !this->m_freecam_active )
    {
        this->m_freecam_angles  = g_cl.m_cmd->viewangles;
        this->m_freecam_buttons = 0;
        return;
    }

    this->m_freecam_buttons = g_cl.m_cmd->buttons & MOVE_BUTTONS;

    g_cl.m_cmd->buttons &= ~MOVE_BUTTONS;
    g_cl.m_cmd->forwardmove = 0.0f;
    g_cl.m_cmd->sidemove    = 0.0f;
    g_cl.m_cmd->upmove      = 0.0f;
    g_cl.m_cmd->viewangles  = this->m_freecam_angles;
}

void misc::advance_thirdperson( bool wanted )
{
    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    const double previous{ this->m_third_time };

    this->m_third_time = now;

    const float seconds{ static_cast< float >( g_cfg.m_misc.m_thirdperson_smooth_time ) / 1000.0f };

    if ( !g_cfg.m_misc.m_thirdperson_smooth || seconds <= 0.0f || previous <= 0.0 )
    {
        this->m_third_fraction = wanted ? 1.0f : 0.0f;
        return;
    }

    const float delta{ math::smaller( static_cast< float >( now - previous ), 0.1f ) };
    const float step{ delta / seconds };

    this->m_third_fraction = math::clamp( this->m_third_fraction + ( wanted ? step : -step ),
                                          0.0f, 1.0f );
}

void misc::restart_minigun_effects( C_TFMinigun *minigun )
{
    if ( minigun->m_iWeaponState( ) != AC_STATE_FIRING )
    {
        return;
    }

    minigun->StartMuzzleEffect( );
    minigun->StartBrassEffect( );
}

void misc::thirdperson( CViewSetup *setup, const vec3 &eye )
{
    if ( g_cl.m_local->IsTaunting( ) )
    {
        return;
    }

    const bind_t &key{ g_cfg.m_misc.m_thirdperson_key };
    const bool    gated{ key.m_key != 0 || key.m_mode == bind_mode_t::always };

    const bool wanted{ g_cfg.m_misc.m_thirdperson && g_cl.m_local->IsAlive( ) &&
                       ( !gated || gui::bind_active( g_cfg.m_misc.m_thirdperson_key ) ) };

    this->advance_thirdperson( wanted );

    const bool pulled_out{ this->m_third_fraction > 0.0f || this->m_freecam_active }; // this name is so bad, but its so funny
    if ( pulled_out != g_tf2.m_input->CAM_IsThirdPerson( ) )
    {
        if ( pulled_out )
        {
            g_tf2.m_input->CAM_ToThirdPerson( );
        } else
        {
            g_tf2.m_input->CAM_ToFirstPerson( );
        }

        g_cl.m_local->ThirdPersonSwitch( pulled_out );

        if ( g_cl.m_weapon && g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_MINIGUN )
        {
            this->restart_minigun_effects( g_cl.m_weapon->As< C_TFMinigun * >( ) );
        }
    }

    if ( !pulled_out || this->m_freecam_active )
    {
        return;
    }

    vec3 angles{ };
    g_tf2.m_engine->GetViewAngles( angles );

    vec3 forward{ };
    vec3 right{ };
    vec3 up{ };
    math::angle_vectors( angles, &forward, &right, &up );

    const float travel{ this->m_third_fraction };

    const vec3 wanted_origin{ eye - forward * ( static_cast< float >( g_cfg.m_misc.m_thirdperson_distance ) * travel ) +
                              right * ( static_cast< float >( g_cfg.m_misc.m_thirdperson_side ) * travel ) +
                              up * ( static_cast< float >( g_cfg.m_misc.m_thirdperson_height ) * travel ) };

    setup->angles = angles;
    setup->origin = wanted_origin;

    if ( !g_cfg.m_misc.m_thirdperson_collision )
    {
        return;
    }
    trace_t                       trace{ };
    Ray_t                         ray{ };
    CTraceFilterWorldAndPropsOnly filter{ };

    ray.Init( eye, wanted_origin, vec3{ -8.0f, -8.0f, -8.0f }, vec3{ 8.0f, 8.0f, 8.0f } );
    g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );

    setup->origin = trace.endpos;
}
