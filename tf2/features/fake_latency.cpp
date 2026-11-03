#include "fake_latency.hpp"

#include "gui/gui.hpp"

#include "lagcomp.hpp"
#include "ragebot.hpp"
#include "tickbase.hpp"

ATLAS_NO_DESTROY fake_latency g_fake_latency{ };

constexpr i32    RING_ROOM{ 58 };
constexpr double FILTER_GRACE{ 1.5 };
constexpr i32    MAX_BLOCK_BYTES{ 65536 };
constexpr i32    COPY_LIMIT{ 32 };
constexpr u32    FRAGMENT_BYTES{ static_cast< u32 >( FRAGMENT_SIZE ) };
constexpr u64    FNV_BASIS{ 0xcbf29ce484222325ull };
constexpr u64    FNV_PRIME{ 0x100000001b3ull };

void fake_latency::clear( )
{
    *this = fake_latency{ };
}

bool fake_latency::synchronize( CNetChannel *channel )
{
    if ( channel != this->m_channel )
    {
        this->clear( );
        this->m_channel = channel;
    }

    return channel != nullptr;
}

bool fake_latency::active( ) const
{
    const config::exploits_t &cfg{ g_cfg.m_exploits };

    if ( !cfg.m_fake_latency || cfg.m_fake_latency_ms <= 0 )
    {
        return false;
    }

    if ( !g_tf2.m_cl->IsActive( ) ||
         !g_tf2.m_cl->m_NetChannel ||
         g_tf2.m_demo_player->IsPlayingBack( ) )
    {
        return false;
    }

    if ( !g_cl.m_local || !g_cl.m_local->IsAlive( ) || !g_cl.m_weapon )
    {
        return false;
    }

    return true; // NOTE: Hoku; Removed the check for this since keeping fake latency on has no real consequence for us.
}

float fake_latency::target( ) const
{
    if ( !this->active( ) )
    {
        return 0.0f;
    }

    CNetChannel *channel{ g_tf2.m_cl->m_NetChannel };

    const auto wish{ this->calc_wish_latency( channel ) };
    const f32  lerp{ TICKS_TO_TIME( TIME_TO_TICKS( lag_compensation::lerp_time( ) ) ) };
    const f32  unlag_room{ g_tf2.sv_maxunlag->GetFloat( ) - channel->GetLatency( FLOW_OUTGOING ) - lerp - 2.0f * TICK_INTERVAL };

    // the frame we point at has to still sit in the server's ring when the ack lands
    f32       updaterate{ g_tf2.cl_updaterate->GetFloat( ) };
    const f32 max_updaterate{ g_tf2.sv_maxupdaterate->GetFloat( ) };
    if ( max_updaterate > 0.0f )
    {
        updaterate = math::smaller( updaterate, max_updaterate );
    }

    const auto ring_room{ static_cast< f32 >( RING_ROOM ) / math::bigger( updaterate, 1.0f ) };

    return math::clamp( wish, 0.0f, math::smaller( unlag_room, ring_room ) );
}

float fake_latency::applied( ) const
{
    if ( !this->m_sample_count )
    {
        return 0.0f;
    }

    return static_cast< float >( this->m_sample_sum / static_cast< double >( this->m_sample_count ) );
}

float fake_latency::reply_delay( CNetChannel *channel ) const
{
    return math::bigger( channel->GetLatency( FLOW_INCOMING ) - this->applied( ), 0.0f );
}

i32 fake_latency::scoreboard_latency( ) const
{
    return this->m_scoreboard_latency;
}

f32 fake_latency::calc_wish_latency( CNetChannel *netchan ) const
{
    Assert( netchan != nullptr );

    auto     &exploits{ g_cfg.m_exploits };
    const f32 latency{ netchan->GetLatency( FLOW_INCOMING ) };
    if ( !gui::bind_active( exploits.m_fake_latency_key ) )
    {
        // The math for this could be done better...
        return exploits.m_extended_backtrack ? math::bigger( 0.2f, latency ) : latency;
    }

    return ( f32 )exploits.m_fake_latency_ms * 0.001f;
}

