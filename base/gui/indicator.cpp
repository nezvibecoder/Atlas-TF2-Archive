#include "input/input.hpp"
#include "runtime/assert.hpp"
#include "math/math.hpp"
#include "render/render.hpp"

#include "gui/gui.hpp"

#include "gui/indicator.hpp"

constexpr i32 INDICATOR_PADDING{ 6 };
constexpr i32 INDICATOR_WELL_PADDING{ 5 };
constexpr i32 INDICATOR_WELL_GAP{ 4 };
constexpr i32 INDICATOR_SHADOW_SPREAD{ 14 };
constexpr i32 INDICATOR_SHADOW_DROP{ 3 };
constexpr i32 INDICATOR_GRID{ INDICATOR_MARGIN };

constexpr float   INDICATOR_FILL_GLIDE{ 140.0f };
constexpr u32     INDICATOR_PULSE_PERIOD{ 2000 };
constexpr u8      INDICATOR_PULSE_FLOOR{ 199 };
constexpr u8      INDICATOR_RULE_PEAK{ 130 };
constexpr color_t INDICATOR_TITLE_RULE{ 5, 5, 6, 140 };
constexpr color_t INDICATOR_SHADOW{ 0, 0, 0, 118 };

constexpr usize INDICATOR_CARD_LIMIT{ 6 };

static const indicator_card_t *s_dragged{ };
static const indicator_card_t *s_cards[ INDICATOR_CARD_LIMIT ]{ };

ATLAS_FORCE_INLINE i32 tiny_text_y( i32 top, i32 height )
{
    return top + ( height - TINY_FONT_INK_HEIGHT ) / 2 - TINY_FONT_INK_TOP;
}

static void drive_drag( indicator_card_t &card, rect_t bounds )
{
    const point_t mouse{ g_input.mouse( ) };

    if ( s_dragged != &card )
    {
        if ( s_dragged || g_gui.dragging( ) || !g_input.pressed( VK_LBUTTON ) || !g_gui.hit_test( bounds ) )
        {
            return;
        }

        card.m_grab = point_t{ mouse.x - bounds.left, mouse.y - bounds.top };
        s_dragged   = &card;
    }

    card.m_at = point_t{ mouse.x - card.m_grab.x, mouse.y - card.m_grab.y };

    if ( !g_input.down( VK_LBUTTON ) )
    {
        s_dragged = nullptr;
    }
}

static void enlist( const indicator_card_t &card )
{
    for ( const indicator_card_t *slot : s_cards )
    {
        if ( slot == &card )
        {
            return;
        }
    }

    for ( const indicator_card_t *&slot : s_cards )
    {
        if ( !slot )
        {
            slot = &card;
            return;
        }
    }

    Assert( false ); // more cards than INDICATOR_CARD_LIMIT
}

ATLAS_FORCE_INLINE i32 snap_to_grid( i32 value )
{
    return ( value + INDICATOR_GRID / 2 ) / INDICATOR_GRID * INDICATOR_GRID;
}

static void separate( indicator_card_t &card, i32 screen_width, i32 screen_height )
{
    for ( const indicator_card_t *other : s_cards )
    {
        if ( !other || other == &card )
        {
            continue;
        }

        const i32 dx{ card.m_at.x - other->m_at.x };
        const i32 dy{ card.m_at.y - other->m_at.y };

        if ( dx <= -card.m_width || dx >= other->m_width || dy <= -card.m_height || dy >= other->m_height )
        {
            continue;
        }

        const i32  push_x{ dx < 0 ? other->m_at.x - card.m_width : other->m_at.x + other->m_width };
        const i32  push_y{ dy < 0 ? other->m_at.y - card.m_height : other->m_at.y + other->m_height };
        const i32  depth_x{ math::smaller( card.m_at.x + card.m_width, other->m_at.x + other->m_width ) -
                           math::bigger( card.m_at.x, other->m_at.x ) };
        const i32  depth_y{ math::smaller( card.m_at.y + card.m_height, other->m_at.y + other->m_height ) -
                           math::bigger( card.m_at.y, other->m_at.y ) };
        const bool fits_x{ push_x >= 0 && push_x <= screen_width - card.m_width };
        const bool fits_y{ push_y >= 0 && push_y <= screen_height - card.m_height };

        if ( fits_x && ( depth_x <= depth_y || !fits_y ) )
        {
            card.m_at.x = push_x;
        } else if ( fits_y )
        {
            card.m_at.y = push_y;
        }
    }
}

