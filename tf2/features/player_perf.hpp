#pragma once

#include "link.hpp"

constexpr i32 PERF_MAX_RECORDS{ 32 };

struct perf_record_t
{
    double m_time{ };
    i32    m_num_cmds{ };
    i32    m_dropped{ };
};

struct perf_frame_t
{
    double m_time{ };
    i32    m_num_cmds{ };
    i32    m_ticks_corrected{ };
    double m_final_simulation{ };
};

struct perf_dump_t
{
    perf_frame_t  m_frames[ PERF_MAX_RECORDS ]{ };
    perf_record_t m_packets[ PERF_MAX_RECORDS ]{ };
    i32           m_frame_count{ };
    i32           m_packet_count{ };
};

enum class perf_owner_t : u8
{
    nospread,
    watchdog,
    networking,
};

class player_perf
{
public:
    bool request( perf_owner_t owner, i32 index, i32 records );
    bool on_text_message( bf_read &buffer );
    void on_packet( );
    void clear( );

private:
    perf_dump_t  m_dump{ };
    double       m_sent{ };
    i32          m_index{ };
    perf_owner_t m_owner{ };
    bool         m_in_flight{ };
};

extern player_perf g_player_perf;
