#include "anti_aim.hpp"

ATLAS_NO_DESTROY anti_aim g_anti_aim{ };

#include "anims.hpp"
#include "fakelag.hpp"
#include "melee.hpp"
#include "tickbase.hpp"

ATLAS_FORCE_INLINE f32 bin_yaw( i32 bin )
{
    return static_cast< f32 >( bin ) * ( 360.0f / AUTO_YAW_BINS ) - 180.0f;
}

ATLAS_FORCE_INLINE i32 yaw_bin( f32 yaw )
{
    return static_cast< i32 >( ( math::normalize_yaw( yaw ) + 180.0f ) / ( 360.0f / AUTO_YAW_BINS ) + 0.5f ) % AUTO_YAW_BINS;
}

ATLAS_FORCE_INLINE vec3 spin( const vec3 &point, const vec3 &axis, f32 degrees )
{
    const f32 s{ math::sin( DEG2RAD( degrees ) ) };
    const f32 c{ math::cos( DEG2RAD( degrees ) ) };

    const f32 x{ point.x - axis.x };
    const f32 y{ point.y - axis.y };

    return vec3{ axis.x + x * c - y * s, axis.y + x * s + y * c, point.z };
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/public/bone_setup.cpp#L5118
ATLAS_FORCE_INLINE f32 torso_twist( const mstudioposeparamdesc_t *aim_yaw, f32 eye_yaw, f32 feet_yaw )
{
    return -math::clamp( -math::normalize_yaw( eye_yaw - feet_yaw ), aim_yaw->start, aim_yaw->end );
}

ATLAS_FORCE_INLINE bool is_headshot_weapon( C_TFWeaponBase *weapon )
{
    switch ( weapon->GetWeaponID( ) )
    {
        case TF_WEAPON_SNIPERRIFLE:
        case TF_WEAPON_SNIPERRIFLE_CLASSIC:
        case TF_WEAPON_SNIPERRIFLE_DECAP:
        case TF_WEAPON_COMPOUND_BOW: return true;
        case TF_WEAPON_REVOLVER: return weapon->m_iItemDefinitionIndex( ) == Spy_m_TheAmbassador || weapon->m_iItemDefinitionIndex( ) == Spy_m_FestiveAmbassador;
        default: return false;
    }
}

ATLAS_FORCE_INLINE bool is_world_clear( const vec3 &from, const vec3 &to )
{
    trace_t                       trace{ };
    Ray_t                         ray{ };
    CTraceFilterWorldAndPropsOnly filter{ };

    ray.Init( from, to );
    g_tf2.m_trace->TraceRay( ray, MASK_SHOT, &filter, &trace );

    return trace.fraction == 1.0f;
}

ATLAS_FORCE_INLINE bool build_local_bones( CStudioHdr *studio, const pose_snapshot_t &snapshot, i32 bone_mask, matrix3x4 *bones )
{
    alignas( 16 ) vec3       pos[ MAX_STUDIO_BONES ];
    alignas( 16 ) quaternion q[ MAX_STUDIO_BONES ];

    g_tf2.m_mdl_cache->BeginLock( );
    const bool built{ g_bone_mgr.build( studio, snapshot, bone_mask, bones, pos, q ) };
    g_tf2.m_mdl_cache->EndLock( );

    return built;
}

ATLAS_FORCE_INLINE bool is_better_yaw( const yaw_score_t &score, const yaw_score_t &best )
{
    if ( score.m_real_exposed != best.m_real_exposed )
    {
        return score.m_real_exposed < best.m_real_exposed;
    }

    if ( score.m_overlap != best.m_overlap )
    {
        return score.m_overlap < best.m_overlap;
    }

    return score.m_fake_exposed > best.m_fake_exposed;
}

void anti_aim::think( )
{
    if ( !this->should_run( ) )
    {
        return;
    }

    if ( g_tickbase.shifting( ) )
    {
        return;
    }

    this->fetch_targets( );

    this->m_profile = g_cfg.m_anti_hack.m_aa_profiles[ client::get_profile( ) ];
    g_tf2.m_engine->GetViewAngles( this->m_camera_angles );

    // the auto yaws plan the next batch with as many choked commands as the last one had
    if ( g_tf2.m_cl->chokedcommands == 0 )
    {
        this->m_planned_choke = this->m_last_choked;
    }

    this->m_last_choked    = g_tf2.m_cl->chokedcommands;
    this->m_using_fake_yaw = this->uses_fake_yaw( );

    // Handle fakelag hijack.
    if ( !g_fakelag.is_active( ) )
    {
        if ( this->m_using_fake_yaw )
        {
            g_cl.m_send_packet = g_tf2.m_cl->chokedcommands >= 3;
        }
    }

    if ( this->m_using_fake_yaw )
    {
        this->perform_min_walk( );
    }

    if ( g_cl.m_will_fire || g_melee.aimed_this_command( ) )
    {
        if ( g_cfg.m_anti_hack.m_aa_mode == config::AA_MODE_LEGIT )
        {
            return;
        } else
        {
            g_cl.m_cmd->viewangles.x = math::signbit( this->m_wish_pitch ) ? g_cl.m_cmd->viewangles.x + 1080.f : g_cl.m_cmd->viewangles.x + -1080.f;
            return;
        }
    }

    switch ( g_cfg.m_anti_hack.m_aa_mode )
    {
        case config::AA_MODE_LEGIT:
        {
            this->m_rage_mode = false;
            this->handle_legit( );
            break;
        }

        case config::AA_MODE_RAGE:
        {
            this->m_rage_mode = true;
            this->handle_rage( );
            break;
        }

        default: Assert( 0 && "How did we get here?" ); break;
    }
}

void anti_aim::paint( )
{
    if ( !should_run( ) )
    {
        return;
    }

    if ( !g_tf2.m_input->CAM_IsThirdPerson( ) )
    {
        return;
    }

    if ( !g_cfg.m_anti_hack.m_draw_real_hitboxes && !g_cfg.m_anti_hack.m_draw_fake_hitboxes )
    {
        return;
    }

    CStudioHdr *studio{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< CStudioHdr * >( g_cl.m_local ) };
    if ( !studio || !studio->m_pStudioHdr )
    {
        return;
    }

    pose_snapshot_t snapshot{ };
    if ( g_cfg.m_anti_hack.m_draw_real_hitboxes && g_bone_mgr.stored_pose( g_cl.m_local, studio->m_pStudioHdr, snapshot ) )
    {
        snapshot.m_origin = g_cl.m_local->GetAbsOrigin( );
        this->draw_hitboxes( studio, snapshot );
    }

    if ( g_cfg.m_anti_hack.m_draw_fake_hitboxes && g_bone_mgr.shown_pose( studio->m_pStudioHdr, snapshot ) )
    {
        snapshot.m_origin = g_cl.m_local->GetAbsOrigin( );
        this->draw_hitboxes( studio, snapshot, true );
    }
}

bool anti_aim::build_shown_bones( matrix3x4 *bones, i32 &bone_count )
{
    if ( !this->should_run( ) )
    {
        return false;
    }

    CStudioHdr *studio{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< CStudioHdr * >( g_cl.m_local ) };
    if ( !studio || !studio->m_pStudioHdr )
    {
        return false;
    }

    pose_snapshot_t snapshot{ };
    if ( !g_bone_mgr.shown_pose( studio->m_pStudioHdr, snapshot ) )
    {
        return false;
    }

    snapshot.m_origin = g_cl.m_local->GetAbsOrigin( );

    bone_count = studio->m_pStudioHdr->numbones;
    return build_local_bones( studio, snapshot, BONE_USED_BY_ANYTHING, bones );
}

// For use in fakelag.
// If it's enabled, fakelag will override the choke times from anti-aim.
// However, for fake angles to work, a minimum choke is required for the animation state desync to happen on the server.
// So, this allows fakelag to keep it's min bounds for the fakelag as needed against anti-aim.
bool anti_aim::using_fake_angles( )
{
    return should_run( ) && this->m_using_fake_yaw;
}

void anti_aim::reset( )
{
    this->m_profile          = { };
    this->m_wish_real_yaw    = 0.0f;
    this->m_wish_fake_yaw    = 0.0f;
    this->m_wish_pitch       = 0.0f;
    this->m_jitter_flip      = false;
    this->m_rage_mode        = false;
    this->m_fake_update_time = 0.0f;
    this->m_real_update_time = 0.0f;
    this->m_spin_yaw         = 0.0f;
    this->m_cycle_fake       = false;
    this->m_cycle_real       = false;
    this->m_using_fake_yaw   = false;
    this->m_last_choked      = 0;
    this->m_planned_choke    = 0;
    this->m_camera_angles    = { };
    this->m_sent_angles      = { };
    this->m_targets.clear( );
}

void anti_aim::on_va_lock( vec3 &angle )
{
    if ( !should_run( ) || g_cfg.m_anti_hack.m_aa_mode != config::AA_MODE_RAGE )
    {
        return;
    }

    angle.x = math::signbit( this->m_wish_pitch ) ? angle.x + 1080.f : angle.x + -1080.f;
}

void anti_aim::on_move_sent( i32 command )
{
    this->m_sent_angles = g_tf2.m_input->m_pCommands[ command % MULTIPLAYER_BACKUP ].viewangles;
}

bool anti_aim::should_run( )
{
    if ( !g_cl.m_local ||
         !g_cl.m_weapon )
    {
        return false;
    }

    if ( !g_cl.m_local->IsAlive( ) ||
         g_cl.m_local->IsInKart( ) ||
         g_cl.m_local->IsTaunting( ) ||
         g_cl.m_local->IsCharging( ) )
    {
        return false;
    }

    if ( g_cl.m_local->m_MoveType( ) == MOVETYPE_NOCLIP ||
         g_cl.m_local->m_MoveType( ) == MOVETYPE_LADDER )
    {
        return false;
    }

    if ( !g_cfg.m_anti_hack.m_anti_aim_enabled )
    {
        return false;
    }

    if ( !gui::bind_active( g_cfg.m_anti_hack.m_anti_aim_key ) )
    {
        return false;
    }

    return true;
}

void anti_aim::perform_min_walk( )
{
    if ( g_cl.m_cmd->forwardmove != 0.0f ||
         g_cl.m_cmd->sidemove != 0.0f ||
         g_cl.m_local->m_vecVelocity( ).length2d( ) > 10.0f ||
         !g_cl.m_local->IsOnGround( ) )
    {
        return;
    }

    const f32 speed{ g_cl.m_local->IsDucked( ) ? 3.75f : 1.25f };

    const f32 yaw{ DEG2RAD( math::fmod( g_tf2.m_globals->curtime * 360.f * 2.0f, 360.f ) ) };

    g_cl.m_cmd->forwardmove = math::cos( yaw ) * speed;
    g_cl.m_cmd->sidemove    = math::sin( yaw ) * speed;
}

void anti_aim::handle_legit( )
{
    if ( !this->m_using_fake_yaw )
    {
        return;
    }

    if ( g_tf2.m_cl->chokedcommands == 0 && this->scan_head( ) )
    {
        this->m_wish_real_yaw = this->determine_best_real_yaw( this->m_camera_angles.y );
    }

    if ( g_cl.m_send_packet )
    {
        return;
    }

    g_cl.m_cmd->viewangles.y = this->m_wish_real_yaw;
}

void anti_aim::handle_rage( )
{
    this->update_rage_states( );

    this->calc_wish_angles( );

    this->handle_pitch( );

    if ( this->m_using_fake_yaw )
    {
        if ( g_cl.m_send_packet )
        {
            this->handle_fake( );
        } else
        {
            this->handle_real( );
        }
    } else
    {
        this->handle_real( );
    }
}

void anti_aim::fetch_targets( )
{
    this->m_targets.clear( );

    for ( i32 i{ 1 }; i <= g_tf2.m_engine->GetMaxClients( ); ++i )
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

        // Would be a weird case if the engine started adding non-player entities into the reserved player entity slots...
        if ( client_class->GetClassID( ) != ETFClassID::CTFPlayer )
        {
            continue;
        }

        if ( base_entity->IsDormant( ) )
        {
            continue;
        }

        const auto player{ base_entity->As< C_TFPlayer * >( ) };

        // Don't consider teammates a threat unless friendly fire is enabled on the server and ragebot wants to target teammates.
        if ( player->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) &&
             ( !g_tf2.mp_friendlyfire->GetBool( ) || g_cfg.m_ragebot.m_preferences[ config::PREFERENCE_FRIENDLY_FIRE ] == false ) )
        {
            continue;
        }

        if ( !player->IsAlive( ) ||
             player->IsTaunting( ) ||
             player->IsCloaked( ) ||
             player->IsBonked( ) ||
             player->IsInKart( ) ||
             player->IsGhost( ) )
        {
            continue;
        }

        player_info_t info{ };
        if ( g_tf2.m_engine->GetPlayerInfo( i, &info ) )
        {
            // Ragebot would ignore this person, so don't class them as an anti-aim threat.
            if ( g_cl.is_ignored_player( info.friendsID ) )
            {
                continue;
            }
        }

        vec3 va_to{ };
        math::vector_angles( player->WorldSpaceCenter( ) - g_cl.m_local->Weapon_ShootPosition( ), va_to );

        auto tgt{ this->m_targets.emplace_back( ) };
        Assert( tgt != nullptr );

        C_BaseEntity *weapon{ player->m_hActiveWeapon( ).Get( ) };

        tgt->m_player    = player;
        tgt->m_angle_to  = va_to;
        tgt->m_shoot_pos = player->Weapon_ShootPosition( );
        tgt->m_fov_to    = math::calc_fov( this->m_camera_angles, va_to );
        tgt->m_is_threat = !g_cfg.m_anti_hack.m_headshot_threats_only || ( weapon && is_headshot_weapon( weapon->As< C_TFWeaponBase * >( ) ) );
    }

    if ( this->m_targets.empty( ) )
    {
        return;
    }

    atl::sort( this->m_targets.begin( ), this->m_targets.end( ), [ & ]( const anti_aim_target_t &first, const anti_aim_target_t &second ) -> bool
    {
        return first.m_fov_to < second.m_fov_to;
    } );
}

