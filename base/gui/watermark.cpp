#include "input/input.hpp"
#include "math/math.hpp"
#include "render/render.hpp"
#include "util/format.hpp"
#include "util/string.hpp"
#include "util/time.hpp"

#include "gui/gui.hpp"
#include "gui/theme.hpp"

#include "gui/watermark.hpp"

constexpr i32   WATERMARK_HEIGHT{ 20 }; // 1px frame + 18px row + 1px frame
constexpr i32   WATERMARK_MARGIN{ 12 };
constexpr i32   WATERMARK_EDGE{ 3 };
constexpr i32   WATERMARK_LOGO_PAD{ 7 };
constexpr i32   WATERMARK_PADDING{ 8 };
constexpr i32   WATERMARK_GAP{ 7 };
constexpr i32   WATERMARK_PING_WARNING_MS{ 80 };
constexpr u32   WATERMARK_SAMPLE_WINDOW{ 500 };
constexpr i32   WATERMARK_MAX_CELLS{ 4 };
constexpr float WATERMARK_GLIDE{ 95.0f };
constexpr u32   WATERMARK_REVEAL{ 180 };

constexpr i32 ANCHOR_ARM_REACH{ 170 };
constexpr i32 ANCHOR_GLOW_REACH{ 72 };
constexpr u32 ANCHOR_GLOW_PEAK{ 130 };
constexpr u32 ANCHOR_GLOW_QUANTUM{ 10 };

struct cell_t
{
    const char *m_text{ };
    color_t     m_color{ };
    i32         m_width{ };
};

struct counter_t
{
    u32 m_frames{ };
    u32 m_elapsed{ };
    i32 m_value{ };
};

static counter_t s_counter{ };
static u64       s_tick{ };

// static i32     s_anchor{ ANCHOR_TOP_RIGHT };
static point_t s_free{ }; // top left corner while the strip is unanchored
static point_t s_grab{ }; // cursor offset inside the strip when the drag started
static bool    s_dragging{ };

static float s_render_x{ };
static float s_render_y{ };
static bool  s_placed{ };

static bool s_launched{ };
static u8   s_reveal{ 255 };

static void step_counter( u32 delta )
{
    counter_t &counter{ s_counter };

    ++counter.m_frames;
    counter.m_elapsed += delta;

    if ( counter.m_elapsed < WATERMARK_SAMPLE_WINDOW )
    {
        return;
    }

    counter.m_value   = static_cast< i32 >( counter.m_frames * 1000u / counter.m_elapsed );
    counter.m_frames  = 0;
    counter.m_elapsed = 0;
}

static void push_cell( cell_t ( &cells )[ WATERMARK_MAX_CELLS ], i32 &count, const char *text,
                       color_t value )
{
    if ( count >= WATERMARK_MAX_CELLS || !text || !*text )
    {
        return;
    }

    cells[ count ].m_text  = text;
    cells[ count ].m_color = value;
    cells[ count ].m_width = gui::estimate_text_width( text );
    ++count;
}

// no zero padding in str::format_to, and the clock is the only thing that wants it.
static void write_clock( char ( &out )[ 8 ], u16 hour, u16 minute )
{
    out[ 0 ] = static_cast< char >( '0' + hour / 10u % 10u );
    out[ 1 ] = static_cast< char >( '0' + hour % 10u );
    out[ 2 ] = ':';
    out[ 3 ] = static_cast< char >( '0' + minute / 10u % 10u );
    out[ 4 ] = static_cast< char >( '0' + minute % 10u );
    out[ 5 ] = 0;
}

