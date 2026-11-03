#include "event_notifiers.hpp"

ATLAS_NO_DESTROY event_notifiers g_event_notifiers{ };

const char *player_class_name( i32 player_class )
{
    switch ( player_class )
    {
        case TF_CLASS_SCOUT: return ATLAS_XORSTR_STATIC( "Scout" );
        case TF_CLASS_SNIPER: return ATLAS_XORSTR_STATIC( "Sniper" );
        case TF_CLASS_SOLDIER: return ATLAS_XORSTR_STATIC( "Soldier" );
        case TF_CLASS_DEMOMAN: return ATLAS_XORSTR_STATIC( "Demoman" );
        case TF_CLASS_MEDIC: return ATLAS_XORSTR_STATIC( "Medic" );
        case TF_CLASS_HEAVYWEAPONS: return ATLAS_XORSTR_STATIC( "Heavy" );
        case TF_CLASS_PYRO: return ATLAS_XORSTR_STATIC( "Pyro" );
        case TF_CLASS_SPY: return ATLAS_XORSTR_STATIC( "Spy" );
        case TF_CLASS_ENGINEER: return ATLAS_XORSTR_STATIC( "Engineer" );
        case 10: return ATLAS_XORSTR_STATIC( "Civilian" );
        default: return ATLAS_XORSTR_STATIC( "UNKNOWN" );
    }
}

static const char *get_vote_cast( bool state )
{
    return state ? ATLAS_XORSTR_STATIC( "Yes" ) : ATLAS_XORSTR_STATIC( "No" );
}

void event_notifiers::on_game_event( IGameEvent *event, const hash_t &hash )
{
    if ( !g_cfg.m_events.m_enabled || !g_cl.m_resource )
    {
        return;
    }

    // Load bearing character array buffer.
    static char buf[ 256 ]{ };
    switch ( hash )
    {
        case HASH_CT( "player_connect_client" ):
        {
            if ( g_cfg.m_events.m_notify_targets[ config::NOTIF_EVENT_PLAYER_CONNECT ] == false )
            {
                break;
            }

            const auto player_name{ event->GetString( ATLAS_XORSTR( "name" ).get( ) ) };

            // Sometimes the server can fill in an unknown player name. We'd still like to know that someone joined the server, so we combat it here.
            bool unknown_name{ g_cfg.m_streamer_mode.m_enabled };
            switch ( HASH_RT( player_name ) )
            {
                case HASH_CT( "unconnected" ):
                case HASH_CT( "NULLNAME" ): unknown_name = true; break;
                default: break;
            }

            const auto userid{ event->GetInt( ATLAS_XORSTR( "userid" ).get( ) ) };
            const auto entidx{ g_tf2.m_engine->GetPlayerForUserID( userid ) };

            atl::memset( buf, 0, sizeof( buf ) );
            atl::format( buf, sizeof( buf ), ATLAS_XORSTR( "%s has connected to the server." ).get( ), unknown_name ? ATLAS_XORSTR_STATIC( "A player" ) : player_name );
            push_notification( notify_kind_t::information, ATLAS_XORSTR_STATIC( "Player connected" ), buf );
            break;
        }

        case HASH_CT( "player_disconnect" ):
        {
            if ( g_cfg.m_events.m_notify_targets[ config::NOTIF_EVENT_PLAYER_CONNECT ] == false )
            {
                break;
            }

            const auto player_name{ event->GetString( ATLAS_XORSTR( "name" ).get( ) ) };

            // Sometimes the server can fill in an unknown player name. We'd still like to know that someone joined the server, so we combat it here.
            bool unknown_name{ g_cfg.m_streamer_mode.m_enabled };
            switch ( HASH_RT( player_name ) )
            {
                case HASH_CT( "unconnected" ):
                case HASH_CT( "NULLNAME" ): unknown_name = true; break;
                default: break;
            }

            atl::memset( buf, 0, sizeof( buf ) );
            atl::format( buf, sizeof( buf ), ATLAS_XORSTR( "%s has disconnected from the server." ).get( ), unknown_name ? ATLAS_XORSTR_STATIC( "A player" ) : player_name );
            push_notification( notify_kind_t::information, ATLAS_XORSTR_STATIC( "Player disconnected" ), buf );
            break;
        }

        case HASH_CT( "player_changeclass" ):
        {
            if ( g_cfg.m_events.m_notify_targets[ config::NOTIF_EVENT_CLASS_CHANGE ] == false )
            {
                break;
            }

            const auto uid{ event->GetInt( ATLAS_XORSTR( "userid" ).get( ) ) };
            const auto player{ g_tf2.m_engine->GetPlayerForUserID( uid ) };
            if ( player == g_tf2.m_engine->GetLocalPlayer( ) || player > g_tf2.m_engine->GetMaxClients( ) )
            {
                break;
            }

            const auto _class{ player_class_name( event->GetInt( ATLAS_XORSTR( "class" ).get( ) ) ) };
            atl::memset( buf, 0, sizeof( buf ) );
            atl::format( buf, sizeof( buf ), ATLAS_XORSTR( "%s changed class to %s." ).get( ), g_cl.get_player_name( player ), _class );
            push_notification( notify_kind_t::information, ATLAS_XORSTR_STATIC( "Class change" ), buf );
            break;
        }

        case HASH_CT( "vote_cast" ):
        {
            if ( g_cfg.m_events.m_notify_targets[ config::NOTIF_EVENT_VOTE_CAST ] == false )
            {
                break;
            }

            const auto idx{ event->GetInt( ATLAS_XORSTR( "entityid" ).get( ) ) };
            if ( idx == g_tf2.m_engine->GetLocalPlayer( ) )
            {
                break;
            }

            const auto vote_state{ event->GetInt( ATLAS_XORSTR( "vote_option" ).get( ) ) == 0 };

            atl::memset( buf, 0, sizeof( buf ) );
            atl::format( buf, sizeof( buf ), ATLAS_XORSTR( "%s has voted %s." ).get( ), g_cl.get_player_name( idx ), get_vote_cast( vote_state ) );
            push_notification( notify_kind_t::information, ATLAS_XORSTR_STATIC( "Vote cast" ), buf );
            break;
        }

        case HASH_CT( "player_hurt" ):
        {
            if ( g_cfg.m_events.m_notify_targets[ config::NOTIF_EVENT_DAMAGE ] == false )
            {
                break;
            }

            const auto attacker_idx{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ATLAS_XORSTR( "attacker" ).get( ) ) ) };
            if ( attacker_idx != g_tf2.m_engine->GetLocalPlayer( ) )
            {
                break;
            }

            const auto target_idx{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ATLAS_XORSTR( "userid" ).get( ) ) ) };
            if ( target_idx == g_tf2.m_engine->GetLocalPlayer( ) )
            {
                break;
            }

            const auto damage_amount{ event->GetInt( ATLAS_XORSTR( "damageamount" ).get( ) ) };
            atl::memset( buf, 0, sizeof( buf ) );
            atl::format( buf, sizeof( buf ), ATLAS_XORSTR( "%s took %i damage." ).get( ), g_cl.get_player_name( target_idx ), damage_amount );
            push_notification( notify_kind_t::information, ATLAS_XORSTR_STATIC( "Damage given" ), buf );
            break;
        }
    }
}