bool anti_aim::should_hide_head( )
{
    if ( !g_cfg.m_anti_hack.m_headshot_threats_only )
    {
        return true;
    }

    for ( const anti_aim_target_t &target : this->m_targets )
    {
        if ( target.m_is_threat )
        {
            return true;
        }
    }

    return false;
}

bool anti_aim::uses_fake_yaw( )
{
    if ( g_cfg.m_anti_hack.m_aa_mode == config::AA_MODE_LEGIT )
    {
        return this->should_hide_head( );
    }

    // either we have an unset fake yaw mode or both fake yaw and real yaw are both using spin (which share the same angles)
    return this->m_profile.m_fake_yaw_mode != config::YAW_MODE_NONE &&
           !( this->m_profile.m_real_yaw_mode == config::YAW_MODE_SPIN && this->m_profile.m_fake_yaw_mode == config::YAW_MODE_SPIN );
}

void anti_aim::handle_real( )
{
    g_cl.m_cmd->viewangles.y = this->real_command_yaw( );
}

void anti_aim::handle_fake( )
{
    if ( !this->m_using_fake_yaw )
    {
        return;
    }

    g_cl.m_cmd->viewangles.y = this->fake_command_yaw( );
}

void anti_aim::handle_pitch( )
{
    g_cl.m_cmd->viewangles.x = this->m_wish_pitch;
}

