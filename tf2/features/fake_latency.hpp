#pragma once

#include "link.hpp"

struct packet_record_t
{
    i32    m_sequence{ -1 };
    i32    m_reliable_state{ };
    double m_time{ };
};

struct reliable_block_t
{
    u64  m_hash{ };
    i32  m_start{ };
    i32  m_num{ };
    i32  m_length{ };
    bool m_valid{ };

    bool operator==( const reliable_block_t &other ) const
    {
        return m_valid && other.m_valid && m_hash == other.m_hash && m_start == other.m_start &&
               m_num == other.m_num && m_length == other.m_length;
    }
};

struct presented_ack_t
{
    i32  m_sequence{ };
    bool m_faked{ };
};

class fake_latency
{
public:
    bool  active( ) const;
    float target( ) const;
    float applied( ) const;
    float reply_delay( CNetChannel *channel ) const;
    i32   scoreboard_latency( ) const;
    f32   calc_wish_latency( CNetChannel *netchan ) const;

    presented_ack_t present( CNetChannel *channel );
    void            restore( CNetChannel *channel, const presented_ack_t &presented, bool sent );

    void begin_packet( CNetChannel *channel, bool has_header );
    void end_packet( CNetChannel *channel, double received_time );

    bool filtering( CNetChannel *channel ) const;
    bool skip_block( CNetChannel *channel, bf_read &buffer, i32 stream );
    void accept_block( i32 stream );

    void clear( );

private:
    static constexpr usize RECORDS{ 128 };
    static constexpr usize SAMPLES{ 64 };

    struct block_peek_t
    {
        bool             m_present{ };
        i32              m_bits{ };
        reliable_block_t m_id{ };
    };

    struct subchannel_t
    {
        reliable_block_t m_stream[ MAX_STREAMS ]{ };
        i32              m_flip_sequence{ INT_MAX }; // our datagram that first carried the flipped bit
        i32              m_previous_sequence{ -1 };  // the datagram we sent right before that one
        i32              m_copies{ };                // copies skipped since the block was accepted
        bool             m_flip_pending{ };
    };

    bool                   synchronize( CNetChannel *channel );
    const packet_record_t *choose( double now, float target ) const;
    void                   push_sample( double delay );
    bool                   peek_block( CNetChannel *channel, bf_read &buffer, i32 stream, block_peek_t &out ) const;
    bool                   peek_packet( CNetChannel *channel, bf_read &buffer, i32 first_stream );
    bool                   is_copy( CNetChannel *channel, subchannel_t &state ) const;

    CNetChannel    *m_channel{ };
    packet_record_t m_records[ RECORDS ]{ };
    usize           m_record_head{ };
    usize           m_record_count{ };
    double          m_samples[ SAMPLES ]{ };
    usize           m_sample_next{ };
    usize           m_sample_count{ };
    double          m_sample_sum{ };
    i32             m_presented_sequence{ -1 };
    double          m_last_faked_time{ -1.0 };
    i32             m_sending_sequence{ -1 };
    i32             m_last_sent_sequence{ -1 };
    i32             m_scoreboard_latency{ };
    bool            m_honest_transfer{ };
    subchannel_t    m_subchannels[ MAX_SUBCHANNELS ]{ };

    i32          m_packet_index{ -1 };
    i32          m_reliable_before{ };
    i32          m_previous_ack{ -1 };
    bool         m_packet_open{ };
    bool         m_packet_header{ };
    bool         m_packet_dup{ };
    block_peek_t m_peek[ MAX_STREAMS ]{ };

#if defined( ATLAS_DEV )
    i32    m_filtered{ };
    i32    m_undone{ };
    i32    m_accepted{ };
    i32    m_filtered_at_log{ };
    double m_log_time{ };
#endif
};

extern fake_latency g_fake_latency;
