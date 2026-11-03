#pragma once

#include "link.hpp"

class IGameEvent;
enum class party_vacc_request : u8;
enum class party_vacc_result : u8;

class auto_vacc
{
public:
    static constexpr i32 RESIST_NONE{ -1 };

    void run( );
    void clear( );
    void on_network_update( );

    void on_player_hurt( IGameEvent *event );
    void on_party_status( u32 medic, u32 patient );
    void on_party_request( u32 patient, u32 medic, party_vacc_request request );
    void on_party_result( u32 medic, u32 patient, party_vacc_result result );

    static i32 weapon_resist_type( i32 weapon_id, i32 item );

private:
    enum cause_t : u8
    {
        cause_crit,
        cause_lethal,
        cause_headshot,
        cause_crit_boost,
        cause_crit_stream,
        cause_cheater_sniper,
    };

    struct ward_t
    {
        C_TFPlayer *m_player{ };
        vec3        m_origin{ };
        vec3        m_velocity{ };
        vec3        m_mins{ };
        vec3        m_maxs{ };
        vec3        m_eye{ };
        f32         m_health{ };
        f32         m_speed{ };
        bool        m_minicrits{ };
        i32         m_covered_until[ MEDIGUN_NUM_RESISTS ]{ };
    };

    struct flight_t
    {
        C_BaseEntity *m_projectile{ };
        C_BaseEntity *m_attacker{ };
        vec3          m_origin{ };
        vec3          m_velocity{ };
        vec3          m_mins{ };
        vec3          m_maxs{ };
        f32           m_gravity{ };
        f32           m_damage{ };
        f32           m_radius{ };
        f32           m_edge{ };
        f32           m_spread{ };
        i32           m_seen{ };
        i32           m_type{ RESIST_NONE };
        i32           m_weapon_id{ };
        bool          m_crit{ };
        bool          m_contact{ };
        bool          m_headshots{ };
        bool          m_crit_burning{ };
        bool          m_standing{ };
    };

    struct landing_t
    {
        vec3 m_position{ };
        i32  m_tick{ };
        bool m_landed{ };
    };

    struct hit_t
    {
        i32  m_tick{ };
        i32  m_type{ };
        i32  m_ward{ };
        i32  m_escape{ };
        f32  m_damage{ };
        bool m_crit{ };
        bool m_standing{ };
    };

    struct need_t
    {
        f32     m_seen{ };
        f32     m_hidden{ };
        i32     m_deadline{ };
        i32     m_escape{ };
        bool    m_continuous{ };
        bool    m_wanted{ };
        cause_t m_cause{ };
    };

    struct sighting_t
    {
        i32   m_serial{ };
        i32   m_deflected{ };
        vec3  m_origin{ };
        float m_time{ };
    };

    struct pop_t
    {
        i32 m_command{ };
        i32 m_tick{ };
        i32 m_type{ };
    };

    static constexpr usize MAX_WARDS{ 2 };
    static constexpr usize MAX_HITS{ 64 };
    static constexpr usize MAX_PRESSES{ 8 };
    static constexpr usize MAX_POPS{ MEDIGUN_NUM_RESISTS };

    C_WeaponMedigun *active_vaccinator( ) const;
    C_TFPlayer      *patient_of( C_WeaponMedigun *medigun ) const;

    bool build_ward( C_TFPlayer *player, ward_t &out ) const;
    void place_patient( ward_t &ward ) const;
    i32  covered_until( C_TFPlayer *player, i32 resist ) const;

    void scan_projectiles( );
    vec3 track_velocity( C_BaseEntity *projectile, bool pipe, f32 gravity );
    bool read_flight( C_BaseEntity *projectile, ETFClassID class_id, flight_t &out );
    void fly_straight( const flight_t &flight );
    bool fly_arc( const flight_t &flight, i32 last, landing_t &out );
    void watch_sticky( const flight_t &flight, C_TFGrenadePipebombProjectile *pipe );
    void watch_roller( const flight_t &flight, C_TFGrenadePipebombProjectile *pipe, const vec3 &position );
    void explode( const flight_t &flight, const vec3 &point, i32 tick, i32 direct, f32 scale );
    bool enters_ward( const flight_t &flight, const ward_t &ward, const vec3 &from, const vec3 &to,
                      f32 from_time, f32 to_time, f32 &entry ) const;
    f32  blow( const flight_t &flight, const ward_t &ward, f32 falloff, bool headshot, bool &crit ) const;
    void add_hit( i32 tick, i32 type, i32 ward, i32 escape, f32 damage, bool crit, bool standing );

    bool aims_at_head( C_TFPlayer *player, const vec3 &eye, const ward_t &ward ) const;
    void scan_players( );
    void scan_sentries( );
    void weigh_hits( );
    void ask_pop( i32 type, i32 deadline, i32 escape, bool continuous, cause_t cause );

    void sync_commands( C_WeaponMedigun *medigun );
    i32  commands_to_reach( i32 resist ) const;
    bool may_pop( i32 resist ) const;
    i32  urgent_resist( ) const;
    bool window_open( i32 resist ) const;
    i32  passive_resist( ) const;

    void drive( i32 wanted, bool pop, cause_t cause, bool requested );
    void notify_pop( i32 resist, cause_t cause ) const;
    void log_state( i32 wanted, bool pop );

    void request_party_charge( );
    void sync_party_patient( C_WeaponMedigun *medigun, C_TFPlayer *patient );
    void clear_party_request( );

    ward_t m_wards[ MAX_WARDS ]{ };
    usize  m_ward_count{ };
    hit_t  m_hits[ MAX_HITS ]{ };
    usize  m_hit_count{ };
    need_t m_needs[ MEDIGUN_NUM_RESISTS ]{ };
    i32    m_exec{ };
    i32    m_horizon{ };
    i32    m_effective{ };
    f32    m_charge{ };

    i32   m_presses[ MAX_PRESSES ]{ };
    usize m_press_count{ };
    pop_t m_pops[ MAX_POPS ]{ };
    usize m_pop_count{ };
    i32   m_last_buttons{ };
    i32   m_crit_stream_until[ MEDIGUN_NUM_RESISTS ]{ };

    sighting_t m_sightings[ MAX_EDICTS ]{ };

    i32  m_last_wanted{ };
    bool m_last_pop{ };

    u32    m_sync_medic{ };
    double m_sync_until{ };
    u64    m_last_request_batch{ };
    u32    m_pending_pop_medic{ };
    double m_pending_pop_until{ };

    u32    m_status_patient{ };
    double m_next_status_time{ };
    u32    m_request_patient{ };
    i32    m_requested_resist{ };
    double m_request_until{ };
    bool   m_request_pop_sent{ };
};

extern auto_vacc g_auto_vacc;