void anti_aim::update_rage_states( )
{
    // Jitter angles only have a noticeable effect when unchoking, so don't update these when we're choking.
    if ( g_cl.m_send_packet )
    {
        this->m_jitter_flip = !this->m_jitter_flip;
    }

    // Prevents a double update of spin.
    bool has_already_updated_spin{ false };

    switch ( this->m_profile.m_real_yaw_mode )
    {
        case config::YAW_MODE_CYLICAL:
        {
            if ( this->m_real_update_time < g_tf2.m_globals->curtime )
            {
                DBGOUT( "anti_aim: Updating cycle for real." );
                this->m_cycle_real       = !this->m_cycle_real;
                this->m_real_update_time = g_tf2.m_globals->curtime + m_profile.m_real_cycle_time;
                break;
            }

            break;
        }

        case config::YAW_MODE_SPIN:
        {
            if ( g_cl.m_send_packet )
            {
                DBGOUT( "anti_aim: Updating spin angle for real." );
                this->m_spin_yaw += ( f32 )this->m_profile.m_spin_amount;
                this->m_spin_yaw         = math::normalize_yaw( this->m_spin_yaw );
                has_already_updated_spin = true;
                break;
            }

            break;
        }

        default: break;
    }

    // Prevent a choke if we aren't using fake yaw.
    if ( !this->m_using_fake_yaw )
    {
        DBGOUT( "anti_aim: Fake yaw not set or fake and real both are using spin, not updating fake states." );
        return;
    }

    switch ( m_profile.m_fake_yaw_mode )
    {
        case config::YAW_MODE_CYLICAL:
        {
            if ( this->m_fake_update_time < g_tf2.m_globals->curtime )
            {
                DBGOUT( "anti_aim: Updating cycle for fake." );
                this->m_cycle_fake       = !this->m_cycle_fake;
                this->m_fake_update_time = g_tf2.m_globals->curtime + this->m_profile.m_fake_cycle_time;
                break;
            }

            break;
        }

        case config::YAW_MODE_SPIN:
        {
            // only the sent command shows the fake, so stepping it on the choked ones as well would make a step that
            // divides 360 over a batch (90 with three chokes) land every send on the same angle;
            if ( g_cl.m_send_packet && !has_already_updated_spin )
            {
                DBGOUT( "anti_aim: Updating spin angle for fake." );
                this->m_spin_yaw += ( f32 )this->m_profile.m_spin_amount;
                this->m_spin_yaw = math::normalize_yaw( this->m_spin_yaw );
                break;
            }
        }
    }
}

