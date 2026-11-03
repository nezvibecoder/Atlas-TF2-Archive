#include "tickbase.hpp"

#include "gui/gui.hpp"
#include "gui/indicator.hpp"
#include "gui/notifications.hpp"
#include "render/render.hpp"
#include "util/format.hpp"

#include "anti_aim.hpp"
#include "holdaim_mgr.hpp"
#include "lagcomp.hpp"
#include "networking.hpp"
#include "prediction.hpp"
#include "ragebot.hpp"
#include "resolver.hpp"

ATLAS_NO_DESTROY tick_manager g_tickbase{ };

void tick_manager::on_pre_prediction( )
{
    Assert( g_cl.m_local != nullptr );
    Assert( g_cl.m_weapon != nullptr );

    this->m_old_next_attack           = g_cl.m_local->m_flNextAttack( );
    this->m_old_next_primary_attack   = g_cl.m_weapon->m_flNextPrimaryAttack( );
    this->m_old_next_secondary_attack = g_cl.m_weapon->m_flNextSecondaryAttack( );
    if ( g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_MINIGUN )
    {
        this->m_old_minigun_state = g_cl.m_weapon->As< C_TFMinigun * >( )->m_iWeaponState( );
    }

    if ( this->m_shift_type != shift_type_t::rapidfire )
    {
        return;
    }

    // every extra holds the trigger and nothing else; shift_amount planned a shot on each one whose cooldown ran out,
    // and all fire on the first shot's angles, which on_prediction weighs before arming; A reload would eat a planned
    // shot, and on the minigun an extra without the trigger idles it into WindDown once m_flTimeWeaponIdle runs out,
    // while one with the secondary button too flips and shoots in the same tick, see spin_shot..
    // An extra the schedule withholds holds nothing, so the shot due on it waits, see rapid_fire_plan_t::keeps_firing.
    g_cl.m_cmd->buttons = ( g_cl.m_cmd->buttons | IN_ATTACK ) & ~( IN_ATTACK2 | IN_RELOAD );
    if ( this->withholds_shot( ) )
    {
        g_cl.m_cmd->buttons &= ~IN_ATTACK;
    }

    if ( g_cfg.m_exploits.m_anti_warp )
    {
        this->anti_warp( );
    }
}

// this was is_blacklisted_weapon; ragebot.cpp has its own inline one by that name with a
// different list, and two inline bodies under one name are one function to the linker!
ATLAS_FORCE_INLINE bool is_blacklisted_shift_weapon( C_TFWeaponBase *wep )
{
    switch ( wep->m_iItemDefinitionIndex( ) )
    {
        case Soldier_m_RocketJumper:
        case Demoman_s_StickyJumper: return true;
        default: break;
    }

    switch ( wep->GetWeaponID( ) )
    {
        case TF_WEAPON_MEDIGUN:
        case TF_WEAPON_GRAPPLINGHOOK:
        case TF_WEAPON_LASER_POINTER:
        case TF_WEAPON_PDA:
        case TF_WEAPON_PDA_SPY:
        case TF_WEAPON_PDA_SPY_BUILD:
        case TF_WEAPON_PDA_ENGINEER_BUILD:
        case TF_WEAPON_PDA_ENGINEER_DESTROY:
        case TF_WEAPON_SNIPERRIFLE:
        case TF_WEAPON_SNIPERRIFLE_CLASSIC:
        case TF_WEAPON_SNIPERRIFLE_DECAP:
        case TF_WEAPON_CLEAVER:
        case TF_WEAPON_JAR:
        case TF_WEAPON_JAR_GAS:
        case TF_WEAPON_JAR_MILK:
        case TF_WEAPON_PASSTIME_GUN: return true;
        case TF_WEAPON_PIPEBOMBLAUNCHER:
        case TF_WEAPON_CANNON:
        case TF_WEAPON_COMPOUND_BOW: return true; // these fire on release, holding the trigger only charges them
        default: break;
    }

    if ( g_cl.attrib_hook( wep, 0.0f, ATLAS_XORSTR( "auto_fires_full_clip" ).get( ) ) != 0.0f )
    {
        return true;
    }

    return false;
}

// the extra the minigun's first shot goes off on when the shot command spun it, counted from the shot command's tick;
// Spinning out of FIRING puts both attack timers 0.1 out, the first command that reaches them only flips it back to
// FIRING and the one after shoots; The SDK adds a double 0.1 to the float curtime and keeps the float, so we do too.
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_weapon_minigun.cpp#L308-L322
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_weapon_minigun.cpp#L413-L433
ATLAS_FORCE_INLINE i32 spin_shot( i32 tick )
{
    const auto spun{ static_cast< f32 >( TICKS_TO_TIME( tick ) + 0.1 ) };

    i32 extra{ 1 };
    while ( TICKS_TO_TIME( tick + extra ) < spun )
    {
        ++extra;
    }

    return extra + 1;
}

