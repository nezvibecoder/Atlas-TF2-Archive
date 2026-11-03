#pragma once

#include "link.hpp"

struct perf_dump_t;

using send_net_msg_fn = bool ( ATLAS_THISCALL * )( CNetChannel *, INetMessage &, bool, bool );

struct sent_packet_t
{
    i32 m_sequence{ -1 };
    i32 m_command{ };
    double m_time{ };
    bool m_movement{ };
    bool   m_acked{ };
};

struct packet_plan_t
{
    i32 m_advance{ };
    i32 m_choked{ };
};

struct send_delay_t
{
    i32 m_psilent_ticks{ };
    i32 m_ack_ticks{ };
    i32 m_rate_ticks{ };

    constexpr i32 ticks( ) const
    {
        return math::bigger( math::bigger( this->m_psilent_ticks, this->m_ack_ticks ), this->m_rate_ticks );
    }

    static constexpr i32 remaining_ticks( double seconds, double interval )
    {
        if ( seconds <= 0.0 )
        {
            return 0;
        }
        const double ticks{ seconds / interval };
        const i32 whole{ static_cast< i32 >( ticks ) };
        return whole + ( ticks > whole ? 1 : 0 );
    }
};

enum class move_failure_t
{
    none,
    disconnected,
    host_stopped,
    control_wait,
    demo_playback,
    command_limit,
    create_move_missing,
    channel_changed,
    deferred,
    encode,
    move_message,
    tick_message,
    datagram,
};

struct move_result_t
{
    bool           m_generated{ };
    bool           m_sent{ };
    i32            m_sequence{ -1 };
    i32            m_commands{ };
    move_failure_t m_failure{ move_failure_t::none };
};

class packet_history
{
public:
    static constexpr usize CAPACITY{ 256 };

    constexpr void reset( i32 sequence, i32 command, i32 acknowledged )
    {
        for ( sent_packet_t &packet : this->m_sent )
        {
            packet = sent_packet_t{ };
        }

        this->m_next          = 0;
        this->m_last_sequence = sequence;
        this->m_move_sequence = sequence;
        this->m_last_command  = command;
        this->m_acknowledged  = acknowledged;
        this->m_ack_sequence  = -1;
        this->m_last_control  = false;
        this->m_clamps_drop   = true;
    }

    constexpr packet_plan_t plan( i32 sequence, i32 extra, bool movement ) const
    {
        if ( !movement )
        {
            return packet_plan_t{ this->m_last_sequence + 1 - sequence, 0 };
        }

        const i32 base{ extra == 0 && this->m_clamps_drop ? this->known_sequence( ) : this->m_last_sequence };
        const i32 gap{ sequence - base - 1 };
        return gap < extra ? packet_plan_t{ extra - gap, 0 }
                           : packet_plan_t{ 0, gap - extra };
    }

    constexpr void record( i32 sequence, i32 command, bool movement, double time = 0.0 )
    {
        this->m_sent[ this->m_next ] = sent_packet_t{ sequence, command, time, movement };
        this->m_next = ( this->m_next + 1 ) % CAPACITY;
        this->m_last_sequence = sequence;
        this->m_move_sequence        = movement ? sequence : this->m_move_sequence;
        this->m_last_command  = math::bigger( this->m_last_command, command );
        this->m_last_control  = !movement;
    }

    constexpr const sent_packet_t *find( i32 sequence ) const
    {
        for ( const sent_packet_t &packet : this->m_sent )
        {
            if ( packet.m_sequence == sequence && packet.m_sequence >= 0 )
            {
                return &packet;
            }
        }
        return nullptr;
    }

    constexpr i32 acknowledge( i32 sequence )
    {
        for ( sent_packet_t &packet : this->m_sent )
        {
            if ( packet.m_sequence == sequence && packet.m_sequence >= 0 )
            {
                packet.m_acked       = true;
                this->m_acknowledged = math::bigger( this->m_acknowledged, packet.m_command );
                this->m_ack_sequence = math::bigger( this->m_ack_sequence, sequence );
                break;
            }
        }

        return this->m_acknowledged;
    }

