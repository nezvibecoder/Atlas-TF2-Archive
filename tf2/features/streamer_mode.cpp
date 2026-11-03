#include "streamer_mode.hpp"

#include "steam.hpp"

#include "entity_cache.hpp"

ATLAS_NO_DESTROY streamer_mode g_streamer_mode{ };

bool streamer_mode::should_hide_player( i32 idx )
{
    if ( !g_cfg.m_streamer_mode.m_enabled )
    {
        return false;
    }

    // m_local is null during level transitions while IsInGame is already true, and the team check below needs it..
    if ( !g_cl.m_local || !g_cl.m_resource )
    {
        return true;
    }

    if ( idx == g_tf2.m_engine->GetLocalPlayer( ) && g_cfg.m_streamer_mode.m_hide_targets[ config::STREAMER_MODE_HIDE_LOCAL ] == true )
    {
        return true;
    }

    player_info_t info{ };
    if ( !g_tf2.m_engine->GetPlayerInfo( idx, &info ) )
    {
        return false;
    }

    if ( g_steam.is_friend( info.friendsID ) && g_cfg.m_streamer_mode.m_hide_targets[ config::STREAMER_MODE_HIDE_STEAM_FRIENDS ] == true )
    {
        return true;
    }

    if ( g_cl.is_party_member( info.friendsID ) && g_cfg.m_streamer_mode.m_hide_targets[ config::STREAMER_MODE_HIDE_PARTY_MEMBERS ] == true )
    {
        return true;
    }

    const i32 team{ g_cl.m_resource->m_iTeam( )[ idx ] };
    switch ( team )
    {
        case TF_TEAM_RED:
        case TF_TEAM_BLUE: break;
        default: return true;
    }

    const bool is_teammate{ team == g_cl.m_local->m_iTeamNum( ) };
    if ( is_teammate && g_cfg.m_streamer_mode.m_hide_targets[ config::STREAMER_MODE_HIDE_TEAMMATES ] == true )
    {
        return true;
    } else if ( !is_teammate && g_cfg.m_streamer_mode.m_hide_targets[ config::STREAMER_MODE_HIDE_ENEMY ] == true )
    {
        return true;
    }

    return false;
}

bool streamer_mode::should_hide_player( steamid_t &steam_id )
{
    if ( !g_cfg.m_streamer_mode.m_enabled )
    {
        return false;
    }

    if ( g_cl.is_friend( steam_id.m_steamid.m_comp.account_id ) && g_cfg.m_streamer_mode.m_hide_targets[ config::STREAMER_MODE_HIDE_STEAM_FRIENDS ] == true )
    {
        return true;
    }

    if ( g_cl.is_party_member( steam_id.m_steamid.m_comp.account_id ) && g_cfg.m_streamer_mode.m_hide_targets[ config::STREAMER_MODE_HIDE_PARTY_MEMBERS ] == true )
    {
        return true;
    }

    if ( g_cl.is_atlas_user( steam_id.m_steamid.m_comp.account_id ) && g_cfg.m_streamer_mode.m_hide_targets[ config::STREAMER_MODE_HIDE_ATLAS_USERS ] == true )
    {
        return true;
    }

    for ( i32 i{ 1 }; i < g_tf2.m_engine->GetMaxClients( ); ++i )
    {
        IClientEntity *client_entity{ g_tf2.m_entity_list->GetClientEntity( i ) };
        ClientClass   *client_class{ client_entity ? client_entity->GetClientClass( ) : nullptr };
        if ( !client_class )
        {
            continue;
        }

        if ( client_class->GetClassID( ) != ETFClassID::CTFPlayer )
        {
            continue;
        }

        player_info_t info{ };
        if ( !g_tf2.m_engine->GetPlayerInfo( i, &info ) )
        {
            continue;
        }

        if ( info.friendsID != steam_id.m_steamid.m_comp.account_id )
        {
            continue;
        }

        if ( !this->should_hide_player( i ) )
        {
            continue;
        }

        return true;
    }

    return false;
}

