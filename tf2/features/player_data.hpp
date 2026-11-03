#pragma once

#include "link.hpp"
#include "runtime/unordered_map.hpp"

constexpr i32 VELOCITY_SAMPLES{ 67 }; // i regret writing this
struct velocity_sample_t
{
    vec3  m_origin{ };
    vec3  m_velocity{ }; // ( origin - previous origin ) / gap;
    vec3  m_base_velocity{ };
    vec3  m_mins{ };
    vec3  m_maxs{ };
    vec3  m_hull{ };
    i32   m_tick{ };
    i32   m_gap{ };
    i32   m_lead{ };     // this tick minus the server tick of the snapshot that carried it; the clock correction's head start
    i32   m_batch_lead{ };
    i32   m_ground_index{ -1 };
    i32   m_ground_serial{ -1 };
    float m_maxspeed{ };
    float m_surface_friction{ };
    float m_eye_yaw{ };
    float m_eye_pitch{ };
    bool  m_charging{ };
    bool  m_on_ground{ };
    bool  m_ducked{ };
    u8    m_water_level{ };
    bool  m_swimming{ };
    bool  m_wall{ };
    bool  m_air_current{ };
    bool  m_lost_footing{ };
    bool  m_ground_conveyor{ };
    bool  m_base_velocity_applied{ };
    bool  m_transport_valid{ };
};

inline bool known_conveyor_transport_interval( const velocity_sample_t &newer,
                                               const velocity_sample_t &older )
{
    return newer.m_gap == 1 && newer.m_on_ground && older.m_on_ground &&
           !newer.m_swimming && !older.m_swimming && newer.m_ground_conveyor &&
           older.m_ground_conveyor && newer.m_ground_index >= 0 &&
           newer.m_ground_index == older.m_ground_index &&
           newer.m_ground_serial == older.m_ground_serial &&
           !newer.m_base_velocity_applied && !older.m_base_velocity_applied &&
           newer.m_base_velocity.z == 0.0f;
}

inline vec3 conveyor_relative_interval_velocity( const velocity_sample_t &newer,
                                                 const velocity_sample_t &older )
{
    vec3 out{ newer.m_velocity };
    if ( known_conveyor_transport_interval( newer, older ) )
    {
        out.x -= newer.m_base_velocity.x;
        out.y -= newer.m_base_velocity.y;
    }
    return out;
}

struct player_record_t
{
    i32 m_serial{ };
    bool m_dead{ };

    vec3  m_dormant_origin{ };
    float m_dormant_expire{ };
    bool  m_dormant_manual{ };

    float m_simulation_time{ };
    float m_old_simulation_time{ };
    velocity_sample_t m_samples[ VELOCITY_SAMPLES ]{ };
    i32               m_sample_head{ };
    i32               m_sample_count{ };
    i32 m_hop_stay{ };
    i32 m_hop_tick{ -1 };

    i32   m_vacc_serial{ -1 };
    u8    m_vacc_mask{ };
    double m_vacc_stamp[ 3 ]{ };
    u32   m_vacc_weapon{ INVALID_EHANDLE_INDEX };
    u32   m_vacc_target{ INVALID_EHANDLE_INDEX };
    float m_vacc_charge{ };
    bool  m_vacc_sampled{ };

    i32    m_buff_owner{ };
    double m_buff_stamp{ };
    double m_banner_start{ };
    double m_banner_last{ };
};

struct entity_record_t
{
    i32   m_serial{ };
    vec3  m_dormant_origin{ };
    float m_dormant_expire{ };
    bool  m_dormant_manual{ };
};

class player_data
{
public:
    static bool  install_netvar_hooks( );
    static void  remove_netvar_hooks( );
    void         on_set_dormant( C_BaseEntity *entity, bool dormant );
    void         on_start_sound( const StartSoundParams_t &params );
    void         on_life_change( i32 index, bool dead = false );
    void         on_level_shutdown( );
    void         update( );
    bool         dormant_origin( C_BaseEntity *entity, vec3 &origin, float &remaining );
    bool         alive( i32 index ) const;
    bool         connected( i32 index ) const;
    i32          team( i32 index ) const;
    i32          player_class( i32 index ) const;
    void         on_simulation_time( i32 index, float previous, float current );
    void         hold_stale_simulation_times( );
    float        simulation_time( i32 index ) const;
    float        previous_simulation_time( i32 index ) const;
    float        vacc_remaining( C_TFPlayer *player, i32 resist ) const;
    void         on_player_buff( i32 index, i32 owner );
    float        buff_remaining( i32 index ) const;
    static float window( );

    const velocity_sample_t *velocity_sample( i32 index, i32 age ) const;
    i32                      velocity_samples( i32 index ) const;
    void                     remember_hop( i32 index, i32 stay, i32 tick );
    bool                     recall_hop( i32 index, i32 &stay, i32 &tick ) const;

private:
    player_record_t       *player( i32 index );
    const player_record_t *player( i32 index ) const;

    void         sample_velocity( i32 index, C_TFPlayer *player );
    void         forget_velocity( i32 index );
    void         sample_vaccinator( i32 index, C_TFPlayer *player );
    void         forget_vaccinator( i32 index );
    void         remember( C_BaseEntity *entity, const vec3 &origin, bool manual = false );
    void         clear_dormant( i32 index );
    void         forget( i32 index );
    void         push_origin( i32 index, i32 serial, const vec3 &origin ) const;
    static bool  remembered_class( ETFClassID class_id );
    static float class_window( ETFClassID class_id );
    bool         resource_slot( i32 index ) const;

    player_record_t                            m_players[ MAX_TRACKED_PLAYERS ]{ };
    atl::unordered_map< i32, entity_record_t > m_entities{ };
};

extern player_data g_player_data;
