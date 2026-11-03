#pragma once

#include "runtime/vector.hpp"

#include "link.hpp"
#include "skins.hpp"

class skin_share
{
public:
    void                    update( );
    const shared_loadout_t *loadout( u32 account ) const;
    bool                    is_peer( u32 account ) const;

    bool empty( ) const
    {
        return this->m_peers.empty( );
    }

private:
    struct peer_t
    {
        u32              m_account{ };
        shared_loadout_t m_loadout{ };
    };

    static void ATLAS_STDCALL worker( void *context );
    bool                      start( );
    bool                      parse( atl::vector< peer_t > &peers ) const;

    atl::vector< peer_t > m_peers{ };
    atl::vector< char >   m_request{ };
    atl::vector< char >   m_response{ };
    volatile long         m_state{ };
    u32                   m_status{ };
    u64                   m_next_sync{ };
    bool                  m_success{ };
};

extern skin_share g_skin_share;
