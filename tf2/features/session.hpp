#pragma once

#include "runtime/atomic.hpp"
#include "runtime/vector.hpp"

class session
{
public:
    bool startup( );
    bool poll( );
    void shutdown( );

private:
    static void ATLAS_STDCALL worker( void *context );
    bool                      accept_grant( const atl::vector< char > &response, u64 started );

    char                m_session_id[ 37 ]{ };
    atl::atomic< u64 >  m_expires_at{ };
    atl::atomic< bool > m_stopping{ };
    atl::atomic< bool > m_revoked{ };
};

extern session g_session;
