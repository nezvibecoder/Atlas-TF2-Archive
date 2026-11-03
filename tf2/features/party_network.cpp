#include "party_network.hpp"

#include "auto_vacc.hpp"
#include "steam.hpp"

#include "gui/notifications.hpp"

ATLAS_NO_DESTROY party_network g_party_network{ };

static constexpr char PARTY_PREFIX[]{ 'A', 'T', 'L', 'S', '1' };
static constexpr char HEX_DIGITS[]{ "0123456789ABCDEF" };

ATLAS_FORCE_INLINE i32 hex_digit( char digit )
{
    if ( digit >= '0' && digit <= '9' )
    {
        return digit - '0';
    }
    if ( digit >= 'A' && digit <= 'F' )
    {
        return digit - 'A' + 10;
    }
    return -1;
}

ATLAS_FORCE_INLINE void write_account( u8 *bytes, u32 account )
{
    for ( usize i{ }; i < sizeof( account ); ++i )
    {
        bytes[ i ] = static_cast< u8 >( account >> ( i * 8 ) );
    }
}

ATLAS_FORCE_INLINE u32 read_account( const u8 *bytes )
{
    u32 account{ };
    for ( usize i{ }; i < sizeof( account ); ++i )
    {
        account |= static_cast< u32 >( bytes[ i ] ) << ( i * 8 );
    }
    return account;
}

ATLAS_FORCE_INLINE u32 party_account( const char *steamid )
{
    if ( !steamid )
    {
        return 0;
    }

    u64 id{ };
    usize digits{ };
    for ( ; steamid[ digits ]; ++digits )
    {
        const char digit{ steamid[ digits ] };
        if ( digit < '0' || digit > '9' || digits == 20 ||
             id > ( ~static_cast< u64 >( 0 ) - static_cast< u64 >( digit - '0' ) ) / 10 )
        {
            return 0;
        }
        id = id * 10 + static_cast< u64 >( digit - '0' );
    }

    return digits && ( id & 0xFFFFFFFF00000000ull ) == 0x0110000100000000ull
               ? static_cast< u32 >( id )
               : 0;
}

void party_network::send( packet_type type, const u8 *payload, usize size )
{
    Assert( size <= ( MAX_PARTY_TEXT - sizeof( PARTY_PREFIX ) - 2 ) / 2 );

    char text[ MAX_PARTY_TEXT + 1 ]{ };
    for ( usize i{ }; i < sizeof( PARTY_PREFIX ); ++i )
    {
        text[ i ] = PARTY_PREFIX[ i ];
    }

    const u8 kind{ static_cast< u8 >( type ) };
    text[ sizeof( PARTY_PREFIX ) ]     = HEX_DIGITS[ kind >> 4 ];
    text[ sizeof( PARTY_PREFIX ) + 1 ] = HEX_DIGITS[ kind & 0xF ];

    for ( usize i{ }; i < size; ++i )
    {
        text[ sizeof( PARTY_PREFIX ) + 2 + i * 2 ] = HEX_DIGITS[ payload[ i ] >> 4 ];
        text[ sizeof( PARTY_PREFIX ) + 3 + i * 2 ] = HEX_DIGITS[ payload[ i ] & 0xF ];
    }

    TFPartyClient( )->SendPartyChat( text );
}

bool party_network::has_peer( u32 account ) const
{
    if ( !account || !g_cfg.m_misc.m_party_network )
    {
        return false;
    }

    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    for ( const peer_t &peer : this->m_peers )
    {
        if ( peer.m_account == account && now - peer.m_seen_at < 130.0 )
        {
            return true;
        }
    }
    return false;
}

bool party_network::touch_peer( u32 account, double now )
{
    for ( peer_t &peer : this->m_peers )
    {
        if ( peer.m_account == account )
        {
            const bool fresh{ now - peer.m_seen_at < 130.0 };
            peer.m_seen_at = now;
            return !fresh;
        }
    }

    for ( peer_t &peer : this->m_peers )
    {
        if ( !peer.m_account || now - peer.m_seen_at >= 130.0 )
        {
            peer = { account, now };
            return true;
        }
    }
    return false;
}

void party_network::send_vacc_status( u32 target )
{
    u8 payload[ sizeof( target ) ]{ };
    write_account( payload, target );
    this->send( packet_type::vacc_status, payload, sizeof( payload ) );
}

void party_network::send_vacc_request( u32 medic, party_vacc_request request )
{
    u8 payload[ sizeof( medic ) + 1 ]{ };
    write_account( payload, medic );
    payload[ sizeof( medic ) ] = static_cast< u8 >( request );
    this->send( packet_type::vacc_request, payload, sizeof( payload ) );
}

void party_network::send_vacc_result( u32 patient, party_vacc_result result )
{
    u8 payload[ sizeof( patient ) + 1 ]{ };
    write_account( payload, patient );
    payload[ sizeof( patient ) ] = static_cast< u8 >( result );
    this->send( packet_type::vacc_result, payload, sizeof( payload ) );
}

void party_network::think( )
{
    if ( !g_cfg.m_misc.m_party_network )
    {
        if ( this->m_enabled )
        {
            this->m_next_send_time = 0.0;
            for ( peer_t &peer : this->m_peers )
            {
                peer = { };
            }
            this->m_enabled = false;
        }
        return;
    }

    this->m_enabled = true;

    if ( !g_tf2.m_engine->IsInGame( ) || !g_tf2.m_cl->IsActive( ) )
    {
        return;
    }

    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    if ( now < this->m_next_send_time )
    {
        return;
    }

    this->send( packet_type::presence, nullptr, 0 );
    this->m_next_send_time = now + 60.0;
}

