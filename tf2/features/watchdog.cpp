#include "base.hpp"
#include "gui/theme.hpp"
#include "steam.hpp"

#include "entity_cache.hpp"
#include "party_network.hpp"
#include "player_cloud.hpp"
#include "player_perf.hpp"
#include "streamer_mode.hpp"
#include "watchdog.hpp"

ATLAS_NO_DESTROY watchdog g_watchdog{ };

constexpr float FLAG_SCORE{ 100.0f };
constexpr float SCORE_DECAY{ 1.0f };

constexpr float PITCH_SATURATED{ 89.9f };
constexpr i32   PITCH_STREAK{ 3 };
constexpr float PITCH_WEIGHT{ 50.0f };

constexpr i32   CHOKE_STREAK{ 3 };
constexpr float CHOKE_WEIGHT{ 20.0f };

constexpr i32    OVERFLOW_PACKETS{ 3 };
constexpr float  OVERFLOW_WEIGHT{ 50.0f };
constexpr i32    SHIFT_TICKS{ 10 };
constexpr float  SHIFT_WEIGHT{ 50.0f };
constexpr double PROBE_INTERVAL{ 3.0 };
constexpr double PROBE_SPACING{ 1.0 };

constexpr float DOUBLE_TAP_WEIGHT{ 50.0f };

constexpr i32   SNAP_GAP{ 2 };
constexpr float SNAP_DEGREES{ 20.0f };
constexpr float RETURN_DEGREES{ 5.0f };
constexpr float SILENT_AIM_WEIGHT{ 34.0f };

constexpr usize EQUIP_REGION_MASK{ 0x20C };
constexpr i32   MAX_WEARABLES{ 16 };
constexpr float REGION_INTERVAL{ 2.0f };
constexpr float REGION_WEIGHT{ 50.0f };

constexpr usize CLOUD_NAME_SLOTS{ 4 };
static char     s_cloud_names[ CLOUD_NAME_SLOTS ][ 129 ]{ };
static usize    s_next_cloud_name{ };

ATLAS_FORCE_INLINE bool short_gap( i32 gap )
{
    return gap > 0 && gap <= SNAP_GAP;
}

ATLAS_FORCE_INLINE bool shares_equip_region( C_TFPlayer *player, u32 account )
{
    void                  *schema{ g_tf2.m_GetItemSchema.call_as< void * >( ) };
    const handle_vector_t &wearables{ player->m_hMyWearables( ) };

    u16 seen[ MAX_WEARABLES ]{ };
    i32 seen_count{ };
    u32 covered{ };

    for ( i32 index{ }; index < wearables.m_count && seen_count < MAX_WEARABLES; ++index )
    {
        C_BaseEntity *entity{ wearables.m_handles[ index ].Get( ) };

        if ( !entity || entity->As< C_TFWearable * >( )->m_iAccountID( ) != account )
        {
            continue;
        }

        const u16 definition{ entity->As< C_TFWearable * >( )->m_iItemDefinitionIndex( ) };
        bool      repeated{ };
        for ( i32 slot{ }; slot < seen_count; ++slot )
        {
            repeated |= seen[ slot ] == definition;
        }

        if ( repeated )
        {
            continue;
        }

        seen[ seen_count++ ] = definition;

        const u8 *item{ g_tf2.m_CEconItemSchema_GetItemDefinition.call_as< const u8 *, void *, i32 >(
            schema, definition ) };
        const u32 regions{ *reinterpret_cast< const u32 * >( item + EQUIP_REGION_MASK ) };
        if ( regions & covered )
        {
            return true;
        }

        covered |= regions;
    }

    return false;
}

