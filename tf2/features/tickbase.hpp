#pragma once

#include "link.hpp"

static constexpr i32 MAX_SHIFT{ MAX_NEW_COMMANDS + MAX_BACKUP_COMMANDS }; // MAX_NEW_COMMANDS + MAX_BACKUP_COMMANDS

enum class charge_state_t : i32
{
    idle,
    charging,
    synchronizing,
};

enum class shift_type_t : i32
{
    none,
    warp,
    rapidfire,
    speedhack,
};

enum class rapid_fire_state_t : i32
{
    unable,
    not_enough,
    waiting,
    ready,
};

enum class tick_status_t : i32
{
    unable,
    not_enough,
    cooldown,
    charging,
    waiting,
    ready,
    count,
};

enum class shot_command_t : i32
{
    fires,
    flips, // flips a spinning minigun to FIRING, the first extra shoots
    spins, // spins a firing minigun down to SPINNING, see spin_shot
};

struct tick_fix_t
{
    i32  m_command_number{ -1 };
    i32  m_simulation_ticks{ };
    i32  m_server_tick{ };
    i32  m_frame_ticks{ 1 };
    i32  m_correction_ticks{ };
    bool m_multiplayer{ };
    i32  m_copies{ }; // of our last command the server runs before this batch, see drop_overflow
    bool m_sent{ };
    i32  m_raw_tickbase{ };

#if defined( ATLAS_DEV )
    i32 m_logged_shift{ };
#endif
    ATLAS_FORCE_INLINE i32 adjusted_tick( i32 tickbase ) const
    {
        if ( !this->m_multiplayer )
        {
            return this->m_server_tick - this->m_simulation_ticks + this->m_frame_ticks;
        }

        const i32 ideal{ this->m_server_tick + this->m_correction_ticks };
        const i32 estimated{ tickbase + this->m_simulation_ticks };
        if ( estimated > ideal + this->m_correction_ticks || estimated < this->m_server_tick )
        {
            return ideal - this->m_simulation_ticks + this->m_frame_ticks;
        }

        return tickbase;
    }
};

struct anti_warp_t
{
    vec3 m_axis{ };
    f32  m_entry{ };
    f32  m_travel{ };
    bool m_anchored{ };
};

struct rapid_fire_plan_t
{
    vec3 m_axis{ };
    f32  m_path[ MAX_SHIFT ]{ };
    i32  m_extra[ MAX_SHIFT ]{ };
    i32  m_shots{ };
    i32  m_extras{ };
    i32  m_tick{ };
    vec3 m_step{ };
    f32  m_delay{ };
    bool m_slides{ };

    i32 next_shot( i32 extra ) const
    {
        const f32 next_attack{ TICKS_TO_TIME( this->m_tick + extra ) + this->m_delay };

        i32 next{ extra + 1 };
        while ( TICKS_TO_TIME( this->m_tick + next ) < next_attack )
        {
            ++next;
        }

        return next;
    }

    bool keeps_firing( i32 shot, i32 extra ) const
    {
        return TICKS_TO_TIME( this->m_tick + extra ) < static_cast< f32 >( TICKS_TO_TIME( this->m_tick + shot ) + 0.2 );
    }
};

struct sent_batch_t
{
    i32    m_last_command{ -1 };
    double m_sent{ };
    i32    m_after{ -1 };
    i32    m_by{ -1 };
};

struct server_tick_projection_t
{
    i32 m_anchor_server_tick{ -1 };
    i32 m_projected_server_tick{ -1 };

    bool valid( ) const
    {
        return this->m_anchor_server_tick >= 0 && this->m_projected_server_tick >= 0;
    }
};

struct command_tickbase_projection_t
{
    server_tick_projection_t m_server{ };
    i32                      m_player_tickbase{ -1 };
    i32                      m_choked_prefix{ };
    i32                      m_future_delay{ };
    i32                      m_simulation_ticks{ };

    bool valid( ) const
    {
        return this->m_server.valid( ) && this->m_player_tickbase >= 0;
    }
};

class tick_manager
{
public:
    bool                          on_cl_move( float accumulated_extra_samples, bool final_tick );
    void                          on_pre_prediction( );
    void                          on_prediction( );
    bool                          on_run_simulation( C_BasePlayer *player, i32 command_number );
    bool                          next_copy( C_BasePlayer *player, i32 command_number, CUserCmd &copy );
    void                          observe_server_tick( );
    command_tickbase_projection_t project_current_command_tickbase( i32 send_delay_ticks );
    void                          on_move_sent( i32 command, i32 commands );