void fake_latency::push_sample( double delay )
{
    if ( this->m_sample_count == SAMPLES )
    {
        this->m_sample_sum -= this->m_samples[ this->m_sample_next ];
    } else
    {
        ++this->m_sample_count;
    }

    this->m_samples[ this->m_sample_next ] = delay;
    this->m_sample_sum += delay;
    this->m_sample_next = ( this->m_sample_next + 1 ) % SAMPLES;

    // the running sum drifts over hours of adds and subtracts; a full pass every lap resets it
    if ( this->m_sample_next == 0 )
    {
        this->m_sample_sum = 0.0;
        for ( usize index{ }; index < this->m_sample_count; ++index )
        {
            this->m_sample_sum += this->m_samples[ index ];
        }
    }
}

const packet_record_t *fake_latency::choose( double now, float target ) const
{
    if ( !this->m_record_count )
    {
        return nullptr;
    }

    const packet_record_t &newest{ this->m_records[ this->m_record_head ] };
    const packet_record_t *chosen{ };

    for ( usize age{ }; age < this->m_record_count; ++age )
    {
        const packet_record_t &record{ this->m_records[ ( this->m_record_head + RECORDS - age ) % RECORDS ] };

        if ( newest.m_sequence - record.m_sequence > RING_ROOM )
        {
            break;
        }

        if ( record.m_sequence < this->m_presented_sequence )
        {
            break;
        }

        chosen = &record;

        if ( now - record.m_time >= target )
        {
            break;
        }
    }

    return chosen;
}

ATLAS_FORCE_INLINE i32 read_scoreboard_latency( )
{
    if ( !g_cl.m_resource || !g_cl.m_local )
    {
        return 0;
    }

    const i32 index{ g_cl.m_local->entindex( ) };
    if ( index < 1 || index >= MAX_TRACKED_PLAYERS )
    {
        return 0;
    }

    return g_cl.m_resource->m_iPing( )[ index ];
}

presented_ack_t fake_latency::present( CNetChannel *channel )
{
    presented_ack_t real{ channel->m_nInSequenceNr, false };

    if ( !this->synchronize( channel ) )
    {
        return real;
    }

    this->m_sending_sequence   = channel->m_nOutSequenceNr;
    this->m_scoreboard_latency = read_scoreboard_latency( );

    const double now{ tick_manager::packet_time( channel->IsLoopback( ) ) };
    const float  target{ this->target( ) };

    const bool transfer{ channel->m_ReceiveList[ 0 ].buffer != nullptr || channel->m_ReceiveList[ 1 ].buffer != nullptr };
#if defined( ATLAS_DEV )
    if ( transfer != this->m_honest_transfer )
    {
        DBGOUT( "fake latency: %s", transfer ? "transfer in progress, acknowledging honestly" : "transfer done" );
    }
#endif
    this->m_honest_transfer = transfer;

    const packet_record_t *record{ target > 0.0f && !transfer ? this->choose( now, target ) : nullptr };

    if ( !record )
    {
        this->m_presented_sequence = -1;
        this->push_sample( 0.0 );
        return real;
    }

    this->m_presented_sequence = record->m_sequence;
    this->m_last_faked_time    = now;
    this->push_sample( now - record->m_time );

    channel->m_nInSequenceNr = record->m_sequence + NET_FRAMES_BACKUP;
    real.m_faked             = true;

#if defined( ATLAS_DEV )
    if ( now - this->m_log_time >= 1.0 )
    {
        this->m_log_time = now;
        DBGOUT( "fake latency: target %d ms applied %d ms record age %d ms seq %d accepted %d filtered %d (+%d) undone %d scoreboard %d ms",
                static_cast< i32 >( target * 1000.0f ), static_cast< i32 >( this->applied( ) * 1000.0f ),
                static_cast< i32 >( ( now - record->m_time ) * 1000.0 ), record->m_sequence, this->m_accepted,
                this->m_filtered, this->m_filtered - this->m_filtered_at_log, this->m_undone,
                this->scoreboard_latency( ) );
        this->m_filtered_at_log = this->m_filtered;
    }
#endif

    return real;
}

void fake_latency::restore( CNetChannel *channel, const presented_ack_t &presented, bool sent )
{
    if ( presented.m_faked )
    {
        channel->m_nInSequenceNr = presented.m_sequence;
    }

    if ( !sent )
    {
        return;
    }

    for ( subchannel_t &state : this->m_subchannels )
    {
        if ( state.m_flip_pending )
        {
            state.m_flip_pending      = false;
            state.m_flip_sequence     = this->m_sending_sequence;
            state.m_previous_sequence = this->m_last_sent_sequence;
        }
    }

    this->m_last_sent_sequence = this->m_sending_sequence;
}

