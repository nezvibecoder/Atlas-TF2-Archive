#pragma once

#include "link.hpp"

class streamer_mode
{
public:
    bool        should_hide_player( i32 idx );
    bool        should_hide_player( steamid_t &steam_id );
    bool        on_paint_traverse( u32 hash );
    const char *get_ideal_player_name( i32 idx, bool is_cheater = false );
};

extern streamer_mode g_streamer_mode;
