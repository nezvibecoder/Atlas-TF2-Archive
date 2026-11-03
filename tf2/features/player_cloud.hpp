#pragma once

#include "runtime/mutex.hpp"
#include "runtime/vector.hpp"

#include "link.hpp"

class player_cloud
{
public:
    enum class report_result_t : u8
    {
        started,
        no_session,
        busy,
        invalid,
        unavailable,
    };

    void poll( );
    void on_level_shutdown( );
    bool known( u32 account, char ( &alias )[ 129 ] ) const;
    report_result_t submit_report( u32 account, string_view name, string_view reason,
                                   string_view alias );

private:
    enum class operation_t : u8
    {
        list,
        report,
    };

    struct known_t
    {
        u32  m_account{ };
        char m_alias[ 129 ]{ };
    };

    struct report_t
    {
        u32  m_account{ };
        char m_name[ 64 ]{ };
        char m_reason[ 256 ]{ };
        char m_alias[ 33 ]{ };
    };

    static void ATLAS_STDCALL worker( void *context );
    bool start( operation_t operation );
    bool parse_list( );
    void scan_players( );

    mutable atl::mutex   m_publish{ };
    atl::vector< known_t > m_known{ };
    atl::vector< char >    m_response{ };
    report_t               m_report{ };
    volatile long          m_state{ };
    u32                    m_status{ };
    u32                    m_notified[ MAX_TRACKED_PLAYERS ]{ };
    u64                    m_next_fetch{ };
    u64                    m_next_scan{ };
    operation_t            m_operation{ operation_t::list };
    bool                   m_success{ };
    bool                   m_loaded{ };
    bool                   m_in_game{ };
};

extern player_cloud g_player_cloud;