void event_notifiers::on_dispatch_user_message( bf_read &msg )
{
    if ( !g_cfg.m_events.m_enabled )
    {
        return;
    }

    const i32 team{ msg.ReadByte( ) };
    msg.ReadLong( );

    const i32 caller_idx{ msg.ReadByte( ) };
    if ( caller_idx == g_tf2.m_engine->GetLocalPlayer( ) )
    {
        return;
    }

    char vote_issue[ 256 ]{ };
    msg.ReadString( vote_issue, sizeof( vote_issue ) );

    char param[ 256 ]{ };
    msg.ReadString( param, sizeof( param ) );
    msg.ReadOneBit( );

    const i32 vote_target_idx = msg.ReadByte( );
    msg.Seek( 0 );
    if ( !caller_idx || !vote_target_idx )
    {
        return;
    }

    static char buf[ 256 ]{ };
    atl::memset( buf, 0, sizeof( buf ) );
    atl::format( buf, sizeof( buf ), ATLAS_XORSTR( "%s has called a vote on %s." ).get( ), g_cl.get_player_name( caller_idx ), g_cl.get_player_name( vote_target_idx ) );
    push_notification( vote_target_idx == g_tf2.m_engine->GetLocalPlayer( ) ? notify_kind_t::warning : notify_kind_t::information, ATLAS_XORSTR_STATIC( "Vote called" ), buf );

    if ( g_cfg.m_events.m_vote_start_notify_party )
    {
        static char party_buf[ 256 ]{ };
        atl::memset( party_buf, 0, sizeof( party_buf ) );
        atl::format( party_buf, sizeof( party_buf ), ATLAS_XORSTR( "[ATLAS] The player %s has called a votekick on %s!" ).get( ), g_cl.get_player_name( caller_idx ), g_cl.get_player_name( vote_target_idx ) );
        TFPartyClient( )->SendPartyChat( party_buf );
    }
}
