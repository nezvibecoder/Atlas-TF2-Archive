#include "fakelag.hpp"

#include "anti_aim.hpp"
#include "tickbase.hpp"

ATLAS_NO_DESTROY fakelag g_fakelag{ };

void fakelag::think( )
{
    this->m_profile   = g_cfg.m_anti_hack.m_fl_profiles[ client::get_profile( ) ];
    this->m_is_active = this->should_run( );
    if ( !this->m_is_active )
    {
        return;
    }

    switch ( this->m_profile.m_fakelag_mode )
    {
        case config::FAKELAG_MODE_STATIC:
        {
            this->m_abs_goal_choke = this->m_goal_choke = this->m_profile.m_static_amount;
            break;
        }

        case config::FAKELAG_MODE_RANDOM:
        {
            g_tf2.random_seed( g_tf2.m_MD5_PseudoRandom.call_as< i32 >( g_cl.m_cmd->command_number ) & INT_MAX );
            this->m_abs_goal_choke = this->m_goal_choke = g_tf2.random_int( this->m_profile.m_random_min, this->m_profile.m_random_max );
            break;
        }

        case config::FAKELAG_MODE_ADAPTIVE:
        {
            const auto distance{ g_tf2.sv_lagcompensation_teleport_dist->GetFloat( ) };
            const auto speed{ g_cl.m_local->m_vecVelocity( ).length2d( ) }; // TODO: Is m_vecVelocity or GetAbsVelocity correct for this?
            this->m_goal_choke = ( distance * distance ) / speed;

            // Thanks Rud for the idea.
            // - Hoku
            if ( this->m_profile.m_adaptive_randomized_choke )
            {
                const bool reached_goal{ ( g_tf2.m_cl->chokedcommands + 1 ) >= this->m_goal_choke };
                if ( reached_goal )
                {
                    g_tf2.random_seed( g_tf2.m_MD5_PseudoRandom.call_as< i32 >( g_cl.m_cmd->command_number ) & INT_MAX );
                    const auto rand{ g_tf2.random_int( 0, 4 ) };
                    auto       rand_goal{ this->m_goal_choke + rand };
                    rand_goal = math::clamp( rand_goal, 0, MAX_SHIFT );

                    this->m_abs_goal_choke = rand_goal;
                    break;
                }
            }

            this->m_abs_goal_choke = this->m_goal_choke;

            break;
        }

        default: break;
    }

    if ( g_anti_aim.using_fake_angles( ) )
    {
        this->m_abs_goal_choke = math::clamp( this->m_abs_goal_choke, 3, MAX_SHIFT );
    }

    g_cl.m_send_packet = ( g_tf2.m_cl->chokedcommands + 1 ) >= this->m_abs_goal_choke;
}

bool fakelag::is_active( )
{
    return this->m_is_active;
}

void fakelag::reset( )
{
    this->m_goal_choke     = 0;
    this->m_abs_goal_choke = 0;
    this->m_is_active      = false;
    this->m_profile        = { };
}

bool fakelag::should_run( )
{
    if ( !g_cl.m_local || !g_cl.m_weapon )
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

    if ( !g_cfg.m_anti_hack.m_fakelag_enabled )
    {
        return false;
    }

    if ( !gui::bind_active( g_cfg.m_anti_hack.m_fakelag_key ) )
    {
        return false;
    }

    if ( g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_MINIGUN )
    {
        // Don't choke if we're revving.
        if ( g_cl.m_cmd->buttons & IN_ATTACK2 )
        {
            return false;
        }

        // In the case of attack bits not aligning right, we check the minigun state directly.
        switch ( g_cl.m_weapon->As< C_TFMinigun * >( )->m_iWeaponState( ) )
        {
            case AC_STATE_FIRING:
            case AC_STATE_DRYFIRE:
            case AC_STATE_SPINNING:
            case AC_STATE_STARTFIRING: return false;
            default: break;
        }
    }

    // Cut fakelag off if we aren't fast enough or are slowing down to a stop.
    if ( this->m_profile.m_fakelag_mode == config::FAKELAG_MODE_ADAPTIVE )
    {
        const auto vel{ g_cl.m_local->m_vecVelocity( ).length( ) };
        if ( vel < 64.f )
        {
            return false;
        }
    }

    // The min amount is bigger than the max bounds. This is a config error. Fail out.
    if ( this->m_profile.m_fakelag_mode == config::FAKELAG_MODE_RANDOM )
    {
        if ( this->m_profile.m_random_min > this->m_profile.m_random_max )
        {
            return false;
        }
    }

    // Player is ducking on the ground, they can't move fast enough to break lag compensation effectively.
    if ( g_cl.m_local->IsDucked( ) && g_cl.m_local->IsOnGround( ) )
    {
        return false;
    }

    if ( g_cl.m_local->IsFrozen( ) )
    {
        return false;
    }

    if ( g_tickbase.has_ticks_stored( ) )
    {
        return false;
    }

    return true;
}
