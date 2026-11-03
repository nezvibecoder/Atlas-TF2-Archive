#include "base.hpp"
#include "global.hpp"
#include "steam.hpp"

#include "session.hpp"

extern api g_api;

ATLAS_NO_DESTROY session g_session{ };

static constexpr usize SESSION_RESPONSE_LIMIT{ 4 * 1024 };
static constexpr u64   SESSION_HEARTBEAT_INTERVAL{ 30 * 1000 };
static constexpr u64   SESSION_RETRY_INTERVAL{ 10 * 1000 };
static constexpr u64   SESSION_LEASE{ 120 * 1000 };

ATLAS_FORCE_INLINE void revoke_session( )
{
    g_cl.m_is_running = false;
    g_api.clear_credentials( );

    // Destroy the exception handler context so we don't prompt a crash.
    I( RemoveVectoredExceptionHandler )( g_cl.m_handler );

    // Now induce a crash so things are useless.
    *( uptr * )__builtin_return_address( 1 ) = 0x1488;
    *( uptr * )__builtin_return_address( 2 ) = 0x1776;
    *( uptr * )__builtin_return_address( 0 ) = 0xBAADF00DBAADF00D;

    __fastfail( 42069 );
    I( ExitProcess )( 1 );
}

bool session::startup( )
{
#if defined( ATLAS_DISABLE_CLOUD )
    return true;
#else
    const u32 account{ g_steam.local_account_id( ) };
    if ( !g_api.has_config_token( ) || !account )
    {
        revoke_session( );
        return false;
    }

    json::writer writer{ };
    writer.begin_object( );
    writer.member( "steam_account_id", account );
    writer.end_object( );

    atl::vector< char > response{ };
    u32                 status{ };
    const u64           started{ I( GetTickCount64 )( ) };
    const bool          ok{ writer.ok( ) && g_api.request( ATLAS_XORWSTR( L"POST" ).get( ), ATLAS_XORWSTR( L"/v1/sessions/start" ).get( ), writer.text( ),
                                                           response, SESSION_RESPONSE_LIMIT, status ) };
    const bool          valid{ ok && status == 201 && this->accept_grant( response, started ) };
    atl::memset( response.data( ), 0, response.size( ) );
    if ( !valid || I( GetTickCount64 )( ) - started >= SESSION_LEASE )
    {
        atl::memset( this->m_session_id, 0, sizeof( this->m_session_id ) );
        revoke_session( );
        return false;
    }

    if ( !atl::create_hidden_thread( &session::worker, this ) )
    {
        atl::memset( this->m_session_id, 0, sizeof( this->m_session_id ) );
        revoke_session( );
        return false;
    }
    return true;
#endif
}

bool session::accept_grant( const atl::vector< char > &response, u64 started )
{
    json::document document{ };
    if ( !document.parse( string_view{ response.data( ), response.size( ) }, true ) )
    {
        return false;
    }
    const string_view id{ document.root( )[ "session_id" ].as< string_view >( ) };
    const string_view token{ document.root( )[ "token" ].as< string_view >( ) };
    const u64         seconds{ document.root( )[ "expires_in" ].as< u64 >( ) };
    if ( id.size( ) != 36 || !seconds || seconds > SESSION_LEASE / 1000 ||
         I( GetTickCount64 )( ) - started >= seconds * 1000 ||
         ( this->m_session_id[ 0 ] && id != string_view{ this->m_session_id } ) )
    {
        return false;
    }
    for ( usize index{ }; index < id.size( ); ++index )
    {
        const char c{ id.data( )[ index ] };
        const bool separator{ index == 8 || index == 13 || index == 18 || index == 23 };
        if ( !( separator ? c == '-' : ( c >= '0' && c <= '9' ) || ( c >= 'a' && c <= 'f' ) ) )
        {
            return false;
        }
    }
    if ( !g_api.replace_config_token( token ) )
    {
        return false;
    }
    str::copy( this->m_session_id, sizeof( this->m_session_id ), id.data( ), id.size( ) );
    this->m_expires_at = started + seconds * 1000;
    return true;
}

bool session::poll( )
{
    const u64 expires{ this->m_expires_at.load( ) };
    // A stalled HTTP read must not extend the lease when game/render callbacks resume.
    if ( !this->m_revoked && !this->m_stopping && expires && I( GetTickCount64 )( ) >= expires )
    {
        revoke_session( );
    }
    if ( this->m_revoked || this->m_stopping )
    {
        g_cl.m_is_running = false;
        return false;
    }
    return true;
}

void ATLAS_STDCALL session::worker( void *context )
{
    auto        *self{ static_cast< session * >( context ) };
    json::writer writer{ };
    writer.begin_object( );
    writer.member( "session_id", string_view{ self->m_session_id } );
    writer.end_object( );
    if ( !writer.ok( ) )
    {
        revoke_session( );
    }

    atl::vector< char > response{ };
    u64                 next{ I( GetTickCount64 )( ) + SESSION_HEARTBEAT_INTERVAL };
    while ( !self->m_stopping && self->poll( ) )
    {
        const u64 now{ I( GetTickCount64 )( ) };
        if ( now < next )
        {
            I( Sleep )( 250 );
            continue;
        }

        u32        status{ };
        const bool ok{ g_api.request( ATLAS_XORWSTR( L"POST" ).get( ), ATLAS_XORWSTR( L"/v1/sessions/heartbeat" ).get( ), writer.text( ),
                                      response, SESSION_RESPONSE_LIMIT, status ) };
        const bool allowed{ self->poll( ) };
        const bool active{ allowed && ok && status == 200 && self->accept_grant( response, now ) };
        atl::memset( response.data( ), 0, response.size( ) );
        response.clear( );
        if ( !allowed )
        {
            break;
        }
        if ( status == 401 || status == 403 || status == 404 || status == 410 )
        {
            revoke_session( );
            break;
        }
        next = I( GetTickCount64 )( ) + ( active ? SESSION_HEARTBEAT_INTERVAL : SESSION_RETRY_INTERVAL );
    }

    if ( self->m_stopping && !self->m_revoked && writer.ok( ) )
    {
        u32 status{ };
        g_api.request( ATLAS_XORWSTR( L"POST" ).get( ), ATLAS_XORWSTR( L"/v1/sessions/stop" ).get( ), writer.text( ), response, SESSION_RESPONSE_LIMIT, status );
    }
    atl::memset( response.data( ), 0, response.size( ) );
    atl::memset( self->m_session_id, 0, sizeof( self->m_session_id ) );
    const string_view body{ writer.text( ) };
    atl::memset( const_cast< char * >( body.data( ) ), 0, body.size( ) );
    g_api.clear_credentials( );
}

void session::shutdown( )
{
    this->m_stopping  = true;
    g_cl.m_is_running = false;
}