static rect_t anchor_rect( i32 anchor, i32 width, i32 height, i32 margin, i32 screen_width,
                           i32 screen_height )
{
    i32 left{ margin };
    i32 top{ margin };

    switch ( anchor )
    {
        case ANCHOR_TOP_CENTER:
        case ANCHOR_BOTTOM_CENTER: left = ( screen_width - width ) / 2; break;
        case ANCHOR_TOP_RIGHT:
        case ANCHOR_RIGHT:
        case ANCHOR_BOTTOM_RIGHT: left = screen_width - margin - width; break;
        default: break;
    }

    switch ( anchor )
    {
        case ANCHOR_LEFT:
        case ANCHOR_RIGHT: top = ( screen_height - height ) / 2; break;
        case ANCHOR_BOTTOM_LEFT:
        case ANCHOR_BOTTOM_CENTER:
        case ANCHOR_BOTTOM_RIGHT: top = screen_height - margin - height; break;
        default: break;
    }

    return rect_t{ left, top, left + width, top + height };
}

static point_t anchor_origin( i32 anchor, i32 screen_width, i32 screen_height )
{
    point_t origin{ 0, 0 };

    switch ( anchor )
    {
        case ANCHOR_TOP_CENTER:
        case ANCHOR_BOTTOM_CENTER: origin.x = screen_width / 2; break;
        case ANCHOR_TOP_RIGHT:
        case ANCHOR_RIGHT:
        case ANCHOR_BOTTOM_RIGHT: origin.x = screen_width; break;
        default: break;
    }

    switch ( anchor )
    {
        case ANCHOR_LEFT:
        case ANCHOR_RIGHT: origin.y = screen_height / 2; break;
        case ANCHOR_BOTTOM_LEFT:
        case ANCHOR_BOTTOM_CENTER:
        case ANCHOR_BOTTOM_RIGHT: origin.y = screen_height; break;
        default: break;
    }

    return origin;
}

static point_t rect_center( rect_t bounds )
{
    return point_t{ ( bounds.left + bounds.right ) / 2,
                    ( bounds.top + bounds.bottom ) / 2 };
}

static float distance_between( point_t from, point_t to )
{
    const float dx{ static_cast< float >( to.x - from.x ) };
    const float dy{ static_cast< float >( to.y - from.y ) };

    return math::sqrt( dx * dx + dy * dy );
}

static i32 armed_anchor( rect_t strip, i32 margin, i32 screen_width, i32 screen_height,
                         u8 &strength )
{
    const point_t center{ rect_center( strip ) };
    const i32     width{ strip.right - strip.left };
    const i32     height{ strip.bottom - strip.top };
    const float   reach{ static_cast< float >( g_gui.scaled( ANCHOR_ARM_REACH ) ) };

    i32   nearest{ ANCHOR_NONE };
    float closest{ };

    for ( i32 anchor{ }; anchor < ANCHOR_COUNT; ++anchor )
    {
        const rect_t candidate{ anchor_rect( anchor, width, height, margin, screen_width,
                                             screen_height ) };
        const float  distance{ distance_between( center, rect_center( candidate ) ) };

        if ( nearest == ANCHOR_NONE || distance < closest )
        {
            nearest = anchor;
            closest = distance;
        }
    }

    if ( nearest == ANCHOR_NONE || closest >= reach )
    {
        strength = 0;
        return ANCHOR_NONE;
    }

    strength = static_cast< u8 >(
        math::smooth_step( 1.0f - closest / reach ) * 255.0f + 0.5f );

    return strength ? nearest : ANCHOR_NONE;
}

static u8 glow_alpha( float reached, u8 strength )
{
    if ( reached >= 1.0f )
    {
        return 0;
    }

    const u32 peak{ ANCHOR_GLOW_PEAK * static_cast< u32 >( strength ) / 255u };
    u32       value{ static_cast< u32 >(
        math::smooth_step( 1.0f - reached ) * static_cast< float >( peak ) + 0.5f ) };

    value -= value % ANCHOR_GLOW_QUANTUM;
    return static_cast< u8 >( value );
}