void fake_latency::begin_packet( CNetChannel *channel, bool has_header )
{
    this->synchronize( channel );

    this->m_packet_header   = has_header;
    this->m_packet_open     = true;
    this->m_packet_dup      = false;
    this->m_packet_index    = -1;
    this->m_reliable_before = channel->m_nInReliableState;
    this->m_previous_ack    = channel->m_nOutSequenceNrAck;

    for ( block_peek_t &peek : this->m_peek )
    {
        peek = block_peek_t{ };
    }
}

void fake_latency::end_packet( CNetChannel *channel, double received_time )
{
    this->m_packet_open = false;

    if ( this->m_packet_dup && this->m_packet_index >= 0 )
    {
        if ( channel->m_nInReliableState == ( this->m_reliable_before ^ ( 1 << this->m_packet_index ) ) )
        {
            channel->m_nInReliableState = this->m_reliable_before;
#if defined( ATLAS_DEV )
            ++this->m_undone;
#endif
        } else
        {
            DBGOUT( "fake latency: copy on sub %d left the reliable state at %d, expected %d flipped",
                    this->m_packet_index, channel->m_nInReliableState, this->m_reliable_before );
        }
    }

    this->m_packet_dup = false;

    if ( !this->m_packet_header )
    {
        return;
    }

    const i32 sequence{ channel->m_nInSequenceNr };
    if ( this->m_record_count && this->m_records[ this->m_record_head ].m_sequence >= sequence )
    {
        return;
    }

    if ( this->m_record_count )
    {
        this->m_record_head = ( this->m_record_head + 1 ) % RECORDS;
    }

    this->m_record_count                   = math::smaller( this->m_record_count + 1, RECORDS );
    this->m_records[ this->m_record_head ] = packet_record_t{ sequence, channel->m_nInReliableState, received_time };
}

bool fake_latency::filtering( CNetChannel *channel ) const
{
    if ( channel != this->m_channel || this->m_last_faked_time < 0.0 )
    {
        return false;
    }

    return tick_manager::packet_time( channel->IsLoopback( ) ) - this->m_last_faked_time < FILTER_GRACE;
}

bool fake_latency::peek_block( CNetChannel *channel, bf_read &buffer, i32 stream, block_peek_t &out ) const
{
    const i32  begin{ buffer.m_iCurBit };
    const bool single{ buffer.ReadOneBit( ) == 0 };

    i32 start{ };
    i32 num{ };
    if ( !single )
    {
        start = static_cast< i32 >( buffer.ReadUBitLong( 18 ) );
        num   = static_cast< i32 >( buffer.ReadUBitLong( 3 ) );
    }

    u32 bytes{ };
    i32 fragments{ };
    if ( single || start == 0 )
    {
        if ( single )
        {
            if ( buffer.ReadOneBit( ) )
            {
                buffer.ReadUBitLong( 26 ); // uncompressed size
            }

            bytes = buffer.ReadVarInt32( );
        } else
        {
            if ( buffer.ReadOneBit( ) )
            {
                buffer.ReadUBitLong( 32 ); // transfer id
                char filename[ 260 ]{ };
                buffer.ReadString( filename, sizeof( filename ) );
            }

            if ( buffer.ReadOneBit( ) )
            {
                buffer.ReadUBitLong( 26 ); // uncompressed size
            }

            bytes = buffer.ReadUBitLong( 26 );
        }

        fragments = static_cast< i32 >( ( bytes + FRAGMENT_BYTES - 1u ) / FRAGMENT_BYTES );
        if ( single )
        {
            start = 0;
            num   = fragments;
        }
    } else
    {
        const dataFragments_t &transfer{ channel->m_ReceiveList[ stream ] };
        bytes     = transfer.bytes;
        fragments = transfer.numFragments;
    }

    i32 length{ num * FRAGMENT_SIZE };
    if ( start + num == fragments )
    {
        const i32 rest{ FRAGMENT_SIZE - static_cast< i32 >( bytes % FRAGMENT_BYTES ) };
        if ( rest < FRAGMENT_SIZE )
        {
            length -= rest;
        }
    }

    if ( buffer.m_bOverflow || length <= 0 || length > MAX_BLOCK_BYTES )
    {
        return false;
    }

    u64 hash{ FNV_BASIS };
    for ( i32 byte{ }; byte < length; ++byte )
    {
        hash ^= static_cast< u64 >( buffer.ReadUBitLong( 8 ) );
        hash *= FNV_PRIME;
    }

    if ( buffer.m_bOverflow )
    {
        return false;
    }

    out.m_present = true;
    out.m_bits    = buffer.m_iCurBit - begin;
    out.m_id      = reliable_block_t{ hash, start, num, length, true };
    return true;
}