ATLAS_FORCE_INLINE i32 first_shot( shot_command_t command, i32 shot )
{
    switch ( command )
    {
        case shot_command_t::flips: return shot + 1;
        case shot_command_t::spins: return shot + spin_shot( shot );
        default: return shot;
    }
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_weaponbase_gun.cpp#L165-L169
ATLAS_FORCE_INLINE f32 fire_delay( C_TFWeaponBase *wep )
{
    const f32 delay{ g_prediction.m_weapon_fire_delay };
    if ( wep->m_iClip1( ) != -1 )
    {
        return delay;
    }

    return g_cl.attrib_hook( wep, delay, ATLAS_XORSTR( "fast_reload" ).get( ) );
}

ATLAS_FORCE_INLINE i32 count_shots( i32 first, i32 last, f32 delay, i32 rounds, i32 *ticks = nullptr )
{
    i32 shots{ 0 };
    i32 tick{ first };
    while ( tick <= last && shots < rounds )
    {
        if ( ticks )
        {
            ticks[ shots ] = tick;
        }

        ++shots;
        const auto next_attack{ TICKS_TO_TIME( tick ) + delay };
        ++tick;
        while ( TICKS_TO_TIME( tick ) < next_attack )
        {
            ++tick;
        }
    }

    return shots;
}

ATLAS_FORCE_INLINE f32 shot_delay( C_TFWeaponBase *wep, shot_command_t command )
{
    const f32 delay{ wep->m_flNextPrimaryAttack( ) - g_tf2.m_globals->curtime };
    if ( command == shot_command_t::flips || delay <= 0.0f )
    {
        return fire_delay( wep );
    }

    return delay;
}

ATLAS_FORCE_INLINE i32 shot_rounds( C_TFWeaponBase *wep )
{
    const i32 ammo_type{ wep->m_iPrimaryAmmoType( ) };
    return wep->m_iClip1( ) > 0 ? wep->m_iClip1( ) + 1
           : ammo_type >= 0     ? ( &g_cl.m_local->m_iAmmo( ) )[ ammo_type ]
                                : INT_MAX;
}

i32 tick_manager::shift_amount( C_TFWeaponBase *wep, shot_command_t command ) const
{
    Assert( wep != nullptr && "You're calling shift_amount with a nullptr. You have a code bug." );
#if defined( ATLAS_DEV )
    if ( is_blacklisted_shift_weapon( wep ) )
    {
        Assert( 0 && "Calling shift_amount with a blacklisted weapon. You have a code bug." );
        return 0;
    }
#endif // ATLAS_DEV

    // the shot already took its round in prediction, so an empty clip can't make a second one;
    if ( wep->m_iClip1( ) == 0 )
    {
        return this->packet_room( );
    }

    const f32 delay{ shot_delay( wep, command ) };
    if ( delay <= 0.0f )
    {
        return 0;
    }

    const i32 rounds{ shot_rounds( wep ) };

    // Every later shot the server runs in the same tick takes the first shot's angles (sv_maxusrcmdprocessticks_holdaim,
    // restored in CPlayerMove::RunCommand) and its lag compensation record from
    // wherever the extras moved us, which only lands while the anti-warp holds us next to the first shot; The first
    // command after the packet is aimed by the ragebot on its own instead; It runs at the packet's last tickbase + 1,
    // its estimate stays inside the correction window so the server doesn't pull it back.
    //
    // Both times are float math on the tickbase the server pulls the packet back to, and that tick moves with the packet
    // length, so we test every length the way the server will instead of rounding the delay, the shortest one with the
    // first shot alone inside and the second on that command is what a DT needs at least
    const auto room{ this->packet_room( ) };
    i32        needed{ 0 };
    i32        second_inside{ 0 };
    for ( i32 extra{ 1 }; extra <= room; ++extra )
    {
        const auto shot{ this->shot_tick( extra ) };
        const auto first{ first_shot( command, shot ) };
        const auto inside{ count_shots( first, shot + extra, delay, rounds ) };

        // the correction pulls a longer packet a tick earlier too, so with a fire delay of a whole number of ticks the
        // second shot lands inside at every length!
        if ( inside > 1 )
        {
            second_inside = extra;
            break;
        }

        if ( inside == 1 && count_shots( first, shot + extra + 1, delay, rounds ) > 1 )
        {
            needed = extra;
            break;
        }
    }

    // a minigun that can't fit a second shot after its spin up only pushes its first one later..
    if ( needed == 0 && second_inside == 0 )
    {
        return command == shot_command_t::spins ? 0 : room;
    }

    // airbone, fuck
    const bool held_still{ g_cfg.m_exploits.m_anti_warp && g_cl.m_local->IsOnGround( ) };
    if ( needed == 0 && !held_still )
    {
        return 0;
    }

    const i32 least{ needed != 0 ? needed : second_inside };

    // We wanna give it everything we got for rapidfire weapons.
    const CTFWeaponInfo *info{ wep->GetWeaponInfo( ) };
    Assert( info != nullptr );

    const bool rapid_fire{ info->GetWeaponData( math::clamp( wep->m_iWeaponMode( ), 0, 1 ) ).m_bUseRapidFireCrits };
    if ( !rapid_fire || !held_still || this->shift_room( ) < least )
    {
        return least;
    }

    return this->shift_room( );
}

// commands one packet still carries after the held ones and the regular one, bounded like the flush in on_cl_move..
i32 tick_manager::packet_room( ) const
{
    return math::smaller( MAX_SHIFT, this->m_max_shift ) - 1 - g_tf2.m_cl->chokedcommands;
}

// the server only runs the ticks we banked; m_credits still holds this command's own grant, which it spends itself..
i32 tick_manager::shift_room( ) const
{
    return math::smaller( this->m_credits - 1, this->packet_room( ) );
}

// partial rebuild of CBasePlayer::AdjustPlayerTimeBase for the packet a shift sends;
// the held commands, the shot and the extras all run in one server tick, so the server pulls the packet back until its
// last command lands where a lone command would have; It corrects the tickbase the head had before any fix of ours..
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/player.cpp#L3152-L3173
i32 tick_manager::shot_tick( i32 extra ) const
{
    const i32  held{ g_tf2.m_cl->chokedcommands };
    tick_fix_t packet{ this->m_fixes[ static_cast< usize >( g_tf2.m_cl->lastoutgoingcommand + 1 ) % MULTIPLAYER_BACKUP ] };
    packet.m_simulation_ticks = held + 1 + extra;

    return packet.adjusted_tick( packet.m_raw_tickbase ) + held;
}

i32 tick_manager::update_rapid_fire_state( )
{
    // Either we have a bad weapon or the charger is still doing work.
    if ( is_blacklisted_shift_weapon( g_cl.m_weapon ) || this->m_charge_state != charge_state_t::idle )
    {
        this->m_rapid_state = rapid_fire_state_t::unable;
        return 0;
    }

    // we can't tell where the server puts the packet until we know which server tick it lands in
    const i32 first{ g_tf2.m_cl->lastoutgoingcommand + 1 };
    if ( this->m_fixes[ static_cast< usize >( first ) % MULTIPLAYER_BACKUP ].m_command_number != first )
    {
        this->m_rapid_state = rapid_fire_state_t::waiting;
        return 0;
    }

    // https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/basecombatweapon_shared.cpp#L1781-L1785
    // the flip doesn't shoot, so will_fire doesn't speak for the extra that does and the ammo is checked here
    if ( g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_MINIGUN && this->m_old_minigun_state == AC_STATE_SPINNING &&
         ( g_cl.m_cmd->buttons & IN_ATTACK ) && g_cl.m_weapon->HasPrimaryAmmoForShot( g_cl.m_local ) )
    {
        return this->update_minigun_state( shot_command_t::flips, this->m_old_next_primary_attack );
    }

    // We don't have enough ticks in the bucket to shift.
    // this compared m_credits against the amount and asserted the amount was never 0; m_credits
    // still holds this command's own grant, which the shot spends, and shift_amount can return 0 when no length gives a
    // second shot, so both read as not enough
    const auto charge_amount{ this->shift_amount( g_cl.m_weapon, shot_command_t::fires ) };
    if ( charge_amount <= 0 || charge_amount > this->shift_room( ) )
    {
        this->m_rapid_state = rapid_fire_state_t::not_enough;
        return 0;
    }

    // The tick the server runs the shot on, see the notes on the weapon timers below.
    const auto shot_time{ TICKS_TO_TIME( this->shot_tick( charge_amount ) ) };

    // Local player state is invalid for an attack to happen.
    // this compared against curtime before; The player's m_flNextAttack is checked on the same
    // pulled back tick as the weapon timers, so it takes shot_time too.
    if ( this->m_old_next_attack > shot_time )
    {
        this->m_rapid_state = rapid_fire_state_t::waiting;
        return 0;
    }

    // Weapon timers are not sufficient for a shift.
    // NOTE: Hoku; We add on the weapon fire delay time onto the next attack amount as when a shift for 1 critical reason:
    // When you shift your tickbase forward, AdjustPlayerTimeBase on the server rolls your clock back by the amount of commands you sent to the server.
    // RunCommand and PlayerRunCommand curtime uses the player's tickbase converted into a floating point time, and is used for comparison in
    // the player/weapon PostThinks for weapon timer checks.
    // So, for example: If your tickbase is at tick 1000, and you shift 21 ticks, your tickbase increments to 1021.
    // Then, the server runs AdjustPlayerTimeBase to correct your clock, which rolls your tickbase back by 20 to 1001.
    // If you do not buffer the additional time on the wait check, you will shift back into a reload state or the previous bullet fired.
    // Even if you predict reloads correctly in the ItemPostFrame functions, this issue will always arise.
    //
    // NOTE: NvK; This code before added the fire delay onto the next attack, which is exactly the server's
    // rollback while the shift is the fire delay in ticks, and that's what shift_amount used to return; The shift doesn't
    // follow the fire delay anymore (see shift_amount), so the check takes the tick shot_tick says the server runs the
    // shot on; Same idea, with the exact rollback instead of the fire delay o.O
    if ( this->m_old_next_primary_attack > shot_time )
    {
        if ( g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_MINIGUN )
        {
            return this->update_minigun_state( shot_command_t::spins, this->m_old_next_secondary_attack );
        }

        this->m_rapid_state = rapid_fire_state_t::waiting;
        return 0;
    }

    this->m_shot_command = shot_command_t::fires;
    this->m_rapid_state  = rapid_fire_state_t::ready;
    return charge_amount;
}

// you're one special fucker ain't you?
i32 tick_manager::update_minigun_state( shot_command_t command, f32 timer )
{
    const auto charge_amount{ this->shift_amount( g_cl.m_weapon, command ) };
    if ( charge_amount <= 0 || charge_amount > this->shift_room( ) )
    {
        this->m_rapid_state = rapid_fire_state_t::not_enough;
        return 0;
    }

    const auto shot_time{ TICKS_TO_TIME( this->shot_tick( charge_amount ) ) };
    if ( this->m_old_next_attack > shot_time || timer > shot_time )
    {
        this->m_rapid_state = rapid_fire_state_t::waiting;
        return 0;
    }

    this->m_shot_command = command;
    this->m_rapid_state  = rapid_fire_state_t::ready;
    return charge_amount;
}

void tick_manager::on_prediction( )
{
    // Rapid fire takes prescendence over warp.
    if ( !g_cfg.m_exploits.m_rapid_fire )
    {
        return;
    }

    // The extras of a shift come through here too, they must not plan another one.
    // this guard used to sit at the top of update_rapid_fire_state, where it never fired because
    // nothing set m_shift_type; on_prediction sets it now, and this is where an extra would arm a second shift.
    if ( this->shifting( ) )
    {
        return;
    }

    const auto charge_amount{ this->update_rapid_fire_state( ) };
    const bool fires{ g_cl.m_will_fire || this->m_shot_command == shot_command_t::flips };
    if ( fires && this->m_rapid_state == rapid_fire_state_t::ready )
    {
        if ( gui::bind_active( g_cfg.m_exploits.m_rapid_fire_key ) )
        {
            if ( !g_cl.m_final_tick )
            {
                this->m_banks_tick = true;
                g_cl.m_will_fire   = false;
                g_holdaim_mgr.reset( );
                return;
            }

            const rapid_fire_plan_t plan{ this->plan_rapid_fire( charge_amount ) };
            u32                     withheld{ };
            if ( g_ragebot.schedule_rapid_fire( plan, withheld ) )
            {
                this->m_shift_type         = shift_type_t::rapidfire;
                this->m_credits_to_consume = charge_amount;
                this->m_withheld           = 0;
                for ( i32 extra{ 1 }; extra <= charge_amount; ++extra )
                {
                    if ( withheld >> extra & 1u )
                    {
                        this->m_withheld |= 1u << ( charge_amount - extra );
                    }
                }

                if ( this->m_shot_command == shot_command_t::fires )
                {
                    return;
                }

                // https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_weapon_minigun.cpp#L242-L249
                if ( this->m_shot_command == shot_command_t::flips )
                {
                    g_cl.m_cmd->buttons &= ~IN_ATTACK2;
                } else
                {
                    g_cl.m_cmd->buttons = ( g_cl.m_cmd->buttons & ~IN_ATTACK ) | IN_ATTACK2;
                }

                g_holdaim_mgr.reset( );
                return;
            }

            g_cl.m_cmd->buttons &= ~IN_ATTACK;
            if ( g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_MINIGUN )
            {
                g_cl.m_cmd->buttons |= IN_ATTACK2;
            }

            g_cl.m_will_fire = false;
            g_holdaim_mgr.reset( );
        }
    }

    // No rapid fire request, lets see about warp...
    if ( g_cfg.m_exploits.m_warp && this->m_charge_state == charge_state_t::idle && g_cl.m_final_tick )
    {
        if ( gui::bind_active( g_cfg.m_exploits.m_warp_key ) )
        {
            const auto warp_ticks{ math::smaller( g_cfg.m_exploits.m_warp_ticks, this->shift_room( ) ) };
            if ( warp_ticks > 0 )
            {
                this->m_shift_type         = shift_type_t::warp;
                this->m_credits_to_consume = warp_ticks;
            }

            return;
        }
    }

    if ( g_cfg.m_exploits.m_speedhack && this->m_charge_state == charge_state_t::idle && g_cl.m_final_tick )
    {
        if ( gui::bind_active( g_cfg.m_exploits.m_speedhack_key ) )
        {
            const auto speedhack_ticks{ math::smaller( g_cfg.m_exploits.m_speedhack_factor, MAX_SHIFT ) };
            if ( speedhack_ticks > 0 )
            {
                this->m_shift_type         = shift_type_t::speedhack;
                this->m_credits_to_consume = speedhack_ticks;
            }
        }
    }
}

void tick_manager::flush( bool final_tick )
{
    if ( final_tick && g_tf2.m_cl->chokedcommands > 0 )
    {
        g_networking.cl_move( 0.0f, true, false );
    }
}

bool tick_manager::on_cl_move( float accumulated_extra_samples, bool final_tick )
{
    // _Host_RunFrame calls CL_Move once per tick of the frame and only the last one sends
    this->m_frame_tick  = this->m_frame_final ? 0 : this->m_frame_tick + 1;
    this->m_frame_final = final_tick;

    ++this->m_send_tick;

    // sanity check
    if ( !g_tf2.m_engine->IsInGame( ) || !g_cl.m_local )
    {
        return false;
    }

    this->m_send_clock = this->m_last_send_tick < 0 ? this->clock_now( )
                                                    : this->m_last_send_clock + ( this->m_send_tick - this->m_last_send_tick );

    if ( !g_cl.m_local->IsAlive( ) )
    {
        return false;
    }

    // NOTE: Hoku; Was bound checking against MAX_SHIFT previously.
    // Unfortunately, while it is true that a single move message is bound by MAX_NEW_COMMANDS + MAX_BACKUP_COMMANDS
    // the server will still allow us to hold back up to the processtick limit.
    // We might as well keep the additional ticks, maybe someone who's warping can make use of them.
    // https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/player.cpp#L3395

    // Hoku 10/21/26; Should prevent some breakage with processticks being set to 0 in a listen server or if processticks changes
    // to a var that would eat into our store.
    const auto process_ticks{ g_tf2.sv_maxusrcmdprocessticks->GetInt( ) };
    this->m_max_shift = process_ticks <= 0 ? 24 : process_ticks;
    if ( this->m_credits > this->m_max_shift )
    {
        this->m_credits = this->m_max_shift;
    }

    this->m_shown_max_shift = this->true_max_shift( );

    if ( g_tf2.m_cl->IsPaused( ) )
    {
        this->clear( );
        return false;
    }

    if ( !g_tf2.m_Host_ShouldRun_fn.call_as< bool >( ) )
    {
        return true;
    }

    g_input.suppress_binds( g_cl.game_owns_keyboard( ) );
    g_input.begin_command( );
    this->grant_credit( );
    g_networking.wait_for_control( final_tick );

    // a frame with more than one tick runs below the tickrate; Only its last tick sends, so a shot on the others has to
    // wait for it (see on_prediction) and the extras all land in one frame, which is why it gets unstable there..
    if ( !final_tick && !this->m_warned_low_fps )
    {
        this->m_warned_low_fps = true;
        // push_notification( notify_kind_t::warning, ICON_ZAP, ATLAS_XORSTR_STATIC( "Below the tickrate" ),
        //                    ATLAS_XORSTR_STATIC( "Under 66 fps exploits get unstable." ) );
    }

    if ( this->recharge( ) )
    {
        if ( final_tick )
        {
            g_networking.keep_alive( 1 );
        }

        return true;
    }

    if ( this->m_charge_state == charge_state_t::synchronizing &&                               // waiting on a fence
         this->m_charge_fence > g_tf2.m_cl->lastoutgoingcommand &&                              // that wasn't sent
         this->m_charge_fence <= g_tf2.m_cl->lastoutgoingcommand + g_tf2.m_cl->chokedcommands ) // but was made, a recharge's isn't yet
    {
        this->flush( final_tick );
        g_prediction.update( );
        return true;
    }

    // TODO: Probably should remove the chokedcommands check here.
    // We already run a safety check within CreateMove to prevent us from blowing the CLC_Move command limit.
    // So why have this here?
    if ( !g_networking.can_run_move( ) || g_tf2.m_cl->chokedcommands >= math::smaller( MAX_SHIFT, this->m_max_shift ) )
    {
        this->flush( final_tick );
        g_prediction.update( );
        return true;
    }

    // the frame's later ticks hold theirs as well; Each of them the server spends above its cap is one more copy the
    // batch's head runs after, known before the head is first predicted..
    this->m_frame_copies = 0;
    if ( !final_tick )
    {
        this->drop_overflow( );

        const i32 held_after{ *g_tf2.m_host_frameticks.get_as< const i32 * >( ) - this->m_frame_tick - 2 };
        this->m_frame_copies = math::bigger( this->m_credits + g_tf2.m_cl->chokedcommands + held_after - this->m_max_shift, 0 );
    }

    if ( this->record_fix( g_tf2.m_cl->lastoutgoingcommand + 1, g_tf2.m_cl->chokedcommands + 1 ) &&
         g_tf2.m_cl->chokedcommands > 0 )
    {
        g_tf2.m_prediction->m_nCommandsPredicted = 0;
    }

    // every command the server runs takes one tick out of the budget, a tick without one stays in it
    // https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/player_command.cpp#L318
    this->m_banks_tick = false;
    if ( !g_networking.cl_move( accumulated_extra_samples, final_tick ).m_generated )
    {
        return true;
    }

    --this->m_credits;

    // the command we just made asked for a shift while it was predicted, and it's still held for it..
    // NOTE: NvK; The shift used to run right here: it flushed the held commands, then made the extras with
    // simTicksThisFrame set to the shift length; The shot left in one move and the extras in another, and the engine's
    // CGameClient::ProcessMove ignores a second move in the same server tick, while one that lands a tick later gets
    // pulled back behind the shot's next attack; simTicksThisFrame only reached record_fix's frame_ticks on loopback,
    // where it predicted no rollback while the server rolled back; See shift( ).
    if ( this->shifting( ) )
    {
        this->shift( );
    }

    // a command we hold leaves the server a tick without one, which the next grant would only find after the command's
    // first prediction; Its copy has to be on the batch before that!
    if ( g_tf2.m_cl->chokedcommands > 0 && this->drop_overflow( ) &&
         this->record_fix( g_tf2.m_cl->lastoutgoingcommand + 1, g_tf2.m_cl->chokedcommands ) )
    {
        g_tf2.m_prediction->m_nCommandsPredicted = 0;
    }

    return true;
}

// the held commands, the one that asked for the shift and every extra have to leave in one CLC_Move; The engine's
// CGameClient::ProcessMove ignores a second move in the same server tick, and a move that lands a tick later gets
// pulled back by @::AdjustPlayerTimeBase behind everything the first one did..
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/player.cpp#L3152-L3173
void tick_manager::shift( )
{
    const i32 extra{ this->m_credits_to_consume };
    this->record_fix( g_tf2.m_cl->lastoutgoingcommand + 1, g_tf2.m_cl->chokedcommands + extra );
    g_tf2.m_prediction->m_nCommandsPredicted = 0;
    g_prediction.update( );

    this->m_anti_warp = { };

    i32 made{ 0 };
    while ( made < extra )
    {
        this->m_credits_to_consume = extra - made - 1;
        if ( !g_networking.cl_move( 0.0f, this->m_credits_to_consume == 0 ).m_generated )
        {
            break;
        }

        --this->m_credits;
        ++made;

        if ( this->m_shift_type == shift_type_t::speedhack )
        {
            if ( this->m_credits < 0 )
            {
                this->m_credits = 0;
            }
        }
    }

    if ( this->m_shift_type == shift_type_t::rapidfire )
    {
        this->m_charge_fence = g_tf2.m_cl->lastoutgoingcommand + g_tf2.m_cl->chokedcommands;
        this->m_charge_state = charge_state_t::synchronizing;
    }

    this->m_credits_to_consume = 0;
    this->m_shift_type         = shift_type_t::none;
    this->m_withheld           = 0;
    DBGOUT( "tick_manager::shift: Made %i of %i extras, waiting on command %i.", made, extra, this->m_charge_fence );
}

struct ground_move_t
{
    f32 m_friction{ };
    f32 m_stopspeed{ };
    f32 m_accelerate{ };
    f32 m_surface{ };
    f32 m_maxspeed{ };
    f32 m_wish_scale{ };
    f32 m_back_cap{ };
    f32 m_back_min{ };
    f32 m_axis_forward{ };
};

struct batch_end_t
{
    f32 m_travel{ };
    f32 m_speed{ };
};

ATLAS_FORCE_INLINE f32 ground_step( f32 &w, f32 wish, const ground_move_t &ground )
{
    f32 speed{ math::abs( w ) };
    if ( speed >= 0.1f )
    {
        const f32 control{ math::bigger( speed, ground.m_stopspeed ) };
        const f32 remaining{ math::bigger( speed - control * ground.m_friction * TICK_INTERVAL, 0.0f ) };

        w *= remaining / speed;
        speed = remaining;
    }

    const f32 wishspeed{ math::smaller( math::abs( wish ), ground.m_maxspeed ) * ground.m_wish_scale };
    if ( wishspeed > 0.0f )
    {
        const f32 direction{ wish > 0.0f ? 1.0f : -1.0f };
        f32       accelerate{ ground.m_accelerate };

        if ( wishspeed < 100.0f * ground.m_friction / ground.m_accelerate )
        {
            accelerate = math::bigger( speed, ground.m_stopspeed ) * ground.m_friction / wishspeed + 1.0f;
        }

        const f32 addspeed{ wishspeed - w * direction };
        if ( addspeed > 0.0f )
        {
            w += math::smaller( accelerate * TICK_INTERVAL * wishspeed * ground.m_surface, addspeed ) * direction;
        }
    }

    w = math::clamp( w, -ground.m_maxspeed, ground.m_maxspeed );

    const f32 behind{ -w * ground.m_axis_forward };
    if ( ground.m_back_cap > 0.0f && math::abs( w ) > ground.m_back_min && behind > ground.m_back_cap )
    {
        w = -ground.m_back_cap * ground.m_axis_forward + w * ( 1.0f - ground.m_axis_forward * ground.m_axis_forward );
    }

    return w * TICK_INTERVAL;
}

ATLAS_FORCE_INLINE batch_end_t batch_end( f32 w, f32 travel, f32 wish, i32 remaining, f32 return_wish,
                                          const ground_move_t &ground )
{
    travel += ground_step( w, wish, ground );

    for ( i32 i{ }; i < remaining; ++i )
    {
        travel += ground_step( w, return_wish, ground );
    }

    return { travel, w };
}

template< typename predicate_t >
ATLAS_FORCE_INLINE f32 bisect_wish( f32 low, f32 high, predicate_t too_low )
{
    for ( i32 i{ }; i < 16; ++i )
    {
        const f32 mid{ ( low + high ) * 0.5f };

        ( too_low( mid ) ? low : high ) = mid;
    }

    return high;
}

ATLAS_FORCE_INLINE f32 anti_warp_wish( const anti_warp_t &state, f32 w, i32 remaining, const ground_move_t &ground )
{
    const f32  target{ state.m_entry * TICK_INTERVAL };
    const f32  return_wish{ math::smaller( state.m_entry / ground.m_wish_scale, ground.m_maxspeed ) };
    const auto simulate{ [ & ]( f32 wish )
    {
        return batch_end( w, state.m_travel, wish, remaining, return_wish, ground );
    } };

    f32         floor{ -ground.m_maxspeed };
    batch_end_t braked{ simulate( floor ) };
    if ( braked.m_speed < 0.0f )
    {
        floor  = bisect_wish( floor, return_wish, [ & ]( f32 wish )
        { return simulate( wish ).m_speed < 0.0f; } );
        braked = simulate( floor );
    }

    if ( braked.m_travel >= target )
    {
        return floor;
    }

    if ( simulate( return_wish ).m_travel < target )
    {
        return return_wish;
    }

    return bisect_wish( floor, return_wish, [ & ]( f32 wish )
    { return simulate( wish ).m_travel < target; } );
}

ATLAS_FORCE_INLINE ground_move_t local_ground( const vec3 &axis )
{
    const vec3 *held{ g_holdaim_mgr.held( ) };
    vec3        facing{ };
    math::angle_vectors( vec3{ 0.0f, held ? held->y : g_cl.m_cmd->viewangles.y, 0.0f }, &facing );

    const f32 maxspeed{ g_cl.m_local->m_flMaxspeed( ) };
    const f32 surface{ g_cl.m_local->m_surfaceFriction( ) };
    const f32 back_fraction{ g_tf2.tf_clamp_back_speed->GetFloat( ) };
    return ground_move_t{ g_tf2.sv_friction->GetFloat( ) * surface,
                          g_tf2.sv_stopspeed->GetFloat( ),
                          g_tf2.sv_accelerate->GetFloat( ),
                          surface,
                          maxspeed,
                          g_cl.m_local->IsDucked( ) ? 1.0f / 3.0f : 1.0f,
                          back_fraction < 1.0f ? maxspeed * back_fraction : 0.0f,
                          g_tf2.tf_clamp_back_speed_min->GetFloat( ),
                          axis.dot( facing ) };
}

void tick_manager::anti_warp( )
{
    const f32 maxspeed{ g_cl.m_local->m_flMaxspeed( ) };
    if ( !g_cl.m_local->IsOnGround( ) || maxspeed <= 0.0f )
    {
        return;
    }

    anti_warp_t &state{ this->m_anti_warp };
    const vec3   velocity{ g_cl.m_local->m_vecVelocity( ) };
    if ( !state.m_anchored )
    {
        state.m_anchored = true;
        state.m_entry    = velocity.length2d( );
        state.m_axis     = state.m_entry > 1.0f ? vec3{ velocity.x, velocity.y, 0.0f } / state.m_entry : vec3{ };
    } else
    {
        state.m_travel += velocity.dot( state.m_axis ) * TICK_INTERVAL;
    }

    vec3 forward{ };
    vec3 right{ };
    math::angle_vectors( vec3{ 0.0f, g_cl.m_cmd->viewangles.y, 0.0f }, &forward, &right );

    if ( state.m_entry <= 1.0f )
    {
        g_cl.m_cmd->forwardmove = 0.0f;
        g_cl.m_cmd->sidemove    = 0.0f;
        return;
    }

    const ground_move_t ground{ local_ground( state.m_axis ) };
    const f32           push{ anti_warp_wish( state, velocity.dot( state.m_axis ), this->m_credits_to_consume, ground ) };
    const vec3          wish{ state.m_axis * push };
    g_cl.m_cmd->forwardmove = wish.dot( forward );
    g_cl.m_cmd->sidemove    = wish.dot( right );
}

i32 tick_manager::true_max_shift( ) const
{
    return g_anti_aim.using_fake_angles( ) ? this->m_max_shift - 3 : this->m_max_shift;
}

rapid_fire_plan_t tick_manager::plan_rapid_fire( i32 extras ) const
{
    rapid_fire_plan_t plan{ };

    const auto shot{ this->shot_tick( extras ) };
    i32        ticks[ MAX_SHIFT ]{ };
    plan.m_tick   = shot;
    plan.m_extras = extras;
    plan.m_delay  = shot_delay( g_cl.m_weapon, this->m_shot_command );
    plan.m_shots  = count_shots( first_shot( this->m_shot_command, shot ), shot + extras, plan.m_delay, shot_rounds( g_cl.m_weapon ), ticks );
    for ( i32 index{ }; index < plan.m_shots; ++index )
    {
        plan.m_extra[ index ] = ticks[ index ] - shot;
    }

    plan.m_slides = g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_MINIGUN;

    const vec3 velocity{ g_cl.m_local->m_vecVelocity( ) };
    const f32  entry{ velocity.length2d( ) };
    if ( entry <= 1.0f )
    {
        return plan;
    }

    plan.m_axis = vec3{ velocity.x, velocity.y, 0.0f } / entry;
    plan.m_step = velocity * TICK_INTERVAL;

    const bool          steered{ g_cfg.m_exploits.m_anti_warp && g_cl.m_local->IsOnGround( ) && g_cl.m_local->m_flMaxspeed( ) > 0.0f };
    const ground_move_t ground{ local_ground( plan.m_axis ) };
    anti_warp_t         state{ plan.m_axis, entry, 0.0f, true };
    f32                 speed{ entry };
    for ( i32 extra{ 1 }; extra <= extras; ++extra )
    {
        if ( steered )
        {
            const f32 wish{ anti_warp_wish( state, speed, extras - extra, ground ) };
            state.m_travel += ground_step( speed, wish, ground );
        } else
        {
            state.m_travel += entry * TICK_INTERVAL;
        }

        plan.m_path[ extra ] = state.m_travel;
    }

    return plan;
}

// Partial rebuild of CBasePlayer::PhysicsSimulate;
// The server grants us one tick of budget every tick before it counts what it can run, and after running our commands
// it never lets more than sv_maxusrcmdprocessticks carry over to the next tick; Commands we're holding were already
// paid for when we made them, but the server hasn't run them yet, so they still sit in its budget.
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/player.cpp#L3243
// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/player.cpp#L3392
void tick_manager::grant_credit( )
{
    this->drop_overflow( );
    ++this->m_credits;
}

// what the server can't carry over isn't simply lost: every tick it ends above the cap runs a copy of our last command,
// tickbase and all, before the commands we're holding; Prediction runs them ahead of the batch's head;
bool tick_manager::drop_overflow( )
{
    const i32 kept{ math::smaller( this->m_credits, this->m_max_shift - g_tf2.m_cl->chokedcommands ) };
    if ( kept == this->m_credits )
    {
        return false;
    }

    this->m_pending_copies += this->m_credits - kept;
    this->m_credits = kept;
    return true;
}

bool tick_manager::should_charge( )
{
    if ( !g_cfg.m_exploits.m_rapid_fire )
    {
        return false;
    }

    if ( g_cfg.m_exploits.m_passive_recharge )
    {
        // The timer isn't on its way there yet, if the user wants an immediate charge they can request it.
        if ( this->m_passive_timer > g_tf2.m_globals->curtime )
        {
            if ( !gui::bind_active( g_cfg.m_exploits.m_recharge_key ) )
            {
                return false;
            }
        }
    } else
    {
        if ( !gui::bind_active( g_cfg.m_exploits.m_recharge_key ) )
        {
            return false;
        }
    }

    // Update the passive timer and initiate 1 charge tick.
    this->m_passive_timer = g_tf2.m_globals->curtime + 0.6f;

    return this->m_credits <= this->true_max_shift( );
}

i32 tick_manager::bank_cap( ) const
{
    return math::smaller( MAX_SHIFT - 1, this->m_max_shift );
}

bool tick_manager::recharge( )
{
    if ( this->m_charge_state == charge_state_t::synchronizing )
    {
        return false;
    }

    // a charge starts on a packet boundary; A flush here would send the held commands early, and with a fake yaw the
    // last one made was a choked one with the real yaw. The batch sends on its own within its choke..
    if ( this->m_charge_state == charge_state_t::idle && g_tf2.m_cl->chokedcommands > 0 )
    {
        return false;
    }

    if ( !this->should_charge( ) )
    {
        if ( this->m_charge_state == charge_state_t::charging )
        {
            // the next command is the first one the server runs after the charge, and AdjustPlayerTimeBase pulls our
            // tickbase forward on it, nothing gets shifted until the server has acknowledged it!
            this->m_charge_fence = g_networking.sequence_number( );
            this->m_charge_state = charge_state_t::synchronizing;
            DBGOUT( "tick_manager::recharge: Banked %i ticks, waiting on command %i.", this->m_credits - 1, this->m_charge_fence );
        }

        return false;
    }

    this->m_charge_state = charge_state_t::charging;
    g_prediction.update( );
    return true;
}

void tick_manager::observe_server_tick( )
{
    const i32 tick{ g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick };
    if ( tick != this->m_server_tick )
    {
        // the server reads packets before it runs a tick and sends that tick's snapshot after it, so every batch a
        // snapshot is the first to acknowledge was read in a tick after the previous snapshot's and no later than its own
        const i32 acknowledged{ g_tf2.m_cl->command_ack };
        if ( this->m_server_tick >= 0 && tick > this->m_server_tick && acknowledged > this->m_server_ack )
        {
            const i32 oldest{ math::bigger( this->m_server_ack, acknowledged - static_cast< i32 >( MULTIPLAYER_BACKUP ) ) };
            for ( i32 command{ acknowledged }; command > oldest; --command )
            {
                sent_batch_t &batch{ this->m_sent_batches[ static_cast< usize >( command ) % MULTIPLAYER_BACKUP ] };
                if ( batch.m_last_command == command )
                {
                    batch.m_after = this->m_server_tick;
                    batch.m_by    = tick;
                }
            }
        }

        this->m_server_tick = tick;
        this->m_server_ack  = acknowledged;
        this->locate_offset( );
    }
}

double tick_manager::clock_now( ) const
{
    if ( g_tf2.m_cl->m_NetChannel->IsLoopback( ) )
    {
        return static_cast< double >( this->m_send_tick );
    }

    return g_tf2.m_engine->Plat_FloatTime( ) / static_cast< double >( TICK_INTERVAL );
}

i32 tick_manager::arrival_tick( double clock ) const
{
    const double lands{ clock + ( this->m_offset_low + this->m_offset_high ) * 0.5 };
    const i32    tick{ static_cast< i32 >( lands ) };
    return tick < lands ? tick + 1 : tick;
}

void tick_manager::locate_offset( )
{
    bool   known{ };
    double low{ };
    double high{ };

    const i32 oldest{ math::bigger( this->m_server_ack - static_cast< i32 >( MULTIPLAYER_BACKUP ), 0 ) };
    for ( i32 command{ this->m_server_ack }; command > oldest; --command )
    {
        const sent_batch_t &batch{ this->m_sent_batches[ static_cast< usize >( command ) % MULTIPLAYER_BACKUP ] };
        if ( batch.m_last_command != command || batch.m_by < 0 )
        {
            continue;
        }

        const double earliest{ batch.m_after - batch.m_sent };
        const double latest{ batch.m_by - batch.m_sent };
        if ( known && ( earliest >= high || latest <= low ) )
        {
            break;
        }

        low   = known ? math::bigger( low, earliest ) : earliest;
        high  = known ? math::smaller( high, latest ) : latest;
        known = true;
    }

    this->m_offset_low   = low;
    this->m_offset_high  = high;
    this->m_offset_known = known;
}

command_tickbase_projection_t tick_manager::project_current_command_tickbase( i32 send_delay_ticks )
{
    command_tickbase_projection_t out{ };
    if ( !g_cl.m_local || !g_cl.m_local->IsAlive( ) || !g_tf2.m_cl || !g_tf2.m_cl->m_NetChannel )
    {
        return out;
    }

    const i32 delay{ math::bigger( send_delay_ticks, 0 ) };
    const i32 frame{ this->send_frame( ) };
    if ( frame < 0 )
    {
        return out;
    }

    out.m_server.m_anchor_server_tick    = g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick;
    out.m_server.m_projected_server_tick = frame + delay;

    out.m_choked_prefix    = math::bigger( g_tf2.m_cl->chokedcommands, 0 );
    out.m_future_delay     = delay;
    out.m_simulation_ticks = out.m_choked_prefix + delay + 1;

    const bool loopback{ g_tf2.m_cl->m_NetChannel->IsLoopback( ) };
    const i32  frame_ticks{ loopback ? math::bigger( g_tf2.m_globals->simTicksThisFrame, 1 ) : 1 };
    const i32  correction_ticks{
        TIME_TO_TICKS( math::clamp( g_tf2.sv_clockcorrection_msecs->GetFloat( ) / 1000.0f, 0.0f, 1.0f ) )
    };
    tick_fix_t fix{ };
    fix.m_simulation_ticks = out.m_simulation_ticks;
    fix.m_server_tick      = out.m_server.m_projected_server_tick;
    fix.m_frame_ticks      = frame_ticks;
    fix.m_correction_ticks = correction_ticks;
    fix.m_multiplayer      = g_tf2.m_globals->maxClients > 1;

    const i32 batch_start{ g_cl.m_local->m_nTickBase( ) - out.m_choked_prefix };
    out.m_player_tickbase = fix.adjusted_tick( batch_start ) + out.m_choked_prefix;
    return out;
}

bool tick_manager::record_fix( i32 command, i32 commands )
{
    Assert( command >= 0 && commands > 0 );
    if ( !this->m_offset_known )
    {
        return false;
    }

    CNetChannel *channel{ g_tf2.m_cl->m_NetChannel };
    Assert( channel != nullptr );

    // @::AdjustPlayerTimeBase measures the tickbase against the tick the batch runs in, the one its send time lands in
    const bool       loopback{ channel->IsLoopback( ) };
    const tick_fix_t plan{
        command,
        math::smaller( commands, this->m_max_shift ),
        this->arrival_tick( this->m_send_clock ),
        loopback ? math::bigger( g_tf2.m_globals->simTicksThisFrame, 1 ) : 1,
        TIME_TO_TICKS( math::clamp( g_tf2.sv_clockcorrection_msecs->GetFloat( ) / 1000.0f, 0.0f, 1.0f ) ),
        g_tf2.m_globals->maxClients > 1,
        this->m_pending_copies + this->m_frame_copies
    };

    tick_fix_t &slot{ this->m_fixes[ static_cast< usize >( command ) % MULTIPLAYER_BACKUP ] };
    bool        changed{ slot.m_command_number != plan.m_command_number || slot.m_simulation_ticks != plan.m_simulation_ticks ||
                         slot.m_server_tick != plan.m_server_tick || slot.m_frame_ticks != plan.m_frame_ticks ||
                         slot.m_correction_ticks != plan.m_correction_ticks || slot.m_multiplayer != plan.m_multiplayer ||
                         slot.m_copies != plan.m_copies };
    if ( changed )
    {
        slot = plan;
    }

    for ( i32 at{ command + 1 }; at < command + commands; ++at )
    {
        tick_fix_t &interior{ this->m_fixes[ static_cast< usize >( at ) % MULTIPLAYER_BACKUP ] };
        if ( interior.m_command_number == at )
        {
            interior = tick_fix_t{ };
            changed  = true;
        }
    }

    return changed;
}

i32 tick_manager::group_start( i32 command ) const
{
    const tick_fix_t &current{ this->m_fixes[ static_cast< usize >( command ) % MULTIPLAYER_BACKUP ] };
    if ( !current.m_sent )
    {
        return command;
    }

    const tick_fix_t *head{ &current };
    i32               first{ command };
    i32               end{ command };
    i32               commands{ };

    const i32 oldest{ math::bigger( g_tf2.m_cl->last_command_ack + 1, command - static_cast< i32 >( MULTIPLAYER_BACKUP ) + 1 ) };
    for ( i32 at{ oldest }; at <= command; ++at )
    {
        const tick_fix_t &packet{ this->m_fixes[ static_cast< usize >( at ) % MULTIPLAYER_BACKUP ] };
        if ( packet.m_command_number != at || !packet.m_sent )
        {
            continue;
        }
        if ( at != end || packet.m_server_tick != head->m_server_tick ||
             packet.m_frame_ticks != head->m_frame_ticks || packet.m_correction_ticks != head->m_correction_ticks ||
             packet.m_multiplayer != head->m_multiplayer || commands + packet.m_simulation_ticks > this->m_max_shift )
        {
            head     = &packet;
            first    = at;
            commands = 0;
        }
        commands += packet.m_simulation_ticks;
        end = at + packet.m_simulation_ticks;
    }
    return first;
}

void tick_manager::clear( )
{
    *this = tick_manager{ };
    g_holdaim_mgr.reset( );
}

bool tick_manager::on_run_simulation( C_BasePlayer *player, i32 command_number )
{
    this->m_copies_left = -1;
    this->m_seen_copies = 0;
    if ( command_number < 0 )
    {
        return false;
    }

    i32 &ran{ this->m_ran_tickbases[ static_cast< usize >( command_number ) % MULTIPLAYER_BACKUP ] };
    ran = player->m_nTickBase( );

    tick_fix_t &fix{ this->m_fixes[ static_cast< usize >( command_number ) % MULTIPLAYER_BACKUP ] };
    if ( fix.m_command_number != command_number )
    {
        return false;
    }

    // PhysicsSimulate aggregates all queued contexts, including recovered backup commands.
    // The 15-new-command wire field is not a server simulation boundary. Matching arrival
    // estimates permit this grouping but do not prove the remote server's actual receive cadence.
    if ( this->group_start( command_number ) < command_number )
    {
        return false;
    }

    tick_fix_t group{ fix };
    if ( group.m_sent )
    {
        for ( i32 next{ command_number + group.m_simulation_ticks }; group.m_simulation_ticks < this->m_max_shift; )
        {
            const tick_fix_t &following{ this->m_fixes[ static_cast< usize >( next ) % MULTIPLAYER_BACKUP ] };
            if ( following.m_command_number != next || !following.m_sent || following.m_simulation_ticks <= 0 ||
                 following.m_server_tick != group.m_server_tick ||
                 following.m_frame_ticks != group.m_frame_ticks || following.m_correction_ticks != group.m_correction_ticks ||
                 following.m_multiplayer != group.m_multiplayer || group.m_simulation_ticks + following.m_simulation_ticks > this->m_max_shift )
            {
                break;
            }
            group.m_simulation_ticks += following.m_simulation_ticks;
            next += following.m_simulation_ticks;
        }
    }

    // the copies the server ran before the batch are ours to run first, except those already in the networked state we
    // predict from; past the acknowledged command, the server's tickbase only grows by running copies;
    const i32 acknowledged{ g_prediction.network_command( ) };
    const i32 seen{ command_number == acknowledged + 1
                        ? math::bigger( g_prediction.network_tickbase( ) - this->m_ran_tickbases[ static_cast< usize >( acknowledged ) % MULTIPLAYER_BACKUP ] - 1, 0 )
                        : 0 };

    // a shift plans its packet from the tickbase the server would correct, the one before any fix of ours..
    const i32 before{ player->m_nTickBase( ) };
    const i32 raw{ before + math::bigger( fix.m_copies - seen, 0 ) };
    const i32 after{ group.adjusted_tick( raw ) };
    fix.m_raw_tickbase  = raw;
    this->m_seen_copies = seen;
    if ( raw > before )
    {
        this->m_copies_left   = raw - before;
        this->m_copy_tickbase = before;
        this->m_head_tickbase = after;
    }
    if ( group.m_sent )
    {
        g_cl.reconcile_throw_batch( command_number, group.m_simulation_ticks,
                                    after, group.m_server_tick );
    }
#if defined( ATLAS_DEV )
    const i32 correction{ before - after };
    if ( correction != fix.m_logged_shift )
    {
        fix.m_logged_shift = correction;
        DBGOUT( "tickbase fix: cmd %d sim %d server %d frame %d tolerance %d before %d after %d shift %d",
                command_number, group.m_simulation_ticks, fix.m_server_tick, fix.m_frame_ticks,
                fix.m_correction_ticks, before, after, correction );
    }
#endif
    player->m_nTickBase( ) = after;
    ran                    = after;
    return after != before;
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/player.cpp#L3392-L3415
bool tick_manager::next_copy( C_BasePlayer *player, i32 command_number, CUserCmd &copy )
{
    if ( this->m_copies_left < 0 )
    {
        return false;
    }

    if ( this->m_copies_left == 0 )
    {
        this->m_copies_left    = -1;
        player->m_nTickBase( ) = this->m_head_tickbase;
        return false;
    }

    --this->m_copies_left;
    copy                   = g_tf2.m_input->m_pCommands[ static_cast< usize >( command_number - 1 ) % MULTIPLAYER_BACKUP ];
    player->m_nTickBase( ) = this->m_copy_tickbase++;
    return true;
}

void tick_manager::on_move_sent( i32 command, i32 commands )
{
    if ( commands <= 0 || commands > MAX_SHIFT || command < commands )
    {
        this->m_pending_copies = 0;
        return;
    }

    this->m_send_clock                                                           = this->clock_now( );
    this->m_last_send_clock                                                      = this->m_send_clock;
    this->m_last_send_tick                                                       = this->m_send_tick;
    this->m_sent_batches[ static_cast< usize >( command ) % MULTIPLAYER_BACKUP ] = sent_batch_t{ command, this->m_send_clock };
    if ( !g_cl.m_local || !g_cl.m_local->IsAlive( ) )
    {
        this->m_pending_copies = 0;
        return;
    }

    const i32   first{ command - commands + 1 };
    const bool  changed{ this->record_fix( first, commands ) };
    tick_fix_t &sent{ this->m_fixes[ static_cast< usize >( first ) % MULTIPLAYER_BACKUP ] };
    if ( sent.m_command_number == first )
    {
        sent.m_sent = true;
    }
    this->m_pending_copies = 0;

    if ( changed || this->group_start( first ) < first )
    {
        g_tf2.m_prediction->m_nCommandsPredicted = 0;
        g_prediction.update( );
    }

#if defined( ATLAS_DEV )
    const tick_fix_t &fix{ this->m_fixes[ static_cast< usize >( first ) % MULTIPLAYER_BACKUP ] };
    if ( commands > MAX_NEW_COMMANDS || fix.m_logged_shift )
    {
        this->m_debug_command = command;
        DBGOUT( "tickbase packet: seq %d first %d last %d count %d estimated %d observed %d client %d frame %d offset %.3f %.3f",
                g_tf2.m_cl->m_NetChannel->m_nOutSequenceNr - 1, first, command, commands,
                fix.m_server_tick, this->m_server_tick, g_tf2.m_globals->tickcount, g_tf2.m_globals->framecount,
                this->m_offset_low, this->m_offset_high );
    }
#endif
    g_holdaim_mgr.reset( );
}

void tick_manager::on_post_data_update( )
{
    const i32 acknowledged{ g_tf2.m_cl->command_ack };
    if ( this->m_charge_state == charge_state_t::synchronizing && acknowledged >= this->m_charge_fence )
    {
        this->m_charge_state = charge_state_t::idle;
        DBGOUT( "tick_manager::on_post_data_update: Command %i acknowledged.", acknowledged );
    }

#if defined( ATLAS_DEV )
    if ( !g_cl.m_local || this->m_debug_command < 0 || acknowledged < this->m_debug_command )
    {
        return;
    }

    DBGOUT( "tickbase ack: seq %d command %d watched %d server %d base %d",
            g_tf2.m_cl->m_NetChannel->m_nOutSequenceNrAck, acknowledged, this->m_debug_command,
            g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick, g_cl.m_local->m_nTickBase( ) );
    this->m_debug_command = -1;
#endif
}

tick_status_t tick_manager::status( ) const
{
    switch ( this->m_charge_state )
    {
        case charge_state_t::charging: return tick_status_t::charging;
        case charge_state_t::synchronizing: return tick_status_t::waiting;
        default: break;
    }

    switch ( this->m_rapid_state )
    {
        case rapid_fire_state_t::ready: return tick_status_t::ready;
        case rapid_fire_state_t::not_enough: return tick_status_t::not_enough;
        case rapid_fire_state_t::waiting: return tick_status_t::cooldown;
        default: return tick_status_t::unable;
    }
}

struct tick_look_t
{
    const char *m_label{ };
    color_t     m_text{ };
};

static indicator_card_t s_indicator{ };
static u8               s_status_mix[ static_cast< usize >( tick_status_t::count ) ]{ };

static tick_look_t tick_look( tick_status_t status )
{
    switch ( status )
    {
        case tick_status_t::ready: return tick_look_t{ ATLAS_XORSTR_STATIC( "ready" ), g_theme.m_accent };
        case tick_status_t::charging: return tick_look_t{ ATLAS_XORSTR_STATIC( "charging" ), CONTROL_VALUE };
        case tick_status_t::waiting: return tick_look_t{ ATLAS_XORSTR_STATIC( "waiting" ), STATUS_WARNING };
        case tick_status_t::not_enough: return tick_look_t{ ATLAS_XORSTR_STATIC( "not ready" ), MENU_TEXT_DIM };
        case tick_status_t::cooldown: return tick_look_t{ ATLAS_XORSTR_STATIC( "cooldown" ), MENU_TEXT_DIM };
        default: return tick_look_t{ ATLAS_XORSTR_STATIC( "unable" ), MENU_TEXT_DIM };
    }
}

ATLAS_FORCE_INLINE u8 status_mix( tick_status_t status )
{
    return gui::eased_mix( s_status_mix[ static_cast< usize >( status ) ] );
}

void tick_manager::paint( ) const
{
    const bool wanted{ g_cfg.m_exploits.m_rapid_fire && g_cfg.m_exploits.m_indicator && g_tf2.m_engine->IsInGame( ) &&
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

    // between the frames the command of the tick already spent its own grant, what's left is the bank; A charging
    // tick made no command, and its grant is banked too.
    const i32 banked{ math::clamp( this->m_credits, 0, this->m_shown_max_shift ) };

    s_indicator.m_at = g_cfg.m_misc.m_tick_indicator_position;
    const u32 delta{ step_indicator( s_indicator, static_cast< float >( banked ) / static_cast< float >( this->m_shown_max_shift ),
                                     g_cl.m_screen_width, g_cl.m_screen_height ) };
    g_cfg.m_misc.m_tick_indicator_position = s_indicator.m_at;

    const tick_status_t status{ this->status( ) };
    for ( usize index{ }; index < static_cast< usize >( tick_status_t::count ); ++index )
    {
        gui::step_mix( s_status_mix[ index ], index == static_cast< usize >( status ), delta, WIDGET_TOGGLE_DURATION );
    }

    const u8 hot{ status_mix( tick_status_t::ready ) };
    const u8 held{ status_mix( tick_status_t::waiting ) };

    const color_t edge{ gui::mix_color( gui::mix_color( g_theme.m_input_focus_border, g_theme.m_accent, hot ), STATUS_WARNING, held ) };
    const color_t wash{ gui::mix_color( g_theme.m_selected_background, g_theme.m_row_selected_background, hot ) };
    const color_t amount_color{ gui::mix_color( MENU_TEXT_DIM, CHECKBOX_MARK_COLOR, hot ) };

    const indicator_layout_t layout{ draw_indicator( s_indicator, ATLAS_XORSTR_STATIC( "ticks" ), wash, edge ) };
    const u8                 enter{ layout.m_enter };

    for ( usize index{ }; index < static_cast< usize >( tick_status_t::count ); ++index )
    {
        const u8 mix{ status_mix( static_cast< tick_status_t >( index ) ) };
        if ( !mix )
        {
            continue;
        }

        const tick_look_t look{ tick_look( static_cast< tick_status_t >( index ) ) };
        g_render.text( MENU_FONT_TINY( ), layout.m_state, gui::with_alpha( gui::with_alpha( look.m_text, mix ), enter ),
                       look.m_label, false, render::text_align::right );
    }

    gui::draw_icon_at( ICON_ZAP, layout.m_glyph, gui::with_alpha( gui::with_alpha( edge, indicator_glyph_alpha( hot ) ), enter ) );

    char amount[ 12 ]{ };
    str::format_to( amount, sizeof( amount ), ATLAS_XORSTR_STATIC( "{}/{}" ), banked, this->m_shown_max_shift );
    g_render.text( MENU_FONT_TINY( ), layout.m_amount, gui::with_alpha( amount_color, enter ), amount, false,
                   render::text_align::left );
}

i32 tick_manager::command_frame( i32 command ) const
{
    for ( i32 at{ command }; at > command - static_cast< i32 >( MULTIPLAYER_BACKUP ); --at )
    {
        const tick_fix_t &batch{ this->m_fixes[ static_cast< usize >( at ) % MULTIPLAYER_BACKUP ] };
        if ( batch.m_command_number != at )
        {
            continue;
        }
        if ( batch.m_sent && command < at + batch.m_simulation_ticks )
        {
            return batch.m_server_tick;
        }
        break;
    }
    return this->send_frame( );
}

i32 tick_manager::send_frame( ) const
{
    return this->m_offset_known ? this->arrival_tick( this->m_send_clock ) : -1;
}