void hide_indicator( indicator_card_t &card )
{
    if ( s_dragged == &card )
    {
        s_dragged = nullptr;
    }

    for ( const indicator_card_t *&slot : s_cards )
    {
        if ( slot == &card )
        {
            slot = nullptr;
        }
    }

    card.m_enter = 0;
    card.m_hover = 0;
    card.m_fill  = 0.0f;
}

u32 step_indicator( indicator_card_t &card, float fill, i32 screen_width, i32 screen_height,
                    i32 width, i32 height, bool visible )
{
    card.m_width  = width;
    card.m_height = height;
    const u32 delta{ gui::animation_delta( card.m_tick ) };

    gui::step_mix( card.m_enter, visible, delta, CONTENT_ENTER_DURATION );
    card.m_fill = gui::approach_float( card.m_fill, fill, delta, INDICATOR_FILL_GLIDE );

    enlist( card );

    const rect_t anchor{ card.m_at.x, card.m_at.y, card.m_at.x + width, card.m_at.y + height };

    if ( visible && card.m_enter == 255 )
    {
        drive_drag( card, anchor );
    }

    const bool hovered{ s_dragged == &card ||
                        ( g_gui.can_interact( ) && !g_gui.dragging( ) && g_gui.hit_test( anchor ) ) };

    gui::step_mix( card.m_hover, hovered, delta, WIDGET_HOVER_DURATION );

    if ( s_dragged == &card && g_theme.m_snap_indicators )
    {
        card.m_at = point_t{ snap_to_grid( card.m_at.x ), snap_to_grid( card.m_at.y ) };
    }

    card.m_at.x = math::clamp( card.m_at.x, 0, screen_width - width );
    card.m_at.y = math::clamp( card.m_at.y, 0, screen_height - height );

    separate( card, screen_width, screen_height );

    return delta;
}

indicator_frame_t draw_indicator_frame( const indicator_card_t &card, const char *title,
                                        color_t title_color, bool filled )
{
    const u8  enter{ gui::eased_out( card.m_enter ) };
    const i32 rise{ CONTENT_ENTER_RISE - CONTENT_ENTER_RISE * enter / 255 };
    const u8  glow{ gui::eased_mix( card.m_hover ) };

    const rect_t bounds{ card.m_at.x, card.m_at.y + rise, card.m_at.x + card.m_width,
                         card.m_at.y + rise + card.m_height };

    gui::draw_drop_shadow( bounds, INDICATOR_SHADOW_SPREAD, INDICATOR_SHADOW_DROP,
                           gui::with_alpha( INDICATOR_SHADOW, enter ) );

    if ( filled )
    {
        g_render.box( bounds, gui::with_alpha( CARD_BACKGROUND, enter ), true );
    }
    g_render.box( rect_t{ bounds.left, bounds.top, bounds.right - 1, bounds.bottom - 1 },
                  gui::with_alpha( gui::mix_color( CARD_BORDER, g_theme.m_accent, glow ), enter ) );

    const rect_t title_row{ bounds.left + 1, bounds.top + 1, bounds.right - 1,
                            bounds.top + 1 + INDICATOR_TITLE_HEIGHT };
    const i32    title_y{ tiny_text_y( title_row.top, INDICATOR_TITLE_HEIGHT ) };

    g_render.box( title_row, gui::with_alpha( CARD_TITLE_BACKGROUND, enter ), true );
    g_render.box( rect_t{ title_row.left, title_row.bottom, title_row.right, title_row.bottom + 1 },
                  gui::with_alpha( INDICATOR_TITLE_RULE, enter ), true );

    gui::draw_falloff_band( rect_t{ title_row.left, title_row.bottom + 1, title_row.right,
                                    title_row.bottom + 1 + INDICATOR_RULE_DEPTH },
                            true, gui::with_alpha( gui::with_alpha( SCROLL_SHADOW, INDICATOR_RULE_PEAK ), enter ) );

    g_render.text( MENU_FONT_TINY( ), point_t{ title_row.left + INDICATOR_PADDING, title_y },
                   gui::with_alpha( title_color, enter ), title, false, render::text_align::left );

    return indicator_frame_t{ enter,
                              rect_t{ bounds.left + 1 + INDICATOR_PADDING,
                                      title_row.bottom + 1 + INDICATOR_RULE_DEPTH,
                                      bounds.right - 1 - INDICATOR_PADDING,
                                      bounds.bottom - 1 - INDICATOR_RULE_DEPTH },
                              point_t{ title_row.right - INDICATOR_PADDING, title_y } };
}

