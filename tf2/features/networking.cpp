#include "networking.hpp"

#include "anti_aim.hpp"
#include "fake_latency.hpp"
#include "nospread.hpp"
#include "player_perf.hpp"
#include "tickbase.hpp"

#include "gui/notifications.hpp"

ATLAS_NO_DESTROY networking g_networking{ };

void networking::clear( )
{
    g_nospread.clear( );
    if ( this->m_channel )
    {
        DBGOUT( "net reset: seq=%d cmd=%d ack=%d", this->m_packets.last_sequence( ),
                this->m_packets.last_command( ), this->m_packets.last_acknowledged( ) );
    }
    this->m_packets.reset( -1, 0, 0 );
    this->m_channel             = nullptr;
    this->m_keep_frame          = -1;
    this->m_warned_timeout      = false;
    this->m_control_waiting     = false;
    this->m_move_pending        = false;
    this->m_control_retries     = 0;
    this->m_last_send_time      = 0.0;
    this->m_created_sequence    = -1;
    this->m_sent_reliable_state = -1;
    this->m_probe_sequence      = -1;
    this->m_probe_first         = -1;
    this->m_probe_last          = -1;
    this->m_probed              = false;

    this->m_reported_channel = nullptr;
    this->m_reported_cmdrate = -1;
}

bool networking::synchronize( )
{
    if ( !g_tf2.m_cl->IsActive( ) || !g_tf2.m_cl->m_NetChannel )
    {
        this->clear( );
        return false;
    }

    CNetChannel *channel{ g_tf2.m_cl->m_NetChannel };
    const i32    last_sent{ channel->m_nOutSequenceNr - channel->m_nChokedPackets - 1 };

    if ( this->m_channel != channel || last_sent < this->m_packets.last_sequence( ) )
    {
        this->clear( );
        this->m_channel = channel;
        this->m_packets.reset( last_sent, g_tf2.m_cl->lastoutgoingcommand,
                               g_tf2.m_cl->last_command_ack );
    } else if ( last_sent > this->m_packets.last_sequence( ) )
    {
        this->m_packets.record( last_sent, this->m_packets.last_acknowledged( ), false );
        this->m_last_send_time = *g_tf2.m_net_time.get_as< const double * >( );
    }

    return true;
}

void networking::report_cmdrate( )
{
    if ( !g_cfg.m_exploits.m_ping_reducer && this->m_reported_cmdrate < 0 )
    {
        return;
    }

    if ( !g_tf2.m_cl->IsActive( ) || !g_tf2.m_cl->m_NetChannel )
    {
        return;
    }

    if ( this->m_channel != g_tf2.m_cl->m_NetChannel && !this->synchronize( ) )
    {
        return;
    }

    this->send_reported_cmdrate( this->m_channel, false );
}

void networking::send_reported_cmdrate( CNetChannel *channel, bool force )
{
    if ( channel != this->m_reported_channel )
    {
        this->m_reported_channel = channel;
        this->m_reported_cmdrate = -1;
    }

    const bool enabled{ g_cfg.m_exploits.m_ping_reducer };
    const i32  actual{ g_tf2.cl_cmdrate->GetInt( ) };
    const i32  target{ enabled ? g_cfg.m_exploits.m_ping_reducer_cmdrate : actual };
    if ( !force && target == this->m_reported_cmdrate )
    {
        return;
    }

    if ( !enabled && this->m_reported_cmdrate < 0 )
    {
        this->m_reported_cmdrate = actual;
        return;
    }

    char value[ 12 ]{ };
    str::format_to( value, sizeof( value ), ATLAS_XORSTR_STATIC( "{}" ), target );
    NET_SetConVar message{ ATLAS_XORSTR_STATIC( "cl_cmdrate" ), value };
    if ( g_hooks.m_net_chan_send_net_msg.thiscall< bool >( channel, message, true, false ) )
    {
        this->m_reported_cmdrate = target;
    }
}

i32 networking::acknowledged( i32 sequence )
{
    if ( !this->m_channel || !g_tf2.m_cl->IsActive( ) ||
         this->m_channel != g_tf2.m_cl->m_NetChannel )
    {
        return sequence;
    }

    this->synchronize( );
    return this->m_packets.acknowledge( sequence );
}

