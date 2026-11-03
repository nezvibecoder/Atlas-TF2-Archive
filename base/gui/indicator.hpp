#pragma once

#include "util/typedefs.hpp"

#include "gui/theme.hpp"

inline constexpr i32 INDICATOR_MARGIN{ 12 };
inline constexpr i32 INDICATOR_WIDTH{ 146 };
inline constexpr i32 INDICATOR_TITLE_HEIGHT{ 15 };
inline constexpr i32 INDICATOR_RULE_DEPTH{ 5 };
inline constexpr i32 INDICATOR_WELL_HEIGHT{ MENU_ICON_SIZE + 4 };
inline constexpr i32 INDICATOR_HEIGHT{ 2 + INDICATOR_TITLE_HEIGHT + 1 + INDICATOR_RULE_DEPTH * 2 + INDICATOR_WELL_HEIGHT };

struct indicator_card_t
{
    point_t m_at{ };
    point_t m_grab{ };
    i32     m_width{ INDICATOR_WIDTH };
    i32     m_height{ INDICATOR_HEIGHT };
    u64     m_tick{ };
    float   m_fill{ };
    u8      m_enter{ };
    u8      m_hover{ };
};

struct indicator_frame_t
{
    u8      m_enter{ };
    rect_t  m_content{ };
    point_t m_state{ };
};

struct indicator_layout_t
{
    u8      m_enter{ };
    point_t m_state{ };
    point_t m_glyph{ };
    point_t m_amount{ };
};

void               hide_indicator( indicator_card_t &card );
u32                step_indicator( indicator_card_t &card, float fill, i32 screen_width, i32 screen_height,
                                   i32 width = INDICATOR_WIDTH, i32 height = INDICATOR_HEIGHT,
                                   bool visible = true );
indicator_frame_t  draw_indicator_frame( const indicator_card_t &card, const char *title,
                                         color_t title_color = CONTROL_LABEL, bool filled = true );
indicator_layout_t draw_indicator( const indicator_card_t &card, const char *title, color_t wash, color_t edge );
u8                 indicator_glyph_alpha( u8 ready );
void               draw_indicator_well( rect_t well, u8 enter );
void               draw_indicator_stripe( rect_t well, u8 enter, color_t stripe );
i32                indicator_text_y( rect_t well );