bool fake_latency::is_copy( CNetChannel *channel, subchannel_t &state ) const
{
    const i32 ack{ channel->m_nOutSequenceNrAck };

    // sent before the server had read the datagram carrying our flip
    if ( ack < state.m_flip_sequence )
    {
        return true;
    }

    // the resend queued by the datagram before our flip, flushed in the first packet after the server
    // read both; the subchannel was not free at that point, so the packet cannot hold a new send
    const bool crossing{ this->m_previous_ack < state.m_flip_sequence && state.m_flip_sequence <= ack };
    const bool queued{ this->m_previous_ack < state.m_previous_sequence };
    if ( crossing && queued )
    {
        return true;
    }

    DBGOUT( "fake latency: same block on sub %d taken as new: in %d ack %d flip %d previous %d prev_ack %d",
            this->m_packet_index, channel->m_nInSequenceNr, ack, state.m_flip_sequence, state.m_previous_sequence,
            this->m_previous_ack );
    return false;
}

bool fake_latency::peek_packet( CNetChannel *channel, bf_read &buffer, i32 first_stream )
{
    const i32 saved{ buffer.m_iCurBit };
    bool      identical{ true };
    bool      present{ };

    subchannel_t &state{ this->m_subchannels[ this->m_packet_index ] };

    for ( i32 stream{ first_stream }; stream < MAX_STREAMS; ++stream )
    {
        if ( stream != first_stream && !buffer.ReadOneBit( ) )
        {
            continue;
        }

        block_peek_t &peek{ this->m_peek[ stream ] };
        if ( !this->peek_block( channel, buffer, stream, peek ) )
        {
            identical = false;
            break;
        }

        present   = true;
        identical = identical && peek.m_id == state.m_stream[ stream ];
    }

    // nothing was consumed, and an overflow while looking ahead is the engine's to find on its own
    buffer.m_iCurBit   = saved;
    buffer.m_bOverflow = false;

    if ( !present || !identical || !this->is_copy( channel, state ) )
    {
        return false;
    }

    if ( ++state.m_copies > COPY_LIMIT )
    {
        DBGOUT( "fake latency: %d copies on sub %d and no flip reached the server, accepting the next one",
                state.m_copies, this->m_packet_index );
        state.m_copies = 0;
        return false;
    }

    return true;
}

bool fake_latency::skip_block( CNetChannel *channel, bf_read &buffer, i32 stream )
{
    if ( this->m_packet_open )
    {
        this->m_packet_open = false;

        // ProcessPacket read the three index bits and one presence bit per stream before handing us
        // this stream, so on the first call of a packet the index sits just behind the cursor
        const i32 saved{ buffer.m_iCurBit };
        buffer.Seek( saved - 3 - ( stream + 1 ) );
        this->m_packet_index = static_cast< i32 >( buffer.ReadUBitLong( 3 ) );
        buffer.Seek( saved );

        this->m_packet_dup = this->peek_packet( channel, buffer, stream );
    }

    if ( !this->m_packet_dup || !this->m_peek[ stream ].m_present )
    {
        return false;
    }

    buffer.Seek( buffer.m_iCurBit + this->m_peek[ stream ].m_bits );
#if defined( ATLAS_DEV )
    ++this->m_filtered;
#endif
    return true;
}

void fake_latency::accept_block( i32 stream )
{
    if ( this->m_packet_index < 0 || !this->m_peek[ stream ].m_present )
    {
        return;
    }

    subchannel_t &state{ this->m_subchannels[ this->m_packet_index ] };

    state.m_stream[ stream ]  = this->m_peek[ stream ].m_id;
    state.m_flip_pending      = true;
    state.m_flip_sequence     = INT_MAX;
    state.m_previous_sequence = -1;
    state.m_copies            = 0;

#if defined( ATLAS_DEV )
    ++this->m_accepted;
#endif
}
