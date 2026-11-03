#pragma once

#include "link.hpp"

enum : usize
{
    PILL_CRIT_KEEP_ROTATION_FIX = 0, // the pill pass owns the tick, the crit stands down
    PILL_CRIT_PREFER_CRIT       = 1, // the crit pass owns the tick, the pill S P I N S
};

class IGameEvent;

struct crit_totals_t
{
    i32 m_ranged{ };
    i32 m_crit{ };
    i32 m_melee{ };

    i32   total( ) const;
    float observed( ) const;
    bool  banned( float threshold ) const;
    i32   damage_required( float threshold ) const;
    i32   crit_for_observed( float observed ) const;
};

struct bucket_t
{
    float m_tokens{ };
    i32   m_checks{ };
    i32   m_requests{ };
};

class crit_history
{
public:
    void          reset( i32 previous_server, float now );
    void          add( i32 damage, bool melee, bool counted_crit, float now );
    void          reconcile( i32 server, float now );
    void          sample( float observed, float now );
    bool          resolve( float now );
    crit_totals_t totals( ) const;

    i32 m_server{ };
    i32 m_round{ };
    i32 m_residual{ };

private:
    struct event_t
    {
        crit_totals_t m_damage{ };
        float         m_time{ };
    };

    struct sample_t
    {
        float m_value{ -1.0f };
        float m_time{ };
        i32   m_from{ };
    };

    void settle( float now );
    void record( );
    void push_state( const crit_totals_t &state );
    bool explains( const sample_t &sample ) const;
    i32  exact_correction( const sample_t &sample ) const;

    crit_totals_t m_base{ };
    crit_totals_t m_events{ };
    event_t       m_pending[ 256 ]{ };
    i32           m_head{ };
    i32           m_count{ };
    i32           m_crit_correction{ };
    i32           m_reset_server{ };
    float         m_reset_time{ };
    bool          m_wait_server{ };
    crit_totals_t m_states[ 256 ]{ };
    i32           m_recorded{ };
    i32           m_update_start{ };
    i32           m_previous_update_start{ };
    sample_t      m_samples[ 8 ]{ };
};

class crits
{
public:
    bool install( );
    void run( );
    void clear( );
    void paint( ) const;

    void on_create_move( i32 command );
    void on_run_simulation( i32 sequence, CUserCmd *command );
    void begin_crit_calc( C_TFWeaponBase *weapon );
    void end_crit_calc( C_TFWeaponBase *weapon );
    bool wants_check_increment( ) const;

    void on_player_hurt( IGameEvent *event );
    void on_observed_crit_chance( C_TFWeaponBase *weapon, float observed );
    void on_network_update( );
    void reset_round_history( );
    void on_player_life_change( i32 index );
    void on_health( C_BaseEntity *entity, i32 health );
    void on_init_predictable( C_BaseEntity *entity );

    // everything FirePipeBomb pulls out of the shared random stream for one launch
    struct pipe_launch_t
    {
        float m_jitter_right{ };
        float m_jitter_up{ };
        i32   m_spin{ };
    };

    pipe_launch_t pipe_launch_for( i32 command, bool skip_angles = false ) const;
    void seed_launch_random( i32 command, bool skip_angles = false ) const;

    enum class crit_status : u8
    {
        none,             // dead, or random crits off for this weapon
        ready,            // the bucket pays for a crit right now
        low_bucket,       // the bucket cannot pay for one yet
        streaming_window, // rapid-fire window already open
        streaming_wait,   // next start roll is still on its one-second cooldown
        crit_boosted,     // already critting
        observed_limit,   // the round's random-crit share sits over the threshold; damage lifts it
        seed_conflict,    // stood down so seed prediction keeps its correction
        too_expensive,
        count,
    };

private:
    enum class crit_type
    {
        regular,
        rapid_fire,
        melee,
    };

    struct indicator_t
    {
        crit_status m_status{ };
        float       m_fill{ };       // bucket over its cap, or remaining rapid-fire window over its duration
        i32         m_available{ };  // crits the bucket pays for back to back
        i32         m_potential{ };  // crits a full bucket pays for at the current ramp, or at the base cost when it pays none
        i32         m_ban_damage{ }; // non-crit damage still owed before the server lifts the ban
        float       m_window{ };     // seconds left on an open rapid-fire window
        bool        m_resynced{ };
    };

    struct batch_slot_t
    {
        i32 m_sequence{ -1 };
        i32 m_batch{ };
    };

    struct weapon_state_t
    {
        u32   m_handle{ INVALID_EHANDLE_INDEX };
        i32   m_saved_tick{ };
        float m_bucket{ };
        i32   m_checks{ };
        i32   m_requests{ };
        i32   m_seed{ };
        float m_crit_time{ };
        float m_last_rapid_check{ };
        i32   m_last_frame{ };
        bool  m_attack_crit{ };
    };