const char *watchdog::detection_name( detection_t detection )
{
    switch ( detection )
    {
        case detection_t::invalid_pitch: return ATLAS_XORSTR_STATIC( "invalid pitch" );
        case detection_t::packet_choke: return ATLAS_XORSTR_STATIC( "packet choke" );
        case detection_t::command_overflow: return ATLAS_XORSTR_STATIC( "command overflow" );
        case detection_t::tickbase_shift: return ATLAS_XORSTR_STATIC( "tickbase shift" );
        case detection_t::double_tap: return ATLAS_XORSTR_STATIC( "double tap" );
        case detection_t::silent_aim: return ATLAS_XORSTR_STATIC( "silent aim" );
        case detection_t::equip_regions: return ATLAS_XORSTR_STATIC( "equip region unlock" );
        default: return ATLAS_XORSTR_STATIC( "unknown" );
    }
}

bool watchdog::conclusive( detection_t detection )
{
    switch ( detection )
    {
        case detection_t::packet_choke: return false;
        default: return true;
    }
}

void watchdog::clear( )
{
    for ( record_t &record : this->m_records )
    {
        record = record_t{ };
    }

    this->m_next_probe = 0.0;
}

const char *watchdog::known_cheater_name( i32 index, u32 account, bool hidden )
{
    if ( !g_cfg.m_misc.m_watchdog || !account )
    {
        return nullptr;
    }

    auto      &cloud_alias{ s_cloud_names[ s_next_cloud_name ] };
    const bool is_cheater{ g_player_cloud.known( account, cloud_alias ) ||
                           g_cfg.player_tag( account ) == config::PLAYER_TAG_CHEATER };
    if ( hidden )
    {
        return is_cheater ? g_streamer_mode.get_ideal_player_name( index, true ) : nullptr;
    }

    if ( !cloud_alias[ 0 ] )
    {
        return nullptr;
    }

    s_next_cloud_name = ( s_next_cloud_name + 1 ) % CLOUD_NAME_SLOTS;
    return cloud_alias;
}

void watchdog::adopt( record_t &record, i32 index, u32 handle )
{
    record          = record_t{ };
    record.m_handle = handle;

    player_info_t info{ };
    record.m_eligible = g_tf2.m_engine->GetPlayerInfo( index, &info ) && info.friendsID &&
                        !info.fakeplayer && !info.ishltv && !info.isreplay;
    record.m_account  = info.friendsID;
}

void watchdog::on_network_update( )
{
    if ( !g_cfg.m_misc.m_watchdog ||
         g_tf2.m_engine->IsPlayingDemo( ) ||
         g_tf2.sv_cheats->GetInt( ) )
    {
        return;
    }

    const i32 server_tick{ g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick };
    const i32 local{ g_tf2.m_engine->GetLocalPlayer( ) };

    for ( const auto *cached : g_entity_cache.players( ) )
    {
        const i32 index{ cached->m_index };
        if ( index == local || index < 1 || index >= MAX_TRACKED_PLAYERS )
        {
            continue;
        }

        record_t &record{ this->m_records[ index ] };
        if ( record.m_handle != cached->m_handle )
        {
            this->adopt( record, index, cached->m_handle );
        }

        if ( !record.m_eligible || record.m_flagged )
        {
            continue;
        }

        C_TFPlayer *player{ cached->m_entity->As< C_TFPlayer * >( ) };
        if ( player->IsDormant( ) )
        {
            record.m_tracked = false;
            continue;
        }

        if ( !player->IsAlive( ) )
        {
            record.m_tracked     = false;
            record.m_region_seen = false;
            continue;
        }

        this->observe( record, index, player, server_tick );
    }

    this->probe( );
    this->report( );
}