bool networking::send_datagram( i32 command, i32 extra, bool movement )
{
    if ( movement && !this->m_packets.can_send_move( extra ) )
    {
        DBGOUT( "net tx blocked: control=%d ack=%d cmd=%d", this->m_packets.last_sequence( ),
                this->m_packets.acknowledged_sequence( ), command );
        return false;
    }

    const i32 sequence{ this->m_channel->m_nOutSequenceNr };
    const i32 choked{ this->m_channel->m_nChokedPackets };
    if ( sequence <= this->m_packets.last_sequence( ) || sequence <= 0 ||
         this->m_channel->m_StreamReliable.IsOverflowed( ) )
    {
        DBGOUT( "net tx rejected: seq=%d previous=%d reliable_overflow=%d", sequence,
                this->m_packets.last_sequence( ), this->m_channel->m_StreamReliable.IsOverflowed( ) );
        return false;
    }

    const packet_plan_t plan{ this->m_packets.plan( sequence, extra, movement ) };
    if ( plan.m_advance >= INT_MAX - sequence || plan.m_choked > 255 )
    {
        DBGOUT( "net tx rejected: seq=%d advance=%d choked=%d", sequence,
                plan.m_advance, plan.m_choked );
        return false;
    }

    const i32  known{ this->m_packets.known_sequence( ) };
    const i32  last{ this->m_packets.last_sequence( ) };
    const bool assumes_lost{ plan.m_choked > sequence - last - 1 };
    this->m_channel->m_nOutSequenceNr += plan.m_advance;
    this->m_channel->m_nChokedPackets = plan.m_choked;

    if ( movement )
    {
        g_nospread.request( );
    }
    const double          send_time{ g_tf2.m_engine->Plat_FloatTime( ) };
    const presented_ack_t acknowledged{ g_fake_latency.present( this->m_channel ) };
    const i32             sent{ this->m_channel->SendDatagram( nullptr ) };
    g_fake_latency.restore( this->m_channel, acknowledged, sent > 0 );

    if ( sent <= 0 )
    {
        DBGOUT( "net tx failed: result=%d seq=%d cmd=%d extra=%d", sent, sequence,
                command, extra );
        this->m_channel->m_nOutSequenceNr = sequence;
        this->m_channel->m_nChokedPackets = choked;
        return false;
    }

    if ( !movement )
    {
        this->m_channel->m_nOutSequenceNr = sequence + 1;
        this->m_channel->m_nChokedPackets = sequence - sent;
    }

    if ( assumes_lost && !this->m_probed && this->m_probe_sequence < 0 )
    {
        this->m_probe_sequence = sent;
        this->m_probe_first    = known + 1;
        this->m_probe_last     = last;
    }

    this->m_packets.record( sent, command, movement, send_time );
    this->m_last_send_time      = *g_tf2.m_net_time.get_as< const double * >( );
    this->m_sent_reliable_state = this->m_channel->m_nInReliableState;
    return true;
}

bool networking::send_tick( )
{
    NET_Tick tick{ g_tf2.m_cl->m_nDeltaTick,
                   *g_tf2.m_host_frametime_unbounded.get_as< const float * >( ),
                   *g_tf2.m_host_frametime_stddeviation.get_as< const float * >( ) };
    return g_tf2.m_cl->m_NetChannel->SendNetMsg( tick, false, false );
}

bool networking::send_keep_alive( )
{
    if ( !this->synchronize( ) || g_tf2.m_demo_player->IsPlayingBack( ) ||
         this->m_keep_frame == g_tf2.m_globals->framecount || !this->m_channel->CanPacket( ) )
    {
        return false;
    }

    const bf_write queued{ this->m_channel->m_StreamUnreliable };
    if ( !this->send_tick( ) ||
         !this->send_datagram( this->m_packets.last_acknowledged( ), 0, false ) )
    {
        this->m_channel->m_StreamUnreliable = queued;
        return false;
    }

    this->m_keep_frame = g_tf2.m_globals->framecount;
    return true;
}

double networking::ack_budget( ) const
{
    return math::bigger( static_cast< double >( this->m_channel->GetLatency( FLOW_OUTGOING ) ), 0.0 ) + 2.0 * TICK_INTERVAL;
}

