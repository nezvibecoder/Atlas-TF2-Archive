#pragma once

#include "util/typedefs.hpp"

// Expose this globally for everyone to use.
enum anchor_t : i32
{
    ANCHOR_NONE = -1,
    ANCHOR_TOP_LEFT,
    ANCHOR_TOP_CENTER,
    ANCHOR_TOP_RIGHT,
    ANCHOR_RIGHT,
    ANCHOR_BOTTOM_RIGHT,
    ANCHOR_BOTTOM_CENTER,
    ANCHOR_BOTTOM_LEFT,
    ANCHOR_LEFT,
    ANCHOR_COUNT,
};

struct watermark_values_t
{
    const char *m_user{ };
    i32         m_latency{ };
    i32         m_fake_latency{ };       // what the acks add on top, as the server measures it
    i32         m_scoreboard_latency{ }; // the ping the server reports back for us
    bool        m_connected{ };
    i32        *m_watermark_position{ };
};

struct watermark_boot_t
{
    const char *m_caption{ };
    i32         m_caption_width{ };
    const char *m_counter{ };
    float       m_fill{ };
    u8          m_line{ };
    u8          m_opacity{ };
};

void draw_watermark( i32 screen_width, i32 screen_height, const watermark_values_t &values );
void draw_watermark_boot( i32 screen_width, i32 screen_height, const watermark_boot_t &boot );
void launch_watermark( );