void watchdog::observe( record_t &record, i32 index, C_TFPlayer *player, i32 server_tick )
{
    if ( server_tick >= record.m_region_tick )
    {
        record.m_region_tick = server_tick + TIME_TO_TICKS( REGION_INTERVAL );
        if ( !record.m_region_seen && shares_equip_region( player, record.m_account ) )
        {
            record.m_region_seen = true;
            this->infract( record, index, detection_t::equip_regions, REGION_WEIGHT );
        }
    }

    const i32 sim_tick{ TIME_TO_TICKS( player->m_flSimulationTime( ) ) };
    if ( record.m_tracked && sim_tick == record.m_sim_tick )
    {
        record.m_seen_tick = server_tick;
        return;
    }

    const vec3 wire{ player->m_angEyeAngles( ).x, player->m_angEyeAngles( ).y, 0.0f };
    const i32  gap{ sim_tick - record.m_sim_tick };

    if ( record.m_tracked )
    {
        if ( record.m_seen_tick - record.m_changed_tick >= MAX_NEW_COMMANDS )
        {
            record.m_probe_wanted = true;
            if ( ++record.m_choke_streak == CHOKE_STREAK )
            {
                record.m_choke_streak = 0;
                this->infract( record, index, detection_t::packet_choke, CHOKE_WEIGHT );
            }
        } else
        {
            record.m_choke_streak = 0;
        }

        if ( record.m_shot && short_gap( record.m_gap ) && short_gap( gap ) &&
             math::calc_fov( record.m_previous_eye, record.m_eye ) >= SNAP_DEGREES &&
             math::calc_fov( record.m_previous_eye, wire ) <= RETURN_DEGREES )
        {
            this->infract( record, index, detection_t::silent_aim, SILENT_AIM_WEIGHT );
        }

        record.m_previous_eye = record.m_eye;
    } else
    {
        record.m_choke_streak = 0;
        record.m_pitch_streak = 0;
        record.m_previous_eye = wire;
    }

    if ( math::abs( wire.x ) < PITCH_SATURATED )
    {
        record.m_pitch_streak = 0;
    } else if ( ++record.m_pitch_streak == PITCH_STREAK )
    {
        record.m_pitch_streak = 0;
        this->infract( record, index, detection_t::invalid_pitch, PITCH_WEIGHT );
    }

    record.m_eye          = wire;
    record.m_gap          = record.m_tracked ? gap : 0;
    record.m_shots        = 0;
    record.m_shot         = false;
    record.m_tracked      = true;
    record.m_sim_tick     = sim_tick;
    record.m_changed_tick = server_tick;
    record.m_seen_tick    = server_tick;
}

void watchdog::on_fire_bullets( i32 shooter )
{
    if ( shooter < 1 || shooter >= MAX_TRACKED_PLAYERS )
    {
        return;
    }

    record_t &record{ this->m_records[ shooter ] };
    if ( !record.m_tracked || record.m_flagged ||
         record.m_changed_tick != g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick )
    {
        return;
    }

    record.m_shot = true;

    if ( ++record.m_shots == 2 && record.m_gap != 0 && record.m_gap < 2 )
    {
        this->infract( record, shooter, detection_t::double_tap, DOUBLE_TAP_WEIGHT );
    }
}

void watchdog::probe( )
{
    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    if ( now < this->m_next_probe )
    {
        return;
    }

    for ( i32 index{ 1 }; index < MAX_TRACKED_PLAYERS; ++index )
    {
        record_t &record{ this->m_records[ index ] };
        if ( !record.m_probe_wanted || !record.m_tracked || record.m_flagged ||
             now - record.m_probe_time < PROBE_INTERVAL )
        {
            continue;
        }

        if ( g_player_perf.request( perf_owner_t::watchdog, index, PERF_MAX_RECORDS ) )
        {
            record.m_probe_wanted = false;
            record.m_probe_time   = now;
            this->m_next_probe    = now + PROBE_SPACING;
        }

        return;
    }
}