bool networking::keep_alive( i32 batch )
{
    if ( !this->synchronize( ) || this->m_move_pending )
    {
        return false;
    }

    const double now{ *g_tf2.m_net_time.get_as< const double * >( ) };
    const bool   control_overdue{ this->m_packets.acknowledged_sequence( ) < this->m_packets.last_sequence( ) &&
                                  now - this->m_last_send_time > this->ack_budget( ) };
    if ( this->m_channel->m_nInReliableState == this->m_sent_reliable_state &&
         this->m_channel->m_StreamReliable.GetNumBitsWritten( ) == 0 &&
         !control_overdue )
    {
        return false;
    }

    if ( ( batch > MAX_SHIFT - 1 && !control_overdue ) ||
         !this->m_packets.clamps_drop( ) ||
         !this->m_packets.can_keep_alive( ) )
    {
        DBGOUT( "keep-alive held back: held=%d batch=%d clamps=%d move_acked=%d", g_tf2.m_cl->chokedcommands, batch,
                this->m_packets.clamps_drop( ), this->m_packets.can_keep_alive( ) );
        return false;
    }

    const bool sent{ this->send_keep_alive( ) };
    DBGOUT( "keep-alive: sent=%d held=%d batch=%d", sent, g_tf2.m_cl->chokedcommands, batch );
    return sent;
}

void networking::on_choked( )
{
    if ( !g_cl.m_final_tick || g_tickbase.holds_packet( ) || !this->synchronize( ) )
    {
        return;
    }

    this->keep_alive( g_tf2.m_cl->chokedcommands + 1 + send_delay_t::remaining_ticks( this->ack_budget( ), TICK_INTERVAL ) +
                      math::bigger( g_tickbase.shift_room( ), 0 ) );
}

void networking::probe_drop_clamp( )
{
    if ( this->m_probe_sequence < 0 || this->m_packets.acknowledged_sequence( ) < this->m_probe_sequence )
    {
        return;
    }

    if ( !this->m_packets.acked_between( this->m_probe_first, this->m_probe_last ) )
    {
        this->m_probe_sequence = -1;
        return;
    }

    if ( !g_player_perf.request( perf_owner_t::networking, g_tf2.m_engine->GetLocalPlayer( ), PERF_MAX_RECORDS ) )
    {
        return;
    }

    this->m_probe_sequence = -1;
    this->m_probed         = true;
}

void networking::on_own_perf( const perf_dump_t &dump )
{
    for ( i32 i{ }; i < dump.m_packet_count; ++i )
    {
        if ( dump.m_packets[ i ].m_dropped < 0 )
        {
            DBGOUT( "net: server passes negative drops, keep-alives off" );
            this->m_packets.refuse_negative_drop( );
            return;
        }
    }
}

bool networking::ready_for_move( i32 extra )
{
    if ( !this->synchronize( ) || this->m_packets.can_send_move( extra ) )
    {
        return true;
    }

    if ( !this->m_control_waiting )
    {
        this->m_control_retries = 0;
    }
    this->m_control_waiting = true;
    this->m_move_pending    = true;
    return false;
}

bool networking::can_run_move( )
{
    if ( !this->synchronize( ) )
    {
        return true;
    }

    const bool allowed{ this->m_packets.can_run_move( g_tf2.m_cl->chokedcommands ) };
    if ( !allowed )
    {
        this->ready_for_move( g_tf2.m_cl->chokedcommands + 1 - MAX_NEW_COMMANDS );
    }
    return allowed;
}

bool networking::can_send_now( ) const
{
    CNetChannel *channel{ g_tf2.m_cl->m_NetChannel };
    if ( !channel )
    {
        return false;
    }
    if ( channel->IsLoopback( ) && !g_tf2.host_limitlocal->GetInt( ) )
    {
        return true;
    }
    return *g_tf2.m_net_time.get_as< const double * >( ) >= g_tf2.m_cl->m_flNextCmdTime &&
           channel->CanPacket( );
}

void networking::gate_send( )
{
    // a shift holds the shot until its last extra, the whole packet has to be one move
    if ( !g_cl.m_final_tick || g_tickbase.holds_packet( ) )
    {
        g_cl.m_send_packet = false;
        return;
    }

    if ( this->m_move_pending && !g_cl.m_psilent_choked && this->can_send_now( ) )
    {
        g_cl.m_send_packet = true;
    }
    if ( g_cl.m_send_packet && !this->ready_for_move( g_tf2.m_cl->chokedcommands + 1 - MAX_NEW_COMMANDS ) )
    {
        g_cl.m_send_packet = false;
    }
}