bool streamer_mode::on_paint_traverse( u32 hash )
{
    if ( !g_cfg.m_streamer_mode.m_enabled || g_cfg.m_streamer_mode.m_hide_targets[ config::STREAMER_MODE_HIDE_SENSITIVE_PANELS ] == false )
    {
        return true;
    }

    switch ( hash )
    {
        case HASH_CT( "avatar" ):
        case HASH_CT( "SteamFriendsList" ):
        case HASH_CT( "RankPanel" ):
        case HASH_CT( "TooltipPanel" ):
        case HASH_CT( "ServerLabelNew" ):
        case HASH_CT( "ServerLabel" ):
        case HASH_CT( "mapname" ):
        case HASH_CT( "itempanel" ):
        case HASH_CT( "DifficultyContainer" ):
        case HASH_CT( "PopFileLabel" ):
        case HASH_CT( "MvMPlayerList" ):
        case HASH_CT( "RedLeaderAvatar" ):
        case HASH_CT( "BLueLeaderAvatar" ):
        case HASH_CT( "PlayerAvatar" ):
        case HASH_CT( "Player1Avatar" ):
        case HASH_CT( "Player2Avatar" ):
        case HASH_CT( "Player3Avatar" ):
        case HASH_CT( "Player4Avatar" ):
        case HASH_CT( "KillStreakPlayer1Avatar" ):
        case HASH_CT( "KillStreakPlayer2Avatar" ):
        case HASH_CT( "KillStreakPlayer3Avatar" ):
        case HASH_CT( "KillStreakPlayer4Avatar" ): return false;
        default: return true;
    }
}

const char *streamer_mode::get_ideal_player_name( i32 idx, bool is_cheater )
{
    Assert( g_cfg.m_streamer_mode.m_enabled != false && "How the hell did we get here?" );

    if ( idx == g_tf2.m_engine->GetLocalPlayer( ) && g_cfg.m_streamer_mode.m_hide_targets[ config::STREAMER_MODE_HIDE_LOCAL ] == true )
    {
        return ATLAS_XORSTR_STATIC( "You" );
    }

    player_info_t info{ };
    if ( !g_tf2.m_engine->GetPlayerInfo( idx, &info ) || !g_cl.m_local || !g_cl.m_resource )
    {
        return ATLAS_XORSTR_STATIC( "Player" );
    }

    if ( g_cl.is_friend( info.friendsID ) && g_cfg.m_streamer_mode.m_hide_targets[ config::STREAMER_MODE_HIDE_STEAM_FRIENDS ] == true )
    {
        return ATLAS_XORSTR_STATIC( "Friend" );
    }

    if ( g_cl.is_party_member( info.friendsID ) && g_cfg.m_streamer_mode.m_hide_targets[ config::STREAMER_MODE_HIDE_PARTY_MEMBERS ] == true )
    {
        return ATLAS_XORSTR_STATIC( "Party Member" );
    }

    if ( g_cl.is_atlas_user( info.friendsID ) && g_cfg.m_streamer_mode.m_hide_targets[ config::STREAMER_MODE_HIDE_ATLAS_USERS ] == true )
    {
        return ATLAS_XORSTR_STATIC( "Atlas User" );
    }

    const i32 team{ g_cl.m_resource->m_iTeam( )[ idx ] };
    switch ( team )
    {
        case TF_TEAM_RED:
        case TF_TEAM_BLUE: break;
        default: return ATLAS_XORSTR_STATIC( "Spectator" );
    }

    const bool is_teammate{ team == g_cl.m_local->m_iTeamNum( ) };
    if ( is_teammate && g_cfg.m_streamer_mode.m_hide_targets[ config::STREAMER_MODE_HIDE_TEAMMATES ] == true )
    {
        return is_cheater ? ATLAS_XORSTR_STATIC( "Teammate Cheater" ) : ATLAS_XORSTR_STATIC( "Teammate" );
    } else if ( !is_teammate && g_cfg.m_streamer_mode.m_hide_targets[ config::STREAMER_MODE_HIDE_ENEMY ] == true )
    {
        return is_cheater ? ATLAS_XORSTR_STATIC( "Enemy Cheater" ) : ATLAS_XORSTR_STATIC( "Enemy Player" );
    }

    return ATLAS_XORSTR_STATIC( "Player" );
}