static void draw_anchor_glow( point_t origin, color_t accent, u8 strength, i32 screen_width,
                              i32 screen_height )
{
    const i32 reach{ g_gui.scaled( ANCHOR_GLOW_REACH ) };

    if ( reach <= 0 || !strength )
    {
        return;
    }

    const i32 first_row{ math::clamp( origin.y - reach, 0, screen_height ) };
    const i32 last_row{ math::clamp( origin.y + reach, 0, screen_height ) };
    const i32 first_column{ math::clamp( origin.x - reach, 0, screen_width ) };
    const i32 last_column{ math::clamp( origin.x + reach, 0, screen_width ) };

    const float span{ static_cast< float >( reach ) };

    for ( i32 row{ first_row }; row < last_row; ++row )
    {
        const float dy{ static_cast< float >( row - origin.y ) };

        i32 run_first{ first_column };
        u8  run_alpha{ };

        for ( i32 column{ first_column }; column <= last_column; ++column )
        {
            u8 alpha{ };

            if ( column < last_column )
            {
                const float dx{ static_cast< float >( column - origin.x ) };

                alpha = glow_alpha( math::sqrt( dx * dx + dy * dy ) / span, strength );
            }

            if ( alpha == run_alpha )
            {
                continue;
            }

            if ( run_alpha )
            {
                g_render.box( rect_t{ run_first, row, column, row + 1 },
                              gui::with_alpha( accent, run_alpha ), true );
            }

            run_first = column;
            run_alpha = alpha;
        }
    }
}

static rect_t resolved_rect( i32 width, i32 height, i32 margin, i32 screen_width, i32 screen_height, i32 *pos )
{
    if ( *pos == ANCHOR_NONE )
    {
        return rect_t{ s_free.x, s_free.y, s_free.x + width, s_free.y + height };
    }

    return anchor_rect( *pos, width, height, margin, screen_width, screen_height );
}

static rect_t render_rect( i32 width, i32 height )
{
    const i32 left{ static_cast< i32 >( s_render_x + 0.5f ) };
    const i32 top{ static_cast< i32 >( s_render_y + 0.5f ) };

    return rect_t{ left, top, left + width, top + height };
}

static void step_placement( u32 delta, rect_t target )
{
    const float left{ static_cast< float >( target.left ) };
    const float top{ static_cast< float >( target.top ) };

    if ( s_dragging )
    {
        s_render_x = left;
        s_render_y = top;
        return;
    }

    s_render_x = gui::approach_float( s_render_x, left, delta, WATERMARK_GLIDE );
    s_render_y = gui::approach_float( s_render_y, top, delta, WATERMARK_GLIDE );

    if ( math::abs( s_render_x - left ) < 0.5f )
    {
        s_render_x = left;
    }

    if ( math::abs( s_render_y - top ) < 0.5f )
    {
        s_render_y = top;
    }
}

static void drive_drag( rect_t strip, i32 width, i32 height, i32 margin, i32 screen_width, i32 screen_height, i32 *pos )
{
    const point_t mouse{ g_input.mouse( ) };

    if ( !s_dragging )
    {
        if ( g_gui.dragging( ) || !g_input.pressed( VK_LBUTTON ) ||
             !g_gui.hit_test( strip ) )
        {
            return;
        }

        s_grab     = point_t{ mouse.x - strip.left, mouse.y - strip.top };
        *pos       = ANCHOR_NONE;
        s_dragging = true;
    }

    s_free.x = math::clamp( mouse.x - s_grab.x, 0, screen_width - width );
    s_free.y = math::clamp( mouse.y - s_grab.y, 0, screen_height - height );

    if ( g_input.down( VK_LBUTTON ) )
    {
        return;
    }

    u8 strength{ };

    *pos = armed_anchor(
        rect_t{ s_free.x, s_free.y, s_free.x + width, s_free.y + height }, margin,
        screen_width, screen_height, strength );

    s_dragging = false;
}