void anti_aim::calc_wish_angles( )
{
    const bool hide_head{ this->should_hide_head( ) };

    // our animstate only catches up with the server when a batch gets sent, so the auto yaws are planned once per batch!
    bool plan_auto_yaw{ false };
    if ( hide_head && g_tf2.m_cl->chokedcommands == 0 &&
         ( this->m_profile.m_real_yaw_mode == config::YAW_MODE_AUTO || this->m_profile.m_fake_yaw_mode == config::YAW_MODE_AUTO ) )
    {
        plan_auto_yaw = this->scan_head( );
    }

    switch ( this->m_profile.m_real_yaw_mode )
    {
        case config::YAW_MODE_NONE:
        {
            this->m_wish_real_yaw = this->m_camera_angles.y;
            break;
        }

        case config::YAW_MODE_AUTO:
        {
            if ( !hide_head )
            {
                this->m_wish_real_yaw = this->m_camera_angles.y;
            } else if ( plan_auto_yaw )
            {
                this->m_wish_real_yaw = this->determine_best_real_yaw( );
            }

            break;
        }

        case config::YAW_MODE_CYLICAL:
        {
            this->m_wish_real_yaw = this->m_cycle_real ? this->m_profile.m_cylical_pos_2_real : this->m_profile.m_cylical_pos_1_real;
            break;
        }

        case config::YAW_MODE_STATIC:
        {
            this->m_wish_real_yaw = this->m_profile.m_static_real_angle;
            break;
        }

        case config::YAW_MODE_SPIN:
        {
            this->m_wish_real_yaw = this->m_spin_yaw;
            break;
        }

        default: Assert( 0 && "How did we get here?" ); break;
    }

    switch ( this->m_profile.m_fake_yaw_mode )
    {
        case config::YAW_MODE_AUTO:
        {
            // planned after the real, so it works around the real yaw we just picked
            if ( !hide_head )
            {
                this->m_wish_fake_yaw = this->m_camera_angles.y;
            } else if ( plan_auto_yaw )
            {
                this->m_wish_fake_yaw = this->determine_best_fake_yaw( this->real_command_yaw( ) );
            }

            break;
        }

        case config::YAW_MODE_CYLICAL:
        {
            this->m_wish_fake_yaw = this->m_cycle_fake ? this->m_profile.m_cylical_pos_2_fake : this->m_profile.m_cylical_pos_1_fake;
            break;
        }

        case config::YAW_MODE_STATIC:
        {
            this->m_wish_fake_yaw = this->m_profile.m_static_fake_angle;
            break;
        }

        case config::YAW_MODE_SPIN:
        {
            this->m_wish_fake_yaw = this->m_spin_yaw;
            break;
        }

        default: break;
    }

    // Fix potential configuration errors.
    if ( this->m_profile.m_break_pitch )
    {
        switch ( this->m_profile.m_real_pitch_mode )
        {
            case config::REAL_PITCH_UP:
            case config::REAL_PITCH_DOWN: break;
            default: this->m_profile.m_real_pitch_mode = config::REAL_PITCH_UP;
        }

        this->m_profile.m_fake_pitch_mode = config::FAKE_PITCH_OFF;
    }

    switch ( this->m_profile.m_real_pitch_mode )
    {
        case config::REAL_PITCH_UP:
        {
            switch ( this->m_profile.m_fake_pitch_mode )
            {
                case config::FAKE_PITCH_DOWN:
                {
                    this->m_wish_pitch = 271.f;
                    break;
                }

                case config::FAKE_PITCH_JITTER:
                {
                    this->m_wish_pitch = this->m_jitter_flip ? -89.f : 271.f;
                    break;
                }

                default:
                {
                    if ( this->m_profile.m_break_pitch )
                    {
                        this->m_wish_pitch = this->m_jitter_flip ? -91.f : -89.f;
                    } else
                    {
                        this->m_wish_pitch = -89.f;
                    }

                    break;
                }
            }

            break;
        }

        case config::REAL_PITCH_DOWN:
        {
            switch ( this->m_profile.m_fake_pitch_mode )
            {
                case config::FAKE_PITCH_UP:
                {
                    this->m_wish_pitch = -271.f;
                    break;
                }

                case config::FAKE_PITCH_JITTER:
                {
                    this->m_wish_pitch = this->m_jitter_flip ? 89.f : -271.f;
                    break;
                }

                default:
                {
                    if ( this->m_profile.m_break_pitch )
                    {
                        this->m_wish_pitch = this->m_jitter_flip ? 91.f : 89.f;
                    } else
                    {
                        this->m_wish_pitch = 89.f;
                    }

                    break;
                }
            }

            break;
        }

        case config::REAL_PITCH_ZERO:
        {
            switch ( this->m_profile.m_fake_pitch_mode )
            {
                case config::FAKE_PITCH_UP:
                {
                    this->m_wish_pitch = -1080.f;
                    break;
                }

                case config::FAKE_PITCH_DOWN:
                {
                    this->m_wish_pitch = 1080.f;
                    break;
                }

                case config::FAKE_PITCH_JITTER:
                {
                    this->m_wish_pitch = this->m_jitter_flip ? -1080.f : 1080.f;
                    break;
                }

                default: this->m_wish_pitch = 0.f; break;
            }

            break;
        }

        case config::REAL_PITCH_JITTER:
        {
            switch ( this->m_profile.m_fake_pitch_mode )
            {
                case config::FAKE_PITCH_UP:
                {
                    this->m_wish_pitch = this->m_jitter_flip ? -271.f : 89.f;
                    break;
                }

                case config::FAKE_PITCH_DOWN:
                {
                    this->m_wish_pitch = this->m_jitter_flip ? 271.f : -89.f;
                    break;
                }

                case config::FAKE_PITCH_JITTER:
                {
                    this->m_wish_pitch = this->m_jitter_flip ? -271.f : 271.f;
                    break;
                }

                default:
                {
                    this->m_wish_pitch = this->m_jitter_flip ? 89.f : -89.f;
                    break;
                }
            }

            break;
        }
    }
}

