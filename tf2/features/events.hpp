#pragma once

#include "link.hpp"

// client side game event listener; server truth for the things netvars cannot see
class game_events final : public IGameEventListener2
{
public:
    bool install( );
    void remove( );

    void FireGameEvent( IGameEvent *event ) override;
    int  GetEventDebugID( ) override;

private:
    bool listen( const char *name );

    bool m_installed{ };
};

extern game_events g_events;