bool networking::wait_for_control( bool final_tick )
{
    if ( !this->m_control_waiting || !this->synchronize( ) )
    {
        return false;
    }

    if ( this->m_packets.can_send_move( g_tf2.m_cl->chokedcommands + 1 - MAX_NEW_COMMANDS ) )
    {
        this->m_control_waiting = false;
        this->m_control_retries = 0;
        return false;
    }

    const double now{ *g_tf2.m_net_time.get_as< const double * >( ) };
    const double retry{ math::bigger( 0.05, 2.0 * this->m_channel->GetLatency( FLOW_OUTGOING ) +
                                                2.0 * TICK_INTERVAL ) *
                        ( 1 << this->m_control_retries ) };
    if ( final_tick && now - this->m_last_send_time >= retry )
    {
        if ( this->send_keep_alive( ) )
        {
            this->m_control_retries = math::smaller( this->m_control_retries + 1, 4 );
        }
    }
    return true;
}

void networking::remove_input_delay( bool final_tick )
{
    CNetChannel *net_channel{ g_tf2.m_engine->GetNetChannelInfo( ) };
    if ( !net_channel || net_channel->IsLoopback( ) || !g_tf2.m_engine->IsInGame( ) )
    {
        return;
    }

    globals_backup_t frame{ };
    frame.store( );

    g_hooks.m_cl_read_packets.thiscall< void >( final_tick );
    m_state.store( );

    frame.restore( );
}

bool networking::should_read_packets( )
{
    CNetChannel *net_channel{ g_tf2.m_engine->GetNetChannelInfo( ) };

    if ( !net_channel || net_channel->IsLoopback( ) || !g_tf2.m_engine->IsInGame( ) )
    {
        return true;
    }

    m_state.restore( );
    return false;
}

i32 networking::sequence_number( ) const
{
    return g_tf2.m_cl->lastoutgoingcommand + g_tf2.m_cl->chokedcommands + 1;
}

send_delay_t networking::send_delay( i32 minimum_hold ) const
{
    send_delay_t delay{ };
    if ( !g_tf2.m_cl->IsActive( ) || !g_tf2.m_cl->m_NetChannel )
    {
        return delay;
    }

    if ( g_tf2.m_cl->chokedcommands + 2 <= MAX_SHIFT )
    {
        delay.m_psilent_ticks = math::bigger( minimum_hold, g_cl.m_psilent_choked ? 1 : 0 );
    }

    CNetChannel *channel{ g_tf2.m_cl->m_NetChannel };
    const double now{ *g_tf2.m_net_time.get_as< const double * >( ) };
    const i32    extra{ g_tf2.m_cl->chokedcommands + 1 + delay.m_psilent_ticks + math::bigger( g_tickbase.shift_room( ), 0 ) -
                        MAX_NEW_COMMANDS };
    if ( this->m_channel == channel && !this->m_packets.can_send_move( extra ) )
    {
        const double age{ math::bigger( now - this->m_last_send_time, 0.0 ) };
        delay.m_ack_ticks = math::bigger( 1, send_delay_t::remaining_ticks( this->ack_budget( ) - age, TICK_INTERVAL ) );
    }

    if ( !channel->IsLoopback( ) || g_tf2.host_limitlocal->GetInt( ) )
    {
        delay.m_rate_ticks = send_delay_t::remaining_ticks( g_tf2.m_cl->m_flNextCmdTime - now,
                                                            TICK_INTERVAL );
        // the extras of a shift run off the final tick but leave with its last one, in this frame's move
        if ( !channel->CanPacket( ) || ( !g_cl.m_final_tick && !g_tickbase.holds_packet( ) ) )
        {
            delay.m_rate_ticks = math::bigger( delay.m_rate_ticks, 1 );
        }
    }
    return delay;
}