f32 anti_aim::get_real_base_yaw( )
{
    // Some of these yaws run their own logic that having base yaw additions would cause problems on. So we correct them here.
    switch ( this->m_profile.m_real_yaw_mode )
    {
        case config::YAW_MODE_NONE:
        case config::YAW_MODE_AUTO:
        case config::YAW_MODE_SPIN: return 0.0f;

        // Otherwise, we fetch appropriate base yaw according to the config here.
        default:
        {
            switch ( this->m_profile.m_yaw_base )
            {
                case config::YAW_BASE_NONE: return 0.0f;
                case config::YAW_BASE_EYE: return this->m_camera_angles.y;
                case config::YAW_BASE_AT_TARGET:
                {
                    if ( !this->m_targets.empty( ) )
                    {
                        for ( auto &&tgt : this->m_targets )
                        {
                            return tgt.m_angle_to.y;
                        }
                    }

                    return this->m_camera_angles.y;
                }

                default: return this->m_camera_angles.y;
            }
        }
    }
}

f32 anti_aim::get_fake_base_yaw( )
{
    // Some of these yaws run their own logic that having base yaw additions would cause problems on. So we correct them here.
    switch ( m_profile.m_fake_yaw_mode )
    {
        case config::YAW_MODE_AUTO:
        case config::YAW_MODE_SPIN: return 0.0f;

        // Otherwise, we fetch appropriate base yaw according to the config here.
        default:
        {
            switch ( m_profile.m_yaw_base )
            {
                case config::YAW_BASE_NONE: return 0.0f;
                case config::YAW_BASE_EYE: return this->m_camera_angles.y;
                case config::YAW_BASE_AT_TARGET:
                {
                    if ( !m_targets.empty( ) )
                    {
                        for ( auto &&tgt : m_targets )
                        {
                            return tgt.m_angle_to.y;
                        }
                    }

                    return this->m_camera_angles.y;
                }

                default: return this->m_camera_angles.y;
            }
        }
    }
}

