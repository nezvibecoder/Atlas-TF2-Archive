#include "base.hpp"
#include "gui/theme.hpp"

#include "player_cloud.hpp"
#include "streamer_mode.hpp"

extern api g_api;

ATLAS_NO_DESTROY player_cloud g_player_cloud{ };

static constexpr usize CLOUD_RESPONSE_LIMIT{ 2 * 1024 * 1024 };
static constexpr usize CLOUD_PLAYER_LIMIT{ 8192 };
static constexpr u64   CLOUD_REFRESH_INTERVAL{ 5 * 60 * 1000 };
static constexpr u64   CLOUD_RETRY_INTERVAL{ 30 * 1000 };

void ATLAS_STDCALL player_cloud::worker( void *context )
{
    auto *self{ static_cast< player_cloud * >( context ) };
    if ( self->m_operation == operation_t::list )
    {
        self->m_success = g_api.request( ATLAS_XORWSTR( L"GET" ).get( ), ATLAS_XORWSTR( L"/v1/cheaters" ).get( ), { }, self->m_response,
                                         CLOUD_RESPONSE_LIMIT, self->m_status );
    } else
    {
        json::writer writer{ };
        writer.begin_object( );
        writer.member( ATLAS_XORSTR( "account_id" ).get( ), self->m_report.m_account );
        writer.member( ATLAS_XORSTR( "name" ).get( ), string_view{ self->m_report.m_name } );
        writer.member( ATLAS_XORSTR( "reason" ).get( ), string_view{ self->m_report.m_reason } );
        writer.member( ATLAS_XORSTR( "alias" ).get( ), string_view{ self->m_report.m_alias } );
        writer.end_object( );
        self->m_success = writer.ok( ) &&
                          g_api.request( ATLAS_XORWSTR( L"POST" ).get( ), ATLAS_XORWSTR( L"/v1/cheaters/reports" ).get( ), writer.text( ),
                                         self->m_response, CLOUD_RESPONSE_LIMIT, self->m_status );
    }
    _InterlockedExchange( &self->m_state, 2 );
}

bool player_cloud::start( operation_t operation )
{
    if ( _InterlockedCompareExchange( &this->m_state, 1, 0 ) != 0 )
    {
        return false;
    }
    this->m_operation = operation;
    this->m_success   = false;
    this->m_status    = 0;
    if ( !atl::create_hidden_thread( &player_cloud::worker, this ) )
    {
        _InterlockedExchange( &this->m_state, 0 );
        return false;
    }
    return true;
}

bool player_cloud::parse_list( )
{
    json::document document{ };
    if ( !document.parse( string_view{ this->m_response.data( ), this->m_response.size( ) } ) )
    {
        return false;
    }
    const json::value items{ document.root( )[ ATLAS_XORSTR( "players" ).get( ) ] };
    if ( items.type( ) != json::type_t::array || items.size( ) > CLOUD_PLAYER_LIMIT )
    {
        return false;
    }

    atl::vector< known_t > players{ };
    u32                    previous{ };
    for ( usize index{ }; index < items.size( ); ++index )
    {
        const json::value item{ items[ index ] };
        const json::value id{ item[ ATLAS_XORSTR( "account_id" ).get( ) ] };
        const json::value name{ item[ ATLAS_XORSTR( "alias" ).get( ) ] };
        const i64         account{ id.as< i64 >( ) };
        const string_view alias{ name.as< string_view >( ) };
        if ( id.type( ) != json::type_t::number || account <= previous ||
             account > 0xFFFFFFFFll || name.type( ) != json::type_t::string ||
             alias.size( ) > 128 )
        {
            return false;
        }
        known_t *player{ players.emplace_back( ) };
        if ( !player )
        {
            return false;
        }
        player->m_account = static_cast< u32 >( account );
        str::copy( player->m_alias, sizeof( player->m_alias ), alias.data( ), alias.size( ) );
        previous = player->m_account;
    }

    atl::scoped_lock lock{ this->m_publish };
    this->m_known.swap( players );
    this->m_loaded = true;
    return true;
}

bool player_cloud::known( u32 account, char ( &alias )[ 129 ] ) const
{
    alias[ 0 ] = 0;
    if ( !account )
    {
        return false;
    }
    atl::scoped_lock lock{ this->m_publish };
    usize            first{ };
    usize            last{ this->m_known.size( ) };
    while ( first < last )
    {
        const usize    middle{ first + ( last - first ) / 2 };
        const known_t &player{ this->m_known[ middle ] };
        if ( player.m_account < account )
        {
            first = middle + 1;
        } else
        {
            last = middle;
        }
    }
    if ( first >= this->m_known.size( ) || this->m_known[ first ].m_account != account )
    {
        return false;
    }
    str::copy( alias, sizeof( alias ), this->m_known[ first ].m_alias );
    return true;
}

player_cloud::report_result_t player_cloud::submit_report( u32 account, string_view name,
                                                           string_view reason,
                                                           string_view alias )
{
    if ( !account || name.empty( ) || name.size( ) >= sizeof( this->m_report.m_name ) ||
         reason.empty( ) || reason.size( ) >= sizeof( this->m_report.m_reason ) ||
         alias.size( ) >= sizeof( this->m_report.m_alias ) )
    {
        return report_result_t::invalid;
    }
    if ( !g_api.has_config_token( ) )
    {
        return report_result_t::no_session;
    }
    if ( this->m_state )
    {
        return report_result_t::busy;
    }
    this->m_report           = report_t{ };
    this->m_report.m_account = account;
    str::copy( this->m_report.m_name, sizeof( this->m_report.m_name ),
               name.data( ), name.size( ) );
    str::copy( this->m_report.m_reason, sizeof( this->m_report.m_reason ),
               reason.data( ), reason.size( ) );
    str::copy( this->m_report.m_alias, sizeof( this->m_report.m_alias ),
               alias.data( ), alias.size( ) );
    return this->start( operation_t::report ) ? report_result_t::started
                                              : report_result_t::unavailable;
}