move_failure_t networking::cl_sendmove( i32 command, i32 commands )
{
    // never report a larger batch as submitted after silently truncating its oldest commands;
    if ( commands <= 0 || commands > MAX_NEW_COMMANDS + MAX_BACKUP_COMMANDS || command < commands )
    {
        DBGOUT( "net move range rejected: last=%d commands=%d", command, commands );
        return move_failure_t::command_limit;
    }
    alignas( 4 ) u8 data[ MAX_CMD_BUFFER ]{ };

    CLC_Move move{ };
    move.m_DataOut.StartWriting( data, sizeof( data ) );

    move.m_nNewCommands = math::clamp( commands, 0, MAX_NEW_COMMANDS );
    const int extra_commands{ commands - move.m_nNewCommands };
    move.m_nBackupCommands = math::smaller( math::clamp( extra_commands, 2, MAX_BACKUP_COMMANDS ),
                                            command - move.m_nNewCommands );

    const int total{ move.m_nNewCommands + move.m_nBackupCommands };

    int  from{ -1 };
    bool ok{ true };

    for ( int to{ command - total + 1 }; to <= command; ++to )
    {
        const bool is_new_command{ to >= ( command - move.m_nNewCommands + 1 ) };
        ok   = ok && g_tf2.m_client->WriteUsercmdDeltaToBuffer( &move.m_DataOut, from, to, is_new_command );
        from = to;
    }

    if ( !ok || move.m_DataOut.IsOverflowed( ) )
    {
        DBGOUT( "net move encode failed: cmd=%d new=%d backup=%d bits=%d overflow=%d",
                command, move.m_nNewCommands, move.m_nBackupCommands,
                move.m_DataOut.GetNumBitsWritten( ), move.m_DataOut.IsOverflowed( ) );
        return move_failure_t::encode;
    }

    return g_tf2.m_cl->m_NetChannel->SendNetMsg( move, false, false )
               ? move_failure_t::none
               : move_failure_t::move_message;
}

