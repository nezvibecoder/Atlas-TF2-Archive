#pragma once

#include "base.hpp"

inline constexpr u64 STEAMID64_INDIVIDUAL{ 0x0110000100000000ull };

class steam
{
public:
    bool init( const util::pe_module &steam_api, const api &packages );
    u32  local_account_id( );
    bool is_friend( u32 account_id );
    void open_profile( u32 account_id ) const;
    i32  medium_avatar( u32 account_id ) const;
    bool request_info( u32 account_id ) const;
    bool image_size( i32 image, u32 &width, u32 &height ) const;
    bool image_rgba( i32 image, u8 *out, i32 capacity ) const;

private:
    void *m_user{ };
    void *m_friends{ };
    void *m_utils{ };
    u32   m_local_account{ };
    u16   m_get_steam_id{ api_invalid_index };
    u16   m_has_friend{ api_invalid_index };
    u16   m_activate_overlay{ api_invalid_index };
    u16   m_get_medium_avatar{ api_invalid_index };
    u16   m_request_user_information{ api_invalid_index };
    u16   m_get_image_size{ api_invalid_index };
    u16   m_get_image_rgba{ api_invalid_index };
};

extern steam g_steam;