bool party_network::on_party_chat( IGameEvent *event )
{
    const i32 chat_type{ event->GetInt( ATLAS_XORSTR( "type" ).get( ) ) };
    if ( chat_type == TF_PARTY_CHAT_MEMBER_JOIN )
    {
        if ( g_cfg.m_misc.m_party_network )
        {
            this->m_next_send_time = 0.0;
        }
        return false;
    }

    if ( chat_type == TF_PARTY_CHAT_MEMBER_LEAVE )
    {
        const u32 account{ party_account( event->GetString( ATLAS_XORSTR( "steamid" ).get( ) ) ) };
        for ( peer_t &peer : this->m_peers )
        {
            if ( peer.m_account == account )
            {
                peer = { };
                break;
            }
        }
        return false;
    }

    if ( chat_type != TF_PARTY_CHAT_MEMBER && chat_type != TF_PARTY_CHAT_SEND_FAILED )
    {
        return false;
    }

    const char *text{ event->GetString( ATLAS_XORSTR( "text" ).get( ) ) };
    const usize length{ str::bounded_length( text, MAX_PARTY_TEXT + 1 ) };
    if ( length < sizeof( PARTY_PREFIX ) + 2 || length > MAX_PARTY_TEXT ||
         ( length - sizeof( PARTY_PREFIX ) - 2 ) % 2 )
    {
        return false;
    }

    for ( usize i{ }; i < sizeof( PARTY_PREFIX ); ++i )
    {
        if ( text[ i ] != PARTY_PREFIX[ i ] )
        {
            return false;
        }
    }

    u8 bytes[ ( MAX_PARTY_TEXT - sizeof( PARTY_PREFIX ) - 2 ) / 2 ]{ };
    for ( usize i{ sizeof( PARTY_PREFIX ) }; i < length; ++i )
    {
        if ( hex_digit( text[ i ] ) < 0 )
        {
            return false;
        }
    }

    const u8 kind{ static_cast< u8 >( ( hex_digit( text[ sizeof( PARTY_PREFIX ) ] ) << 4 ) |
                                     hex_digit( text[ sizeof( PARTY_PREFIX ) + 1 ] ) ) };
    const usize size{ ( length - sizeof( PARTY_PREFIX ) - 2 ) / 2 };
    for ( usize i{ }; i < size; ++i )
    {
        bytes[ i ] = static_cast< u8 >( ( hex_digit( text[ sizeof( PARTY_PREFIX ) + 2 + i * 2 ] ) << 4 ) |
                                      hex_digit( text[ sizeof( PARTY_PREFIX ) + 3 + i * 2 ] ) );
    }

    switch ( static_cast< packet_type >( kind ) )
    {
        case packet_type::presence:
            if ( size != 0 ) return true;
            break;
        case packet_type::vacc_status:
            if ( size != 4 ) return true;
            break;
        case packet_type::vacc_request:
            if ( size != 5 || bytes[ 4 ] >= static_cast< u8 >( party_vacc_request::_COUNT ) ) return true;
            break;
        case packet_type::vacc_result:
            if ( size != 5 || bytes[ 4 ] >= static_cast< u8 >( party_vacc_result::_COUNT ) ) return true;
            break;
        default: return true;
    }

    if ( chat_type == TF_PARTY_CHAT_SEND_FAILED )
    {
        if ( kind == static_cast< u8 >( packet_type::vacc_request ) && g_cfg.m_misc.m_party_network )
        {
            push_notification( notify_kind_t::warning, ATLAS_XORSTR_STATIC( "Party request failed" ),
                               ATLAS_XORSTR_STATIC( "TF2 could not send the party message." ) );
        }
        return true;
    }

    if ( !g_cfg.m_misc.m_party_network )
    {
        return true;
    }

    this->m_enabled = true;

    const u32 account{ party_account( event->GetString( ATLAS_XORSTR( "steamid" ).get( ) ) ) };
    if ( !account || account == g_steam.local_account_id( ) )
    {
        return true;
    }

    if ( kind == static_cast< u8 >( packet_type::vacc_request ) && !this->has_peer( account ) )
    {
        return true;
    }

    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    const bool new_peer{ this->touch_peer( account, now ) };

    switch ( static_cast< packet_type >( kind ) )
    {
        case packet_type::presence:
        {
            if ( size == 0 && new_peer )
            {
                this->send( packet_type::presence, nullptr, 0 );
                this->m_next_send_time = now + 60.0;
            }
            break;
        }
        case packet_type::vacc_status:
        {
            g_auto_vacc.on_party_status( account, read_account( bytes ) );
            break;
        }
        case packet_type::vacc_request:
        {
            g_auto_vacc.on_party_request( account, read_account( bytes ),
                                          static_cast< party_vacc_request >( bytes[ 4 ] ) );
            break;
        }
        case packet_type::vacc_result:
        {
            g_auto_vacc.on_party_result( account, read_account( bytes ),
                                         static_cast< party_vacc_result >( bytes[ 4 ] ) );
            break;
        }
        default: break;
    }

    return true;
}