f32 anti_aim::get_jitter_additive( bool fake )
{
    if ( fake )
    {
        return this->m_profile.m_apply_jitter_fake ? this->m_jitter_flip ? -this->m_profile.m_jitter_amount : this->m_profile.m_jitter_amount : 0;
    } else
    {
        return this->m_profile.m_apply_jitter_real ? this->m_jitter_flip ? -this->m_profile.m_jitter_amount : this->m_profile.m_jitter_amount : 0;
    }
}

f32 anti_aim::real_command_yaw( )
{
    return math::normalize_yaw( this->m_wish_real_yaw + this->get_real_base_yaw( ) + this->get_jitter_additive( false ) );
}

f32 anti_aim::fake_command_yaw( )
{
    return math::normalize_yaw( this->m_wish_fake_yaw + this->get_fake_base_yaw( ) + this->get_jitter_additive( true ) );
}

bool anti_aim::scan_head( )
{
    CTFPlayerAnimState *anim_state{ g_cl.m_local->m_PlayerAnimState( ) };
    CTFPlayerAnimState *sent_state{ g_anims.sent_anim_state( ) };
    CStudioHdr         *studio{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< CStudioHdr * >( g_cl.m_local ) };
    if ( !anim_state || !sent_state || anim_state->m_iAimYaw( ) < 0 || !studio || !studio->m_pStudioHdr )
    {
        return false;
    }

    head_scan_t    &scan{ this->m_head_scan };
    pose_snapshot_t snapshot{ };
    pose_snapshot_t shown{ };
    if ( !g_bone_mgr.stored_pose( g_cl.m_local, studio->m_pStudioHdr, snapshot ) || !g_bone_mgr.shown_pose( studio->m_pStudioHdr, shown ) )
    {
        return false;
    }

    snapshot.m_origin = g_cl.m_local->m_vecOrigin( );
    shown.m_origin    = snapshot.m_origin;

    matrix3x4 shown_bones[ MAX_STUDIO_BONES ]{ };
    if ( !build_local_bones( studio, snapshot, BONE_USED_BY_HITBOX, scan.m_bones ) ||
         !build_local_bones( studio, shown, BONE_USED_BY_HITBOX, shown_bones ) )
    {
        return false;
    }

    scan.m_set          = studio->m_pStudioHdr->pHitboxSet( g_cl.m_local->m_nHitboxSet( ) );
    scan.m_head         = scan.m_set->pHitbox( HITBOX_HEAD );
    scan.m_aim_yaw      = studio->m_pStudioHdr->pPoseParameter( anim_state->m_iAimYaw( ) );
    scan.m_origin       = snapshot.m_origin;
    scan.m_goal_feet    = sent_state->m_flGoalFeetYaw( );
    scan.m_current_feet = sent_state->m_flCurrentFeetYaw( );
    scan.m_moving       = g_cl.m_local->GetAbsVelocity( ).length( ) > 1.0f;
    scan.m_head_yaw     = scan.m_current_feet + torso_twist( scan.m_aim_yaw, sent_state->m_flEyeYaw( ), scan.m_current_feet );
    scan.m_shown_yaw    = shown.m_angles.y + torso_twist( scan.m_aim_yaw, this->m_sent_angles.y, shown.m_angles.y );

    head_points( scan.m_head, scan.m_bones, scan.m_points );
    head_points( scan.m_head, shown_bones, scan.m_shown_points );

    vec3 forward{ };
    math::angle_vectors( this->m_camera_angles, &forward );
    scan.m_front_eye = scan.m_points[ 0 ] + forward * MAX_TRACE_LENGTH;

    for ( anti_aim_target_t &target : this->m_targets )
    {
        target.m_sees_head = target.m_is_threat && is_world_clear( target.m_shoot_pos, scan.m_points[ 0 ] );
    }

    this->count_exposure( scan.m_bones, scan.m_points, scan.m_head_yaw, scan.m_exposed );
    this->count_exposure( shown_bones, scan.m_shown_points, scan.m_shown_yaw, scan.m_shown_exposed );

    return true;
}

