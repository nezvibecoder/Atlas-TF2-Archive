#pragma once

#include "link.hpp"

const char *player_class_name( i32 player_class );

class event_notifiers
{
public:
    void on_game_event( IGameEvent *event, const hash_t &hash );
    void on_dispatch_user_message( bf_read &msg );
};

extern event_notifiers g_event_notifiers;
