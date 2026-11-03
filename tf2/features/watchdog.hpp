#pragma once

#include "link.hpp"

struct perf_dump_t;

class watchdog
{
public:
    void on_network_update( );
    void on_fire_bullets( i32 shooter );
    void on_perf( i32 index, const perf_dump_t &dump );
    void clear( );
    const char *known_cheater_name( i32 index, u32 account, bool hidden );

private:
    enum class detection_t : u8
    {
        invalid_pitch,
        packet_choke,
        command_overflow,
        tickbase_shift,
        double_tap,
        silent_aim,
        equip_regions,
        _COUNT,
    };

    struct record_t
    {
        u32    m_handle{ INVALID_EHANDLE_INDEX };
        u32    m_account{ };
        char   m_name[ MAX_PLAYER_NAME_LENGTH ]{ };
        float  m_score{ };
        double m_score_time{ };
        u16    m_hits[ static_cast< usize >( detection_t::_COUNT ) ]{ };
        bool   m_eligible{ };
        bool   m_flagged{ };
        bool   m_report_pending{ };

        bool m_tracked{ };
        i32  m_sim_tick{ };
        i32  m_changed_tick{ };
        i32  m_seen_tick{ };
        i32  m_choke_streak{ };
        i32  m_pitch_streak{ };

        vec3 m_eye{ };
        vec3 m_previous_eye{ };
        i32  m_gap{ };
        i32  m_shots{ };
        bool m_shot{ };

        bool   m_probe_wanted{ };
        double m_probe_time{ };
        double m_perf_seen{ };

        i32  m_region_tick{ };
        bool m_region_seen{ };
    };

    void adopt( record_t &record, i32 index, u32 handle );
    void observe( record_t &record, i32 index, C_TFPlayer *player, i32 server_tick );
    void infract( record_t &record, i32 index, detection_t detection, float weight );
    void flag( record_t &record, i32 index, detection_t detection );
    void probe( );
    void report( );

    static const char *detection_name( detection_t detection );
    static bool        conclusive( detection_t detection );

    record_t m_records[ MAX_TRACKED_PLAYERS ]{ };
    double   m_next_probe{ };
};

extern watchdog g_watchdog;
