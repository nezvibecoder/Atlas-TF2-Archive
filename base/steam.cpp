#include "steam.hpp"

ATLAS_NO_DESTROY steam g_steam{ };

inline constexpr i32 FRIEND_FLAG_IMMEDIATE{ 0x04 };

bool steam::init( const util::pe_module &steam_api, const api &packages )
{
    util::address get_user{ steam_api.find_export( ATLAS_XORSTR( "SteamAPI_GetHSteamUser" ).get( ) ) };
    util::address find_interface{
        steam_api.find_export( ATLAS_XORSTR( "SteamInternal_FindOrCreateUserInterface" ).get( ) )
    };

    if ( get_user.is_null( ) || find_interface.is_null( ) )
    {
        return false;
    }

    this->m_get_steam_id             = packages.get_index_pkg( HASH_CT( "vfunc.steam.user.get_steam_id" ) );
    this->m_has_friend               = packages.get_index_pkg( HASH_CT( "vfunc.steam.friends.has_friend" ) );
    this->m_activate_overlay         = packages.get_index_pkg( HASH_CT( "vfunc.steam.friends.activate_game_overlay_to_user" ) );
    this->m_get_medium_avatar        = packages.get_index_pkg( HASH_CT( "vfunc.steam.friends.get_medium_avatar" ) );
    this->m_request_user_information = packages.get_index_pkg( HASH_CT( "vfunc.steam.friends.request_user_information" ) );
    this->m_get_image_size           = packages.get_index_pkg( HASH_CT( "vfunc.steam.utils.get_image_size" ) );
    this->m_get_image_rgba           = packages.get_index_pkg( HASH_CT( "vfunc.steam.utils.get_image_rgba" ) );

    // a package that never served these would otherwise leave us calling vtable[ 1024 ]
    if ( this->m_get_steam_id >= api_invalid_index || this->m_has_friend >= api_invalid_index ||
         this->m_activate_overlay >= api_invalid_index ||
         this->m_get_medium_avatar >= api_invalid_index ||
         this->m_request_user_information >= api_invalid_index ||
         this->m_get_image_size >= api_invalid_index || this->m_get_image_rgba >= api_invalid_index )
    {
        return false;
    }

    const i32 user{ get_user.call_as< i32 >( ) };
    this->m_user    = find_interface.call_as< void * >( user, ATLAS_XORSTR( "SteamUser023" ).get( ) );
    this->m_friends = find_interface.call_as< void * >( user, ATLAS_XORSTR( "SteamFriends017" ).get( ) );
    this->m_utils   = find_interface.call_as< void * >( 0, ATLAS_XORSTR( "SteamUtils010" ).get( ) );

    return this->m_user != nullptr && this->m_friends != nullptr && this->m_utils != nullptr;
}

u32 steam::local_account_id( )
{
    if ( !this->m_user )
    {
        return 0;
    }

    if ( this->m_local_account )
    {
        return this->m_local_account;
    }

    u64 id{ };
    util::call_virtual< void( ATLAS_THISCALL * )( void *, u64 * ) >(
        this->m_user, this->m_get_steam_id )( this->m_user, &id );

    this->m_local_account = static_cast< u32 >( id );
    return this->m_local_account;
}

bool steam::is_friend( u32 account_id )
{
    if ( !this->m_friends || !account_id )
    {
        return false;
    }

    return util::call_virtual< bool( ATLAS_THISCALL * )( void *, u64, i32 ) >( this->m_friends, this->m_has_friend )(
        this->m_friends, STEAMID64_INDIVIDUAL | account_id, FRIEND_FLAG_IMMEDIATE );
}

void steam::open_profile( u32 account_id ) const
{
    if ( !account_id )
    {
        return;
    }

    util::call_virtual< void( ATLAS_THISCALL * )( void *, const char *, u64 ) >(
        this->m_friends, this->m_activate_overlay )(
        this->m_friends, ATLAS_XORSTR( "steamid" ).get( ),
        STEAMID64_INDIVIDUAL | account_id );
}

i32 steam::medium_avatar( u32 account_id ) const
{
    if ( !this->m_friends || !account_id )
    {
        return 0;
    }

    return util::call_virtual< i32( ATLAS_THISCALL * )( void *, u64 ) >(
        this->m_friends, this->m_get_medium_avatar )(
        this->m_friends, STEAMID64_INDIVIDUAL | account_id );
}

bool steam::request_info( u32 account_id ) const
{
    if ( !this->m_friends || !account_id )
    {
        return false;
    }

    return util::call_virtual< bool( ATLAS_THISCALL * )( void *, u64, bool ) >(
        this->m_friends, this->m_request_user_information )(
        this->m_friends, STEAMID64_INDIVIDUAL | account_id, false );
}

bool steam::image_size( i32 image, u32 &width, u32 &height ) const
{
    if ( !this->m_utils || image <= 0 )
    {
        return false;
    }

    return util::call_virtual< bool( ATLAS_THISCALL * )( void *, i32, u32 *, u32 * ) >(
        this->m_utils, this->m_get_image_size )( this->m_utils, image, &width, &height );
}

bool steam::image_rgba( i32 image, u8 *out, i32 capacity ) const
{
    if ( !this->m_utils || image <= 0 || !out )
    {
        return false;
    }

    return util::call_virtual< bool( ATLAS_THISCALL * )( void *, i32, u8 *, i32 ) >(
        this->m_utils, this->m_get_image_rgba )( this->m_utils, image, out, capacity );
}
