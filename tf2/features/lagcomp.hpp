#pragma once

#include "bone_setup.hpp"
#include "link.hpp"

constexpr i32 MAX_RECORDS{ 81 };
constexpr i32 RHYTHM_GAPS{ 4 };

ATLAS_FORCE_INLINE bool kept_while_reachable( i32 tick, float interval, float unlag )
{
    const i32   last{ tick + static_cast< i32 >( ( unlag + 0.2f ) / interval ) };
    const float deadtime{ static_cast< float >( static_cast< i32 >( interval * static_cast< float >( last - 1 ) - unlag ) ) };
    return interval * static_cast< float >( tick ) >= deadtime;
}

enum class warp_link_t : u8
{
    first,
    steady,
    burst,
    recharge,
    warp,
};

struct warp_read_t
{
    vec3        m_velocity{ };
    i32         m_commands{ };
    i32         m_banked{ };
    warp_link_t m_link{ warp_link_t::first };
};

struct warp_flow_t
{
    i32  m_newest_server{ -1 };
    i32  m_latest_server{ -1 };
    i32  m_rhythm{ 1 };
    i32  m_burst{ 1 };
    f32  m_rate{ 1.0f };
    i32  m_charge{ };
    f32  m_speed{ };
    bool m_loose{ };
};

struct lag_record_t
{
    pose_snapshot_t m_pose{ };
    matrix3x4       m_bones[ MAX_STUDIO_BONES ]{ };
    i32             m_bone_count{ };
    vec3            m_mins{ };
    vec3            m_maxs{ };
    i32             m_tick{ };
    i32             m_server_tick{ };
    vec3            m_velocity{ };
    float           m_maxspeed{ };
    float           m_eye_yaw{ };
    bool            m_on_ground{ };
    bool            m_ducked{ };
    bool            m_swimming{ };
    bool            m_parachuting{ };
    bool            m_wall{ };
    i32             m_gap{ }; // ticks since the record before it; a choker's burst cadence lives here
    i32             m_commands{ 1 };
    warp_link_t     m_link{ warp_link_t::first };

    const studiohdr_t *m_header{ };
};

struct loose_state_t
{
    vec3 m_origin{ };
    vec3 m_velocity{ };
    i32  m_server_tick{ -1 };
    i32  m_commands{ 1 };
    bool m_on_ground{ };
};

struct rewind_window_t
{
    i32   m_server_tick{ -1 };
    float m_interval{ };
    float m_correct{ };
    float m_unlag{ };

    float age( i32 tick ) const
    {
        return ( this->m_server_tick - tick ) * this->m_interval;
    }

    bool accepts( i32 tick ) const
    {
        const float age{ this->age( tick ) };
        const float delta{ this->m_correct - age };
        return this->m_server_tick >= 0 && age >= 0.0f &&
               ( age <= this->m_unlag || kept_while_reachable( tick, this->m_interval, this->m_unlag ) ) && delta >= -0.2f &&
               delta <= 0.2f;
    }
};

class lag_compensation
{
public:
    void update( );
    void clear( );
    void paint( ) const;

    i32                 count( i32 index ) const;
    i32                 cadence( i32 index ) const;
    bool                broken( i32 index ) const;
    const lag_record_t *record( i32 index, i32 age ) const;
    bool                continuous( i32 index, const lag_record_t &entry ) const;
    bool                hull_at( i32 index, i32 tick, vec3 &origin, vec3 &mins, vec3 &maxs ) const;
    bool                reachable( i32 index, const lag_record_t &entry, i32 ticks_ahead = 0, i32 hold = 0 ) const;
    rewind_window_t     rewind_window( i32 ticks_ahead = 0, i32 hold = 0 ) const;
    warp_flow_t         flow( i32 index ) const;
    const loose_state_t *loose( i32 index ) const;
    static i32          command_bank( );
    i32                 command_tick( i32 tick ) const;
    i32                 command_tick( const lag_record_t &entry ) const;

    void store_bones( C_TFPlayer *player, lag_record_t &entry ) const;
    bool build_bones( C_TFPlayer *player, const lag_record_t *entry, i32 bone_mask, matrix3x4 *out,
                      i32 max_bones ) const;

    static float lerp_time( );
    float        max_unlag( ) const;

private:
    struct track_t
    {
        lag_record_t *m_records{ };
        i32           m_head{ };
        i32           m_count{ };
        i32           m_last_tick{ };
        i32           m_gaps[ 4 ]{ };
        i32           m_gap_rotor{ };
        i32           m_break_tick{ };
        i32           m_server_gaps[ RHYTHM_GAPS ]{ };
        i32           m_server_rotor{ };
        i32           m_charge{ };
        i32           m_over_clock{ };
        loose_state_t m_loose{ };
    };

    C_TFPlayer *trackable( i32 index, bool tracked );
    float       teleport_distance_sqr( ) const;
    void        store( i32 index, C_TFPlayer *player );
    void        loosen( i32 index, C_TFPlayer *player, i32 tick, i32 server_tick );
    warp_read_t read_link( const track_t &track, const lag_record_t &newest, const vec3 &delta, i32 sim_gap,
                           i32 server_gap, float maxspeed, bool on_ground ) const;
    void        invalidate( i32 index );
    void        draw_records( C_TFPlayer *player, i32 index ) const;

    track_t m_tracks[ MAX_TRACKED_PLAYERS ]{ };
};

extern lag_compensation g_lagcomp;