indicator_layout_t draw_indicator( const indicator_card_t &card, const char *title, color_t wash, color_t edge )
{
    const indicator_frame_t frame{ draw_indicator_frame( card, title ) };
    const u8                enter{ frame.m_enter };

    const rect_t well{ frame.m_content };

    g_render.box( well, gui::with_alpha( CONTROL_BACKGROUND, enter ), true );
    g_render.box( rect_t{ well.left, well.top, well.right - 1, well.bottom - 1 },
                  gui::with_alpha( CONTROL_BORDER, enter ) );

    const rect_t inner{ well.left + 1, well.top + 1, well.right - 1, well.bottom - 1 };
    const i32    washed{ static_cast< i32 >( card.m_fill * static_cast< float >( inner.right - inner.left ) +
                                             0.5f ) };
    const i32    fill_edge{ math::smaller( inner.left + washed, inner.right - 1 ) };

    g_render.box( rect_t{ inner.left, inner.top, inner.left + washed, inner.bottom },
                  gui::with_alpha( wash, enter ), true );
    g_render.box( rect_t{ fill_edge, inner.top, fill_edge + 1, inner.bottom }, gui::with_alpha( edge, enter ),
                  true );

    const i32 glyph_left{ inner.left + INDICATOR_WELL_PADDING };

    return indicator_layout_t{ enter, frame.m_state,
                               point_t{ glyph_left, inner.top + ( inner.bottom - inner.top - MENU_ICON_SIZE ) / 2 },
                               point_t{ glyph_left + MENU_ICON_SIZE + INDICATOR_WELL_GAP,
                                        tiny_text_y( inner.top, inner.bottom - inner.top ) } };
}

u8 indicator_glyph_alpha( u8 ready )
{
    const u8 wave{ static_cast< u8 >( 255 - gui::pulse( gui::clock( ), INDICATOR_PULSE_PERIOD ) ) };
    return gui::mix_channel( 255, gui::mix_channel( 255, INDICATOR_PULSE_FLOOR, wave ), ready );
}

void draw_indicator_well( rect_t well, u8 enter )
{
    g_render.box( well, gui::with_alpha( CONTROL_BACKGROUND, enter ), true );
    g_render.box( rect_t{ well.left, well.top, well.right - 1, well.bottom - 1 },
                  gui::with_alpha( CONTROL_BORDER, enter ) );
}

void draw_indicator_stripe( rect_t well, u8 enter, color_t stripe )
{
    g_render.box( rect_t{ well.left + 1, well.top + 1, well.left + 2, well.bottom - 1 },
                  gui::with_alpha( stripe, enter ), true );
}

i32 indicator_text_y( rect_t well )
{
    return tiny_text_y( well.top, well.bottom - well.top );
}