    crit_type classify( C_TFWeaponBase *weapon ) const;
    i32       weapon_mode( C_TFWeaponBase *weapon ) const;
    float     crit_chance( C_TFWeaponBase *weapon, float crit_mult ) const;
    float     shot_damage( C_TFWeaponBase *weapon, bool for_withdraw ) const;
    bool      weapon_can_crit( C_TFWeaponBase *weapon ) const;
    bool      guaranteed_crit( C_TFWeaponBase *weapon ) const;
    float     observed_chance( ) const;
    bool      is_crit_banned( C_TFWeaponBase *weapon ) const;
    i32       damage_to_lift_ban( C_TFWeaponBase *weapon ) const;
    bool      bucket_pays( C_TFWeaponBase *weapon ) const;
    i32       crits_paid( C_TFWeaponBase *weapon, bucket_t bucket ) const;
    float     last_crit_check( C_TFWeaponBase *weapon ) const;
    bool      seed_conflict( ) const;

    crit_totals_t pending_totals( ) const;

    i32  find_crit_command( C_TFWeaponBase *weapon, i32 start, bool want_crit ) const;
    i32  find_pipe_command( i32 start, bool avoid_crit ) const;
    bool steer_crit( );
    bool steer_pipe( );
    void guard_attack_buttons( );
    void save_weapon_state( C_TFWeaponBase *weapon );
    void update_indicator( );
    i32  batch_of( i32 sequence ) const;

    indicator_t    m_indicator{ };
    batch_slot_t   m_batches[ MULTIPLAYER_BACKUP ]{ };
    weapon_state_t m_weapon_states[ MAX_WEAPONS ]{ };
    CUserCmd      *m_sim_input{ };
    i32            m_sim_sequence{ -1 };
    i32            m_create_command{ -1 };
    i32            m_create_frame{ -1 };
    i32            m_open_batch{ -1 };
    i32            m_calc_frame{ -1 };
    bool           m_in_crit_calc{ };
    bool           m_ran_this_command{ };
#if defined( ATLAS_DEV )
    i32  m_log_checks{ };
    i32  m_log_seed{ };
    bool m_log_inherited{ };
#endif

    crit_history m_history{ };
    float        m_resynced_at{ -10.0f };
    void         on_recreated( u32 handle );

    struct health_t
    {
        u32 m_handle{ INVALID_EHANDLE_INDEX };
        i32 m_current{ -1 };
        i32 m_previous{ -1 };
        i32 m_event_health{ -1 };
        i32 m_event_tick{ -1 };
    };

    struct projectile_t
    {
        u32        m_handle{ INVALID_EHANDLE_INDEX };
        u32        m_launcher{ INVALID_EHANDLE_INDEX };
        ETFClassID m_class{ };
        i32        m_weapon{ };
        i32        m_round{ };
        float      m_fired{ };
        float      m_seen{ };
        float      m_radius{ };
        vec3       m_previous{ };
        vec3       m_origin{ };
        vec3       m_velocity{ };
        bool       m_crit{ };
        bool       m_live{ };
    };

    struct crit_shot_t
    {
        i32 m_tick{ -1 };
        i32 m_damage{ };
        i32 m_weapon{ };
    };

    struct hurt_t
    {
        i32                 m_victim{ };
        i32                 m_amount{ };
        i32                 m_before{ -1 };
        i32                 m_after{ };
        i32                 m_weapon{ };
        i32                 m_custom{ };
        vec3                m_position{ };
        bool                m_crit{ };
        bool                m_feign{ };
        crit_totals_t       m_damage{ };
        const projectile_t *m_projectile{ };
    };

    struct observed_t
    {
        u32   m_handle{ INVALID_EHANDLE_INDEX };
        i32   m_round{ -1 };
        i32   m_received_tick{ -1 };
        float m_value{ };
        bool  m_known{ };
        bool  m_pending{ };
        bool  m_active{ };
    };

    void                scan_projectiles( float now );
    const projectile_t *projectile_for( const hurt_t &hurt, float now ) const;
    observed_t         &observed_for( u32 handle );
    void                prepare_damage( float now );
    void                apply_damage( float now );

    health_t     m_health[ MAX_TRACKED_PLAYERS ]{ };
    projectile_t m_projectiles[ 128 ]{ };
    crit_shot_t  m_crit_shots[ 64 ]{ };
    u32          m_crit_shot_next{ };
    hurt_t       m_hurts[ 128 ]{ };
    observed_t   m_observed[ MAX_WEAPONS ]{ };
    i32          m_hurt_count{ };
    u32          m_active{ INVALID_EHANDLE_INDEX };
    i32          m_active_tick{ -1 };
    i32          m_last_melee_weapon{ -1 };
    float        m_last_melee_time{ -10.0f };
};

extern crits g_crits;