void anti_aim::count_exposure( const matrix3x4 *bones, const vec3 *points, f32 built_yaw, i32 *exposed )
{
    const head_scan_t &scan{ this->m_head_scan };

    for ( i32 bin{ 0 }; bin < AUTO_YAW_BINS; ++bin )
    {
        exposed[ bin ] = 0;

        const f32 turn{ built_yaw - bin_yaw( bin ) };
        for ( const anti_aim_target_t &target : this->m_targets )
        {
            if ( !target.m_sees_head )
            {
                continue;
            }

            const vec3 eye{ spin( target.m_shoot_pos, scan.m_origin, turn ) };
            for ( i32 point{ 0 }; point < HEAD_SCAN_POINTS; ++point )
            {
                if ( first_hitbox( scan.m_set, bones, eye, points[ point ] ) == HITBOX_HEAD )
                {
                    ++exposed[ bin ];
                }
            }
        }
    }
}

f32 anti_aim::settled_head_yaw( f32 real_yaw, f32 fake_yaw )
{
    const head_scan_t &scan{ this->m_head_scan };

    if ( !this->m_using_fake_yaw )
    {
        fake_yaw = real_yaw;
    }

    const i32 settle_commands{ static_cast< i32 >( 180.0f / ( 720.0f * TICK_INTERVAL ) ) + 1 };

    f32 goal_feet{ scan.m_goal_feet };
    f32 current_feet{ scan.m_current_feet };
    for ( i32 command{ 0 }; command < settle_commands; command += this->m_planned_choke + 1 )
    {
        for ( i32 choked{ 0 }; choked < this->m_planned_choke; ++choked )
        {
            step_feet_yaw( goal_feet, current_feet, real_yaw, scan.m_moving );
        }

        step_feet_yaw( goal_feet, current_feet, fake_yaw, scan.m_moving );
    }

    return current_feet + torso_twist( scan.m_aim_yaw, fake_yaw, current_feet );
}

i32 anti_aim::count_overlap( f32 head_yaw, f32 fake_yaw )
{
    i32  overlap{ 0 };
    bool seen{ false };

    for ( const anti_aim_target_t &target : this->m_targets )
    {
        if ( !target.m_sees_head )
        {
            continue;
        }

        seen = true;
        overlap += this->count_shot_overlap( target.m_shoot_pos, head_yaw, fake_yaw );
    }

    if ( !seen )
    {
        return this->count_shot_overlap( this->m_head_scan.m_front_eye, head_yaw, fake_yaw );
    }

    return overlap;
}

i32 anti_aim::count_shot_overlap( const vec3 &eye, f32 head_yaw, f32 fake_yaw )
{
    const head_scan_t &scan{ this->m_head_scan };
    const vec3         turned_eye{ spin( eye, scan.m_origin, scan.m_head_yaw - head_yaw ) };

    i32 overlap{ 0 };
    for ( const vec3 &point : scan.m_shown_points )
    {
        const vec3 shown{ spin( point, scan.m_origin, fake_yaw - scan.m_shown_yaw + scan.m_head_yaw - head_yaw ) };
        if ( math::ray_hits_box( turned_eye, shown, scan.m_bones[ scan.m_head->bone ], scan.m_head->bbmin, scan.m_head->bbmax, 0.0f ) )
        {
            ++overlap;
        }
    }

    return overlap;
}