static i32 strip_width( const cell_t ( &cells )[ WATERMARK_MAX_CELLS ], i32 count )
{
    const i32 padding{ g_gui.scaled( WATERMARK_PADDING ) };
    const i32 gap{ g_gui.scaled( WATERMARK_GAP ) };

    i32 values_width{ count > 0 ? padding * 2 + gap * ( count - 1 ) : 0 };

    for ( i32 index{ }; index < count; ++index )
    {
        values_width += cells[ index ].m_width;
    }

    return 1 + g_gui.scaled( WATERMARK_EDGE ) + g_gui.scaled( WATERMARK_LOGO_PAD ) * 2 +
           g_gui.scaled( MENU_LOGO_SIZE ) + 1 + values_width + 1;
}

static rect_t centered_rect( i32 width, i32 height, i32 screen_width, i32 screen_height )
{
    const i32 left{ ( screen_width - width ) / 2 };
    const i32 top{ ( screen_height - height ) / 2 };

    return rect_t{ left, top, left + width, top + height };
}

static i32 draw_strip( rect_t strip, color_t frame, const cell_t ( &cells )[ WATERMARK_MAX_CELLS ],
                       i32 count, u8 opacity, u8 ink )
{
    const i32 edge{ g_gui.scaled( WATERMARK_EDGE ) };
    const i32 logo_pad{ g_gui.scaled( WATERMARK_LOGO_PAD ) };
    const i32 logo_slot{ g_gui.scaled( MENU_LOGO_SIZE ) };

    g_render.box( strip, gui::with_alpha( POPUP_BACKGROUND, opacity ), true );
    g_render.box( rect_t{ strip.left, strip.top, strip.right - 1, strip.bottom - 1 },
                  gui::with_alpha( frame, opacity ) );

    const i32 inner_left{ strip.left + 1 };
    const i32 inner_top{ strip.top + 1 };
    const i32 inner_bottom{ strip.bottom - 1 };
    const i32 row_height{ inner_bottom - inner_top };

    g_render.box( rect_t{ inner_left, inner_top, inner_left + edge, inner_bottom },
                  gui::with_alpha( g_theme.m_accent, opacity ), true );

    const i32 mark_left{ inner_left + edge };
    const i32 mark_right{ mark_left + logo_pad * 2 + logo_slot };

    gui::draw_logo( point_t{ mark_left + logo_pad,
                             inner_top + ( row_height - logo_slot ) / 2 },
                    gui::with_alpha( MENU_LOGO_GLYPH, opacity ) );

    g_render.box( rect_t{ mark_right, inner_top, mark_right + 1, inner_bottom },
                  gui::with_alpha( MENU_SECTION_SEPARATOR, opacity ), true );

    const i32 gap{ g_gui.scaled( WATERMARK_GAP ) };
    const i32 text_y{ g_gui.text_center_y( inner_top, row_height ) };

    i32 cursor{ mark_right + 1 + g_gui.scaled( WATERMARK_PADDING ) };

    for ( i32 index{ }; index < count; ++index )
    {
        g_render.text( MENU_FONT( ), point_t{ cursor, text_y },
                       gui::with_alpha( cells[ index ].m_color, ink ), cells[ index ].m_text, false,
                       render::text_align::left );

        cursor += cells[ index ].m_width + gap;
    }

    return mark_right + 1;
}

void launch_watermark( )
{
    s_launched = true;
    s_reveal   = 0;
}

void draw_watermark_boot( i32 screen_width, i32 screen_height, const watermark_boot_t &boot )
{
    cell_t cells[ WATERMARK_MAX_CELLS ]{ };
    i32    count{ };

    push_cell( cells, count, boot.m_caption, MENU_TEXT_MUTED );
    push_cell( cells, count, boot.m_counter, MENU_TEXT_DIM );

    cells[ 0 ].m_width = boot.m_caption_width;

    const rect_t strip{ centered_rect( strip_width( cells, count ), g_gui.scaled( WATERMARK_HEIGHT ),
                                       screen_width, screen_height ) };

    const i32 line_left{ draw_strip( strip, MENU_OUTER_BORDER, cells, count, boot.m_opacity,
                                     boot.m_opacity ) };
    const i32 line_right{ strip.right - 1 };
    const i32 filled{ static_cast< i32 >( static_cast< float >( line_right - line_left ) * boot.m_fill +
                                          0.5f ) };

    g_render.box( rect_t{ line_left, strip.bottom - 2, line_left + filled, strip.bottom - 1 },
                  gui::with_alpha( gui::with_alpha( g_theme.m_accent, boot.m_line ), boot.m_opacity ),
                  true );
}