void watchdog::on_perf( i32 index, const perf_dump_t &dump )
{
    record_t &record{ this->m_records[ index ] };
    if ( record.m_probe_time == 0.0 || record.m_flagged )
    {
        return;
    }

    const double seen{ record.m_perf_seen };

    i32 overflows{ };
    for ( i32 slot{ }; slot < dump.m_packet_count && dump.m_packets[ slot ].m_time > seen; ++slot )
    {
        const perf_record_t &packet{ dump.m_packets[ slot ] };
        if ( packet.m_num_cmds >= MAX_NEW_COMMANDS - 1 &&
             packet.m_num_cmds + packet.m_dropped > MAX_NEW_COMMANDS )
        {
            ++overflows;
        }
    }

    i32 shifts{ };
    for ( i32 slot{ }; slot + 1 < dump.m_frame_count && dump.m_frames[ slot ].m_time > seen; ++slot )
    {
        const perf_frame_t &frame{ dump.m_frames[ slot ] };
        const perf_frame_t &previous{ dump.m_frames[ slot + 1 ] };
        const i32           advance{ TIME_TO_TICKS( static_cast< float >(
            frame.m_final_simulation - previous.m_final_simulation ) ) };
        if ( !frame.m_ticks_corrected || frame.m_num_cmds - advance < SHIFT_TICKS )
        {
            continue;
        }

        // ping drop fixes
        for ( i32 index{ }; index < dump.m_packet_count; ++index )
        {
            const perf_record_t &packet{ dump.m_packets[ index ] };
            if ( packet.m_time >= previous.m_time && packet.m_time <= frame.m_time &&
                 packet.m_num_cmds >= SHIFT_TICKS )
            {
                ++shifts;
                break;
            }
        }
    }

    record.m_perf_seen = math::bigger( dump.m_packets[ 0 ].m_time,
                                       dump.m_frame_count ? dump.m_frames[ 0 ].m_time : 0.0 );

    if ( overflows >= OVERFLOW_PACKETS )
    {
        this->infract( record, index, detection_t::command_overflow, OVERFLOW_WEIGHT );
    }

    if ( shifts )
    {
        this->infract( record, index, detection_t::tickbase_shift, SHIFT_WEIGHT );
    }
}

void watchdog::infract( record_t &record, i32 index, detection_t detection, float weight )
{
    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    const float  decay{ static_cast< float >( now - record.m_score_time ) * SCORE_DECAY };

    record.m_score      = math::bigger( record.m_score - decay, 0.0f ) + weight;
    record.m_score_time = now;
    ++record.m_hits[ static_cast< usize >( detection ) ];

    if ( record.m_score >= FLAG_SCORE && conclusive( detection ) )
    {
        this->flag( record, index, detection );
    }
}

void watchdog::flag( record_t &record, i32 index, detection_t detection )
{
    record.m_flagged = true;

    char          cloud_alias[ 129 ]{ };
    player_info_t info{ };
    if ( g_steam.is_friend( record.m_account ) ||
         g_party_network.has_peer( record.m_account ) ||
         g_cfg.player_tag( record.m_account ) == config::PLAYER_TAG_FRIEND ||
         g_player_cloud.known( record.m_account, cloud_alias ) ||
         !g_tf2.m_engine->GetPlayerInfo( index, &info ) )
    {
        return;
    }

    str::copy( record.m_name, sizeof( record.m_name ), info.name );
    record.m_report_pending = true;

    char message[ 96 ]{ };
    str::format_to( message, sizeof( message ), ATLAS_XORSTR( "Watchdog: {}" ).get( ),
                    detection_name( detection ) );

    push_notification( notify_kind_t::warning, ICON_BAN, g_cl.get_player_name( index ), message );
}

void watchdog::report( )
{
    for ( record_t &record : this->m_records )
    {
        if ( !record.m_report_pending )
        {
            continue;
        }

        char        reason[ 256 ]{ };
        usize       length{ str::format_to( reason, sizeof( reason ),
                                            ATLAS_XORSTR( "Atlas watchdog:" ).get( ) ) };
        const char *separator{ ATLAS_XORSTR_STATIC( " " ) };
        for ( usize detection{ }; detection < static_cast< usize >( detection_t::_COUNT ); ++detection )
        {
            if ( !record.m_hits[ detection ] )
            {
                continue;
            }

            length += str::format_to( reason + length, sizeof( reason ) - length,
                                      ATLAS_XORSTR( "{}{} x{}" ).get( ), separator,
                                      detection_name( static_cast< detection_t >( detection ) ),
                                      record.m_hits[ detection ] );
            separator = ATLAS_XORSTR_STATIC( ", " );
        }

        if ( g_player_cloud.submit_report( record.m_account, record.m_name, reason, { } ) !=
             player_cloud::report_result_t::busy )
        {
            record.m_report_pending = false;
        }

        return;
    }
}
