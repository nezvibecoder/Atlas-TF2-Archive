#include "player_perf.hpp"

#include "networking.hpp"
#include "nospread.hpp"
#include "watchdog.hpp"

ATLAS_NO_DESTROY player_perf g_player_perf{ };

constexpr double PERF_TIMEOUT{ 2.0 };

void player_perf::clear( )
{
    this->m_in_flight = false;
}

bool player_perf::request( perf_owner_t owner, i32 index, i32 records )
{
    if ( this->m_in_flight )
    {
        return false;
    }

    char command[ 32 ]{ };
    str::format_to( command, sizeof( command ), ATLAS_XORSTR( "playerperf {} {}" ).get( ), index,
                    records );

    NET_StringCmd message{ command };
    if ( !g_tf2.m_cl->m_NetChannel->SendNetMsg( message, true, false ) )
    {
        return false;
    }

    this->m_dump      = perf_dump_t{ };
    this->m_sent      = g_tf2.m_engine->Plat_FloatTime( );
    this->m_index     = index;
    this->m_owner     = owner;
    this->m_in_flight = true;
    return true;
}

bool player_perf::on_text_message( bf_read &buffer )
{
    if ( !this->m_in_flight )
    {
        return false;
    }

    bf_read reader{ buffer };
    if ( reader.ReadUBitLong( 8 ) != 2 )
    {
        return false;
    }

    perf_dump_t &dump{ this->m_dump };
    bool         recognised{ };
    char         line[ 96 ]{ };

    while ( reader.ReadString( line, sizeof( line ), true ) > 0 )
    {
        perf_record_t packet{ };
        perf_frame_t  frame{ };
        double        game_simulation{ };
        double        velocity{ };

        if ( str::scan( line, ATLAS_XORSTR( "{} {} {}" ).get( ), packet.m_time, packet.m_num_cmds,
                        packet.m_dropped ) )
        {
            if ( dump.m_packet_count < PERF_MAX_RECORDS )
            {
                dump.m_packets[ dump.m_packet_count++ ] = packet;
            }
        } else if ( str::scan( line, ATLAS_XORSTR( "{} {} {} {} {} vel {}" ).get( ), frame.m_time,
                               frame.m_num_cmds, frame.m_ticks_corrected, frame.m_final_simulation,
                               game_simulation, velocity ) )
        {
            if ( dump.m_frame_count < PERF_MAX_RECORDS )
            {
                dump.m_frames[ dump.m_frame_count++ ] = frame;
            }
        } else
        {
            break;
        }

        recognised = true;
    }

    return recognised;
}

void player_perf::on_packet( )
{
    if ( !this->m_in_flight )
    {
        return;
    }

    if ( !this->m_dump.m_packet_count )
    {
        if ( g_tf2.m_engine->Plat_FloatTime( ) - this->m_sent > PERF_TIMEOUT )
        {
            this->m_in_flight = false;
        }

        return;
    }

    this->m_in_flight = false;

    if ( this->m_index == g_tf2.m_engine->GetLocalPlayer( ) )
    {
        g_networking.on_own_perf( this->m_dump );
    }

    switch ( this->m_owner )
    {
        case perf_owner_t::nospread: g_nospread.on_perf( this->m_dump ); break;
        case perf_owner_t::watchdog: g_watchdog.on_perf( this->m_index, this->m_dump ); break;
        case perf_owner_t::networking: break;
    }
}