move_result_t networking::cl_move( float accumulated_extra_samples, bool final_tick, bool generate )
{
    move_result_t result{ };
    const double *net_time{ g_tf2.m_net_time.get_as< const double * >( ) };

    if ( !g_tf2.m_cl->IsConnected( ) )
    {
        this->clear( );
        result.m_failure = move_failure_t::disconnected;
        return result;
    }

    if ( !g_tf2.m_Host_ShouldRun_fn.call_as< bool >( ) )
    {
        result.m_failure = move_failure_t::host_stopped;
        return result;
    }

    g_cl.m_send_packet = true;
    g_cl.m_final_tick  = final_tick;
    const bool active{ this->synchronize( ) };
    this->wait_for_control( final_tick );
    this->probe_drop_clamp( );
    if ( generate && active && !this->can_run_move( ) )
    {
        g_cl.m_send_packet = false;
        result.m_failure   = move_failure_t::control_wait;
        return result;
    }

    if ( g_tf2.m_demo_player->IsPlayingBack( ) )
    {
        if ( !g_tf2.m_cl->ishltv )
        {
            result.m_failure = move_failure_t::demo_playback;
            return result;
        }

        g_cl.m_send_packet = false;
    }

    CNetChannel *const channel{ g_tf2.m_cl->m_NetChannel };
    if ( !channel )
    {
        result.m_failure = move_failure_t::disconnected;
        return result;
    }
    if ( !this->can_send_now( ) || !final_tick )
    {
        g_cl.m_send_packet = false;
    }

    const bool attempt_create{ generate && active && g_tf2.m_cl->chokedcommands < MAX_SHIFT };
    bool       create_command{ };
    i32        command{ g_tf2.m_cl->lastoutgoingcommand + g_tf2.m_cl->chokedcommands };
    if ( attempt_create )
    {
        command                  = sequence_number( );
        this->m_created_sequence = -1;
        g_tf2.m_client->CreateMove(
            command,
            g_tf2.m_globals->interval_per_tick - accumulated_extra_samples,
            !g_tf2.m_cl->IsPaused( ) );

        create_command     = this->m_created_sequence == command;
        result.m_generated = create_command;
        result.m_sequence  = command;
        if ( !create_command )
        {
            result.m_failure = move_failure_t::create_move_missing;
            return result;
        }

        // the tickbase banks this host tick like a charge tick; The command stays unmade and the next tick's takes
        // its sequence, the slot gets written again..
        if ( g_tickbase.banks_tick( ) )
        {
            result.m_generated = false;
            result.m_failure   = move_failure_t::deferred;
            return result;
        }

        if ( channel != g_tf2.m_cl->m_NetChannel || !g_tf2.m_cl->IsActive( ) )
        {
            result.m_failure = move_failure_t::channel_changed;
            return result;
        }

        if ( g_tf2.m_demo_recorder->IsRecording( ) )
        {
            // dem_usercmd stores the sent command verbatim, so record it with the engine view angles instead
            const vec3 sent{ g_cl.m_cmd->viewangles };
            if ( g_cfg.m_misc.m_clean_demos )
            {
                g_tf2.m_engine->GetViewAngles( g_cl.m_cmd->viewangles );
            }
            g_tf2.m_demo_recorder->RecordUserInput( command );
            g_cl.m_cmd->viewangles = sent;
        }
    } else if ( generate && active )
    {
        result.m_failure = move_failure_t::command_limit;
    }

    this->gate_send( );
    if ( !g_cl.m_send_packet )
    {
        if ( create_command )
        {
            g_tf2.m_cl->m_NetChannel->SetChoked( );
            ++g_tf2.m_cl->chokedcommands;
        }
        if ( result.m_failure == move_failure_t::none )
        {
            result.m_failure = move_failure_t::deferred;
        }
        return result;
    }

    const bool timing_out{ g_tf2.m_cl->m_NetChannel->IsTimingOut( ) &&
                           !g_tf2.m_demo_player->IsPlayingBack( ) &&
                           g_tf2.m_cl->IsActive( ) };

    if ( timing_out != this->m_warned_timeout )
    {
        this->m_warned_timeout = timing_out;

        if ( timing_out )
        {
            push_notification( notify_kind_t::warning, ICON_ZAP,
                               ATLAS_XORSTR_STATIC( "Networking" ),
                               ATLAS_XORSTR_STATIC( "Connection is timing out; the server has "
                                                    "stopped acknowledging our packets." ) );
        }
    }

    if ( timing_out )
    {
        g_tf2.m_cl->ForceFullUpdate( );
        g_tickbase.clear( );
    }

    if ( active )
    {
        const i32 commands{ g_tf2.m_cl->chokedcommands + ( create_command ? 1 : 0 ) };
        if ( commands == 0 )
        {
            return result;
        }
        const i32      extra{ math::bigger( commands - MAX_NEW_COMMANDS, 0 ) };
        const bf_write queued{ this->m_channel->m_StreamUnreliable };
        move_failure_t failure{ this->cl_sendmove( command, commands ) };
        if ( failure == move_failure_t::none && !this->send_tick( ) )
        {
            failure = move_failure_t::tick_message;
        }
        if ( failure == move_failure_t::none && !this->send_datagram( command, extra, true ) )
        {
            failure = move_failure_t::datagram;
        }
        if ( failure != move_failure_t::none )
        {
            // SendDatagram returns zero before draining the streams; Remove only the move
            // and tick we appended, preserving any messages that were already queued..
            this->m_channel->m_StreamUnreliable = queued;
            this->m_move_pending                = true;
            g_cl.m_send_packet                  = false;
            if ( create_command )
            {
                this->m_channel->SetChoked( );
                ++g_tf2.m_cl->chokedcommands;
                DBGOUT( "networking: move not sent, command %d count %d", command, commands );
            }
            result.m_failure = failure;
            return result;
        }

        g_tf2.m_cl->lastoutgoingcommand = command;
        g_tf2.m_cl->chokedcommands      = 0;
        this->m_move_pending            = false;
        this->m_control_waiting         = false;
        g_cl.m_psilent_choked           = false;
        g_cl.m_psilent_release          = false;
        result.m_sent     = true;
        result.m_commands = commands;
        g_tickbase.on_move_sent( command, commands );
        g_anti_aim.on_move_sent( command );
        g_cl.m_cmd = nullptr;
    } else
    {
        const i32 sequence{ g_tf2.m_cl->m_NetChannel->SendDatagram( nullptr ) };
        if ( sequence <= 0 )
        {
            result.m_failure = move_failure_t::datagram;
            return result;
        }

        static constexpr f32 rate_limiter{ 1.0f / 5.0f };
        g_tf2.m_cl->lastoutgoingcommand = sequence;
        g_tf2.m_cl->chokedcommands      = 0;
        g_tf2.m_cl->m_flNextCmdTime     = *net_time + rate_limiter;
        result.m_sent                   = true;
        return result;
    }

    const float command_interval{ 1.0f / g_tf2.cl_cmdrate->GetFloat( ) };
    const float max_delta{ math::smaller( g_tf2.m_globals->interval_per_tick, command_interval ) };
    const float delta{ math::clamp( static_cast< f32 >( *net_time - g_tf2.m_cl->m_flNextCmdTime ), 0.0f, max_delta ) };
    g_tf2.m_cl->m_flNextCmdTime = *net_time + command_interval - delta;
    return result;
}