void draw_watermark( i32 screen_width, i32 screen_height, const watermark_values_t &values )
{
    const u32 delta{ gui::animation_delta( s_tick ) };

    step_counter( delta );

    const util::system_time_t now{ util::local_time( ) };

    char frames[ 16 ]{ };
    str::format_to( frames, sizeof( frames ), ATLAS_XORSTR_STATIC( "{} fps" ),
                    s_counter.m_value );

    char latency[ 32 ]{ };

    if ( values.m_connected && values.m_fake_latency > 0 )
    {
        str::format_to( latency, sizeof( latency ), ATLAS_XORSTR_STATIC( "{}+{} ms ({})" ),
                        values.m_latency, values.m_fake_latency, values.m_scoreboard_latency );
    } else if ( values.m_connected )
    {
        str::format_to( latency, sizeof( latency ), ATLAS_XORSTR_STATIC( "{} ms" ),
                        values.m_latency );
    }

    char clock[ 8 ]{ };
    write_clock( clock, now.wHour, now.wMinute );

    const color_t ping_color{ values.m_latency > WATERMARK_PING_WARNING_MS ? STATUS_WARNING : CONTROL_VALUE };

    cell_t cells[ WATERMARK_MAX_CELLS ]{ };
    i32    count{ };

    push_cell( cells, count, values.m_user, MENU_TEXT_MUTED );
    push_cell( cells, count, frames, CONTROL_VALUE );
    push_cell( cells, count, latency, ping_color );
    push_cell( cells, count, clock, MENU_TEXT_DIM );

    const i32 width{ strip_width( cells, count ) };
    const i32 height{ g_gui.scaled( WATERMARK_HEIGHT ) };
    const i32 margin{ g_gui.scaled( WATERMARK_MARGIN ) };

    if ( width + margin * 2 > screen_width || height + margin * 2 > screen_height )
    {
        return;
    }

    if ( !s_placed )
    {
        const rect_t start{ s_launched ? centered_rect( width, height, screen_width, screen_height )
                                       : resolved_rect( width, height, margin, screen_width, screen_height, values.m_watermark_position ) };

        s_render_x = static_cast< float >( start.left );
        s_render_y = static_cast< float >( start.top );
        s_placed   = true;
    }

    rect_t strip{ render_rect( width, height ) };

    drive_drag( strip, width, height, margin, screen_width, screen_height, values.m_watermark_position );

    if ( s_dragging )
    {
        const rect_t held{ s_free.x, s_free.y, s_free.x + width, s_free.y + height };

        u8        strength{ };
        const i32 armed{ armed_anchor( held, margin, screen_width, screen_height,
                                       strength ) };

        if ( armed != ANCHOR_NONE )
        {
            const rect_t landing{ anchor_rect( armed, width, height, margin, screen_width,
                                               screen_height ) };

            draw_anchor_glow( anchor_origin( armed, screen_width, screen_height ),
                              g_theme.m_accent, strength, screen_width, screen_height );

            g_render.box( landing, gui::with_alpha( g_theme.m_accent, strength ) );
        }
    }

    step_placement( delta, resolved_rect( width, height, margin, screen_width, screen_height, values.m_watermark_position ) );

    strip = render_rect( width, height );

    const bool hovered{ s_dragging || ( g_gui.can_interact( ) && !g_gui.dragging( ) && g_gui.hit_test( strip ) ) };

    gui::step_mix( s_reveal, true, delta, WATERMARK_REVEAL );

    draw_strip( strip, hovered ? g_theme.m_accent : MENU_OUTER_BORDER, cells, count, 255,
                gui::eased_out( s_reveal ) );
}