    constexpr bool acked_between( i32 first, i32 last ) const
    {
        for ( const sent_packet_t &packet : this->m_sent )
        {
            if ( packet.m_acked && packet.m_sequence >= first && packet.m_sequence <= last )
            {
                return true;
            }
        }
        return false;
    }

    constexpr i32 known_sequence( ) const
    {
        return math::bigger( this->m_ack_sequence, this->m_move_sequence );
    }

    constexpr bool can_keep_alive( ) const
    {
        return this->m_acknowledged >= this->m_last_command;
    }

    constexpr bool can_send_move( i32 extra ) const
    {
        return ( extra <= 0 && this->m_clamps_drop ) || !this->m_last_control ||
               this->m_ack_sequence == this->m_last_sequence;
    }

    constexpr void refuse_negative_drop( )
    {
        this->m_clamps_drop = false;
    }

    constexpr bool clamps_drop( ) const
    {
        return this->m_clamps_drop;
    }

    constexpr bool can_run_move( i32 held ) const
    {
        return held < MAX_NEW_COMMANDS + MAX_BACKUP_COMMANDS - 1 || this->can_send_move( held + 1 - MAX_NEW_COMMANDS );
    }

    constexpr i32 last_sequence( ) const
    {
        return this->m_last_sequence;
    }

    constexpr i32 last_command( ) const
    {
        return this->m_last_command;
    }

    constexpr i32 last_acknowledged( ) const
    {
        return this->m_acknowledged;
    }

    constexpr i32 acknowledged_sequence( ) const
    {
        return this->m_ack_sequence;
    }

private:
    sent_packet_t m_sent[ CAPACITY ]{ };
    usize         m_next{ };
    i32           m_last_sequence{ -1 };
    i32           m_move_sequence{ -1 };
    i32           m_last_command{ };
    i32           m_acknowledged{ };
    i32           m_ack_sequence{ -1 };
    bool          m_last_control{ };
    bool          m_clamps_drop{ true };
};

class networking
{
private:
    move_failure_t cl_sendmove( i32 command, i32 commands );
    bool send_datagram( i32 command, i32 extra, bool movement );
    bool send_tick( );
    bool synchronize( );
    bool send_keep_alive( );
    double         ack_budget( ) const;
    void           probe_drop_clamp( );

    globals_backup_t m_state{ };
    packet_history  m_packets{ };
    CNetChannel    *m_channel{ };
    i32             m_keep_frame{ -1 };
    bool            m_warned_timeout{ };
    bool            m_control_waiting{ };
    bool            m_move_pending{ };
    i32             m_control_retries{ };
    double          m_last_send_time{ };
    i32             m_created_sequence{ -1 };
    i32             m_sent_reliable_state{ -1 };
    i32              m_probe_sequence{ -1 };
    i32              m_probe_first{ -1 };
    i32              m_probe_last{ -1 };
    bool             m_probed{ };
    CNetChannel    *m_reported_channel{ };
    i32             m_reported_cmdrate{ -1 };

public:
    move_result_t cl_move( float accumulated_extra_samples, bool final_tick, bool generate = true );
    void command_created( i32 sequence ) { this->m_created_sequence = sequence; }
    bool          keep_alive( i32 batch );
    void          on_choked( );
    bool can_send_now( ) const;
    i32  sequence_number( ) const;
    send_delay_t send_delay( i32 minimum_hold = 0 ) const;
    void remove_input_delay( bool final_tick );
    void report_cmdrate( );
    void send_reported_cmdrate( CNetChannel *channel, bool force );
    bool should_read_packets( );
    bool          ready_for_move( i32 extra );
    bool can_run_move( );
    void gate_send( );
    bool wait_for_control( bool final_tick );
    i32  acknowledged( i32 sequence );
    void          on_own_perf( const perf_dump_t &dump );
    void clear( );

    const sent_packet_t *sent( i32 sequence ) const
    {
        return this->m_packets.find( sequence );
    }

    i32 last_sent_command( ) const
    {
        return this->m_packets.last_command( );
    }
};

extern networking g_networking;