    static ATLAS_FORCE_INLINE double packet_time( bool loopback )
    {
        return loopback ? *g_tf2.m_net_time.get_as< const double * >( ) : g_tf2.m_engine->Plat_FloatTime( );
    }

    void on_post_data_update( );
    void paint( ) const;

    void clear( );
    i32  command_frame( i32 command ) const;
    i32  send_frame( ) const;
    i32  shift_room( ) const;

    inline bool shifting( ) const
    {
        return this->m_shift_type != shift_type_t::none;
    }

    inline bool banks_tick( ) const
    {
        return this->m_banks_tick;
    }

    inline bool has_ticks_stored( ) const
    {
        return this->m_credits > 3;
    }

    inline bool holds_packet( ) const
    {
        return this->m_credits_to_consume > 0;
    }

    inline bool recharging( ) const
    {
        return this->m_charge_state == charge_state_t::charging;
    }

    inline i32 seen_copies( ) const
    {
        return this->m_seen_copies;
    }

    inline bool at_budget_cap( ) const
    {
        return this->m_credits > this->m_max_shift;
    }

    inline bool withholds_shot( ) const
    {
        return this->m_shift_type == shift_type_t::rapidfire && ( this->m_withheld >> this->m_credits_to_consume & 1u ) != 0;
    }

    inline bool max_shift( ) const
    {
        return this->m_max_shift;
    }

private:
    void               grant_credit( );
    bool               drop_overflow( );
    i32                bank_cap( ) const;
    bool               should_charge( );
    tick_status_t      status( ) const;
    [[nodiscard]] bool recharge( );

    void flush( bool final_tick );
    bool record_fix( i32 command, i32 commands );
    i32  group_start( i32 command ) const;

    double clock_now( ) const;
    i32    arrival_tick( double clock ) const;
    void   locate_offset( );

    i32  packet_room( ) const;
    i32  shot_tick( i32 extra ) const;
    i32  shift_amount( C_TFWeaponBase *wep, shot_command_t command ) const;
    i32  update_rapid_fire_state( );
    i32  update_minigun_state( shot_command_t command, f32 timer );
    void shift( );
    void anti_warp( );

    i32 true_max_shift( ) const;

    rapid_fire_plan_t plan_rapid_fire( i32 extras ) const;

    tick_fix_t m_fixes[ MULTIPLAYER_BACKUP ]{ };
    i32        m_ran_tickbases[ MULTIPLAYER_BACKUP ]{ };
    i32        m_max_shift{ 24 };
    i32        m_shown_max_shift{ 24 };
    i32        m_server_tick{ -1 };

    sent_batch_t m_sent_batches[ MULTIPLAYER_BACKUP ]{ };
    i32          m_send_tick{ };
    double       m_send_clock{ };
    double       m_last_send_clock{ };
    i32          m_last_send_tick{ -1 };
    i32          m_server_ack{ -1 };
    double       m_offset_low{ };
    double       m_offset_high{ };
    bool         m_offset_known{ };

    charge_state_t m_charge_state{ charge_state_t::idle };
    i32            m_credits{ 0 };
    i32            m_pending_copies{ 0 };
    i32            m_frame_copies{ 0 };
    i32            m_frame_tick{ 0 };
    bool           m_frame_final{ true };
    i32            m_copies_left{ -1 };
    i32            m_seen_copies{ };
    i32            m_copy_tickbase{ };
    i32            m_head_tickbase{ };
    i32            m_charge_fence{ -1 };
    f32            m_passive_timer{ 0.0f };

    i32                m_credits_to_consume{ 0 };
    rapid_fire_state_t m_rapid_state{ rapid_fire_state_t::unable };
    shift_type_t       m_shift_type{ shift_type_t::none };
    shot_command_t     m_shot_command{ shot_command_t::fires };
    anti_warp_t        m_anti_warp{ };
    u32                m_withheld{ }; // by extras still to make after the one it marks

    f32 m_old_next_attack{ 0.0f };
    f32 m_old_next_primary_attack{ 0.0f };
    f32 m_old_next_secondary_attack{ 0.0f };
    i32 m_old_minigun_state{ };

    bool m_banks_tick{ false };
    bool m_warned_low_fps{ false };

#if defined( ATLAS_DEV )
    i32 m_debug_command{ -1 };
#endif
};

extern tick_manager g_tickbase;