yaw_score_t anti_aim::score_yaws( f32 real_yaw, f32 fake_yaw )
{
    const f32 head_yaw{ this->settled_head_yaw( real_yaw, fake_yaw ) };

    yaw_score_t score{ };
    score.m_real_exposed = this->m_head_scan.m_exposed[ yaw_bin( head_yaw ) ];

    if ( !this->m_using_fake_yaw )
    {
        return score;
    }

    score.m_overlap      = this->count_overlap( head_yaw, fake_yaw );
    score.m_fake_exposed = this->m_head_scan.m_shown_exposed[ yaw_bin( fake_yaw ) ];
    return score;
}

f32 anti_aim::determine_best_real_yaw( f32 fake_yaw )
{
    f32         best_yaw{ this->m_wish_real_yaw };
    yaw_score_t best{ this->score_yaws( best_yaw, fake_yaw ) };

    for ( i32 bin{ 0 }; bin < AUTO_YAW_BINS; ++bin )
    {
        const f32         yaw{ bin_yaw( bin ) };
        const yaw_score_t score{ this->score_yaws( yaw, fake_yaw ) };
        if ( is_better_yaw( score, best ) )
        {
            best     = score;
            best_yaw = yaw;
        }
    }

    return best_yaw;
}

// Kind of a dirty hack for the rage anti-aim.
// It lets the real rage auto yaw make decisions about itself rather than try and fight the fake while doing angle calculations.
// It will work in theory until the auto yaw finds a good yaw that is inverse, then it will bin that yaw and go for one relatively close to it.
// Of course, eventually it'll steer to that point, but when it will can be an indeterminate time...
// FIXME: Allow the yaw calc to be able to determine an angle without having to steer against another angle.
// It could be a bool passed as a parameter or something, since for fake yaw calculations it would still be a good idea to be able to have the fake
// yaw steer against the real to avoid overlaps. Maybe that too could be a toggleable option (at least in regards to the fake).
f32 anti_aim::determine_best_real_yaw( )
{
    return this->determine_best_real_yaw( -this->m_wish_real_yaw );
}

f32 anti_aim::determine_best_fake_yaw( f32 real_yaw )
{
    f32         best_yaw{ this->m_wish_fake_yaw };
    yaw_score_t best{ this->score_yaws( real_yaw, best_yaw ) };

    for ( i32 bin{ 0 }; bin < AUTO_YAW_BINS; ++bin )
    {
        const f32         yaw{ bin_yaw( bin ) };
        const yaw_score_t score{ this->score_yaws( real_yaw, yaw ) };
        if ( is_better_yaw( score, best ) )
        {
            best     = score;
            best_yaw = yaw;
        }
    }

    return best_yaw;
}

inline color_t get_color_for_hitbox( i32 i, bool fake )
{
    if ( fake )
    {
        if ( g_cfg.m_anti_hack.m_fake_draw_mode == config::DRAW_MODE_SOLID )
        {
            return g_cfg.m_anti_hack.m_fake_base_color;
        } else
        {
            return i == HITBOX_HEAD ? g_cfg.m_anti_hack.m_fake_highlight_color : g_cfg.m_anti_hack.m_fake_base_color;
        }
    } else
    {
        if ( g_cfg.m_anti_hack.m_real_draw_mode == config::DRAW_MODE_SOLID )
        {
            return g_cfg.m_anti_hack.m_real_base_color;
        } else
        {
            return i == HITBOX_HEAD ? g_cfg.m_anti_hack.m_real_highlight_color : g_cfg.m_anti_hack.m_real_base_color;
        }
    }
}

inline bool filter_box( i32 i, bool fake )
{
    if ( i != HITBOX_HEAD )
    {
        return fake ? g_cfg.m_anti_hack.m_fake_draw_mode == config::DRAW_MODE_ONLY_HEAD : g_cfg.m_anti_hack.m_real_draw_mode == config::DRAW_MODE_ONLY_HEAD;
    }

    return false;
}

void anti_aim::draw_hitboxes( CStudioHdr *studio, const pose_snapshot_t &snapshot, bool fake )
{
    matrix3x4 bones[ MAX_STUDIO_BONES ]{ };
    if ( !build_local_bones( studio, snapshot, BONE_USED_BY_HITBOX, bones ) )
    {
        return;
    }

    const mstudiohitboxset_t *set{ studio->m_pStudioHdr->pHitboxSet( g_cl.m_local->m_nHitboxSet( ) ) };
    for ( i32 i{ 0 }; i < set->numhitboxes; ++i )
    {
        if ( filter_box( i, fake ) )
        {
            break;
        }

        const mstudiobbox_t *box{ set->pHitbox( i ) };
        g_cl.draw_3d_box( bones[ box->bone ], box->bbmin, box->bbmax, get_color_for_hitbox( i, fake ) );
    }
}