void player_cloud::scan_players( )
{
    const i32 local{ g_tf2.m_engine->GetLocalPlayer( ) };
    const i32 maximum{ math::smaller( g_tf2.m_engine->GetMaxClients( ),
                                      MAX_TRACKED_PLAYERS - 1 ) };
    for ( i32 index{ 1 }; index <= maximum; ++index )
    {
        player_info_t info{ };
        if ( index == local || !g_tf2.m_engine->GetPlayerInfo( index, &info ) ||
             !info.friendsID || info.fakeplayer || info.ishltv || info.isreplay )
        {
            this->m_notified[ index ] = 0;
            continue;
        }

        char cloud_alias[ 129 ]{ };
        if ( !this->known( info.friendsID, cloud_alias ) )
        {
            this->m_notified[ index ] = 0;
            continue;
        }

        if ( this->m_notified[ index ] == info.friendsID )
        {
            continue;
        }

        this->m_notified[ index ] = info.friendsID;

        const bool  hidden{ g_streamer_mode.should_hide_player( index ) };
        const char *name{ hidden ? g_streamer_mode.get_ideal_player_name( index ) : info.name };
        const char *alias{ hidden ? nullptr : g_cfg.player_alias( info.friendsID ) };
        if ( !alias && !hidden && cloud_alias[ 0 ] )
        {
            alias = cloud_alias;
        }

        // NOTE: WE SHOULD HANDLE THIS IN A BETTER WAY.
        char message[ 512 ]{ };
        if ( alias )
        {
            str::format_to( message, sizeof( message ), ATLAS_XORSTR( "The player \"{}\" is a known cheater and is commonly referred to as \"{}\"." ).get( ), name, alias );
        } else
        {
            str::format_to( message, sizeof( message ), ATLAS_XORSTR( "The player \"{}\" is a known cheater." ).get( ), name );
        }

        push_notification( notify_kind_t::warning, ICON_BAN, name, message );

        if ( g_cfg.m_misc.m_watchdog_notify_party )
        {
            if ( alias )
            {
                str::format_to( message, sizeof( message ), ATLAS_XORSTR( "[ATLAS] The player \"{}\" is a known cheater and is commonly referred to as \"{}\"." ).get( ), info.name, alias );
            } else
            {
                str::format_to( message, sizeof( message ), ATLAS_XORSTR( "[ATLAS] The player \"{}\" is a known cheater." ).get( ), info.name );
            }

            TFPartyClient( )->SendPartyChat( message );
        }
    }
}

void player_cloud::poll( )
{
    if ( !g_cl.m_is_running )
    {
        return;
    }
    const u64 now{ I( GetTickCount64 )( ) };
    if ( _InterlockedCompareExchange( &this->m_state, 2, 2 ) == 2 )
    {
        if ( this->m_operation == operation_t::list )
        {
            const bool ready{ this->m_success && this->m_status == 200 &&
                              this->parse_list( ) };
            this->m_next_fetch = now + ( ready ? CLOUD_REFRESH_INTERVAL : CLOUD_RETRY_INTERVAL );
        } else
        {
            // TODO: Make this shit a switch statement what the fuck.
            const bool  sent{ this->m_success && this->m_status == 201 };
            const char *message{ sent ? ATLAS_XORSTR_STATIC( "Report sent for review" )
                                 : this->m_status == 404
                                     ? ATLAS_XORSTR_STATIC( "Cloud report API not deployed" )
                                 : this->m_status == 409
                                     ? ATLAS_XORSTR_STATIC( "An open report already exists" )
                                 : this->m_status == 401 || this->m_status == 403
                                     ? ATLAS_XORSTR_STATIC( "Cloud session rejected" )
                                 : this->m_status == 429
                                     ? ATLAS_XORSTR_STATIC( "Cloud rate limit reached" )
                                 : this->m_status
                                     ? ATLAS_XORSTR_STATIC( "Cloud request failed" )
                                     : ATLAS_XORSTR_STATIC( "Could not reach Cloud" ) };
            push_notification( sent ? notify_kind_t::success : notify_kind_t::error,
                               ICON_PLAYERS, ATLAS_XORSTR_STATIC( "Cloud report" ),
                               message );
        }
        this->m_response.clear( );
        _InterlockedExchange( &this->m_state, 0 );
    }

    if ( g_api.has_config_token( ) && !this->m_state && now >= this->m_next_fetch )
    {
        if ( !this->start( operation_t::list ) )
        {
            this->m_next_fetch = now + CLOUD_RETRY_INTERVAL;
        }
    }
    if ( now < this->m_next_scan )
    {
        return;
    }
    this->m_next_scan = now + 1000;
    if ( !g_tf2.m_engine->IsInGame( ) )
    {
        if ( this->m_in_game )
        {
            for ( u32 &account : this->m_notified )
            {
                account = 0;
            }
        }
        this->m_in_game = false;
    } else if ( this->m_loaded )
    {
        this->m_in_game = true;
        this->scan_players( );
    }
}

void player_cloud::on_level_shutdown( )
{
    for ( u32 &account : this->m_notified )
    {
        account = 0;
    }
    this->m_in_game   = false;
    this->m_next_scan = 0;
}
