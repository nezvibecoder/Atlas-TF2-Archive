#include "input/input.hpp"
#include "libs/lazy_importer.hpp"
#include "math/math.hpp"
#include "render/icons.hpp"
#include "render/render.hpp"
#include "util/format.hpp"
#include "util/string.hpp"
#include "util/windefs.hpp"

#include "gui.hpp"
#include "theme.hpp"

namespace gui
{
    u64 clock( )
    {
        static LARGE_INTEGER frequency{ };

        if ( !frequency.QuadPart && !I( QueryPerformanceFrequency )( &frequency ) )
        {
            frequency.QuadPart = 0;
        }

        LARGE_INTEGER counter{ };

        if ( frequency.QuadPart > 0 && I( QueryPerformanceCounter )( &counter ) )
        {
            return static_cast< u64 >( counter.QuadPart * 1000 / frequency.QuadPart );
        }

        return static_cast< u64 >( I( GetTickCount64 )( ) );
    }

    u32 animation_delta( u64 &last_tick )
    {
        const u64 now{ clock( ) };
        u32       delta{ 16 };

        if ( last_tick && now >= last_tick )
        {
            const u64 elapsed{ now - last_tick };
            delta = static_cast< u32 >( elapsed > ANIMATION_MAX_DELTA ? ANIMATION_MAX_DELTA
                                                                      : elapsed );
        }

        last_tick = now;
        return delta;
    }

    u8 animation_step( u32 delta, u32 duration )
    {
        if ( !delta || !duration )
        {
            return 0;
        }

        const u32 step{ ( 255u * delta + duration - 1u ) / duration };
        return static_cast< u8 >( step > 255u ? 255u : step );
    }

    void step_mix( u8 &mix, bool on, u32 delta, u32 duration )
    {
        mix = math::approach( mix, static_cast< u8 >( on ? 255 : 0 ),
                              animation_step( delta, duration ) );
    }

    u8 mix_channel( u8 from, u8 to, u8 amount )
    {
        const u32 inverse{ 255u - amount };
        const u32 result{ static_cast< u32 >( from ) * inverse +
                          static_cast< u32 >( to ) * amount + 127u };
        return static_cast< u8 >( result / 255u );
    }

    color_t mix_color( color_t from, color_t to, u8 amount )
    {
        return color_t{ mix_channel( from.r, to.r, amount ), mix_channel( from.g, to.g, amount ),
                        mix_channel( from.b, to.b, amount ), mix_channel( from.a, to.a, amount ) };
    }

    u8 eased_mix( u8 amount )
    {
        const float progress{ math::smooth_step( static_cast< float >( amount ) / 255.0f ) };
        return static_cast< u8 >( progress * 255.0f + 0.5f );
    }

    u8 eased_out( u8 amount )
    {
        const float remaining{ 1.0f - static_cast< float >( amount ) / 255.0f };
        return static_cast< u8 >( ( 1.0f - remaining * remaining * remaining ) * 255.0f + 0.5f );
    }

    u8 pulse( u64 now, u32 period )
    {
        if ( !period )
        {
            return 0;
        }

        const float phase{ static_cast< float >( now % period ) / static_cast< float >( period ) };
        const float triangle{ phase < 0.5f ? phase * 2.0f : ( 1.0f - phase ) * 2.0f };
        return static_cast< u8 >( math::smooth_step( triangle ) * 255.0f + 0.5f );
    }

    float approach_float( float value, float target, u32 delta, float duration )
    {
        const float step{ math::smaller( 1.0f, static_cast< float >( delta ) / duration ) };
        return value + ( target - value ) * step;
    }

    color_t with_alpha( color_t value, u8 opacity )
    {
        value.a = static_cast< u8 >( ( static_cast< u32 >( value.a ) * opacity + 127u ) / 255u );
        return value;
    }
} // namespace gui

inline constexpr i32 FONT_TABLE_FIRST{ 32 };
inline constexpr i32 FONT_TABLE_LAST{ 126 };

inline constexpr i32 LABEL_FONT_ADVANCE[]{
    3, 4, 4, 8, 6, 11, 7, 2, 4, 4, 6, 8, 4, 4, 4, 4,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 4, 8, 8, 8, 5,
    10, 7, 6, 7, 7, 6, 6, 7, 7, 4, 5, 6, 5, 8, 7, 8,
    6, 8, 7, 6, 6, 7, 6, 10, 6, 6, 6, 4, 4, 4, 8, 6,
    6, 6, 6, 5, 6, 6, 4, 6, 6, 2, 3, 5, 2, 8, 6, 6,
    6, 6, 4, 5, 4, 6, 6, 8, 6, 6, 5, 5, 4, 5, 8
};

inline constexpr i32 BOLD_FONT_ADVANCE[]{
    3, 3, 6, 9, 7, 13, 9, 3, 5, 5, 7, 9, 3, 5, 3, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 3, 3, 9, 9, 9, 6,
    10, 8, 7, 7, 8, 6, 6, 8, 8, 5, 6, 7, 6, 10, 7, 8,
    7, 8, 8, 7, 7, 8, 7, 11, 7, 7, 7, 5, 6, 5, 9, 7,
    6, 7, 7, 6, 7, 7, 4, 7, 7, 3, 4, 7, 3, 11, 7, 7,
    7, 7, 5, 6, 5, 7, 7, 9, 7, 7, 6, 7, 7, 7, 9
};

inline constexpr i32 TINY_FONT_ADVANCE[]{
    3, 2, 4, 7, 6, 10, 7, 2, 4, 4, 6, 7, 3, 4, 3, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 7, 7, 7, 5,
    9, 7, 6, 7, 7, 6, 6, 7, 7, 4, 4, 6, 5, 8, 7, 8,
    6, 8, 6, 6, 6, 7, 6, 10, 6, 6, 6, 4, 4, 4, 7, 5,
    5, 5, 6, 5, 6, 5, 3, 6, 6, 2, 3, 6, 2, 8, 6, 6,
    6, 6, 4, 4, 3, 6, 6, 8, 5, 6, 4, 5, 4, 5, 7
};

static_assert( sizeof( LABEL_FONT_ADVANCE ) / sizeof( i32 ) ==
               FONT_TABLE_LAST - FONT_TABLE_FIRST + 1 );
static_assert( sizeof( TINY_FONT_ADVANCE ) / sizeof( i32 ) ==
               FONT_TABLE_LAST - FONT_TABLE_FIRST + 1 );
static_assert( sizeof( BOLD_FONT_ADVANCE ) / sizeof( i32 ) ==
               FONT_TABLE_LAST - FONT_TABLE_FIRST + 1 );

static i32 measure_text( string_view text, const i32 *advances )
{
    i32 width{ };

    for ( usize index{ }; index < text.size( ); ++index )
    {
        const i32 code{ static_cast< i32 >( static_cast< u8 >( text[ index ] ) ) };

        width += code >= FONT_TABLE_FIRST && code <= FONT_TABLE_LAST
                     ? advances[ code - FONT_TABLE_FIRST ]
                     : advances[ 0 ];
    }

    return width;
}

namespace gui
{
    i32 estimate_tiny_text_width( string_view text )
    {
        return measure_text( text, TINY_FONT_ADVANCE );
    }

    i32 estimate_text_width( string_view text )
    {
        return measure_text( text, LABEL_FONT_ADVANCE );
    }

    i32 estimate_bold_text_width( string_view text )
    {
        return measure_text( text, BOLD_FONT_ADVANCE );
    }

    i32 advance_of( char value )
    {
        const i32 code{ static_cast< i32 >( static_cast< u8 >( value ) ) };
        return code >= FONT_TABLE_FIRST && code <= FONT_TABLE_LAST ? LABEL_FONT_ADVANCE[ code - FONT_TABLE_FIRST ] : LABEL_FONT_ADVANCE[ 0 ];
    }
} // namespace gui

static i32 measure_range( const char *text, usize first, usize last )
{
    i32 width{ };

    for ( usize index{ first }; index < last; ++index )
    {
        width += gui::advance_of( text[ index ] );
    }

    return width;
}

static const char *key_name( u32 key )
{
    static char printable[ 2 ]{ };

    switch ( key )
    {
        case 0: return ATLAS_XORSTR_STATIC( "NONE" );
        case VK_LBUTTON: return ATLAS_XORSTR_STATIC( "MOUSE1" );
        case VK_RBUTTON: return ATLAS_XORSTR_STATIC( "MOUSE2" );
        case VK_MBUTTON: return ATLAS_XORSTR_STATIC( "MOUSE3" );
        case VK_XBUTTON1: return ATLAS_XORSTR_STATIC( "MOUSE4" );
        case VK_XBUTTON2: return ATLAS_XORSTR_STATIC( "MOUSE5" );
        case VK_BACK: return ATLAS_XORSTR_STATIC( "BACK" );
        case VK_TAB: return ATLAS_XORSTR_STATIC( "TAB" );
        case VK_RETURN: return ATLAS_XORSTR_STATIC( "ENTER" );
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT: return ATLAS_XORSTR_STATIC( "SHIFT" );
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL: return ATLAS_XORSTR_STATIC( "CTRL" );
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU: return ATLAS_XORSTR_STATIC( "ALT" );
        case VK_CAPITAL: return ATLAS_XORSTR_STATIC( "CAPS" );
        case VK_SPACE: return ATLAS_XORSTR_STATIC( "SPACE" );
        case VK_PRIOR: return ATLAS_XORSTR_STATIC( "PGUP" );
        case VK_NEXT: return ATLAS_XORSTR_STATIC( "PGDN" );
        case VK_END: return ATLAS_XORSTR_STATIC( "END" );
        case VK_HOME: return ATLAS_XORSTR_STATIC( "HOME" );
        case VK_LEFT: return ATLAS_XORSTR_STATIC( "LEFT" );
        case VK_UP: return ATLAS_XORSTR_STATIC( "UP" );
        case VK_RIGHT: return ATLAS_XORSTR_STATIC( "RIGHT" );
        case VK_DOWN: return ATLAS_XORSTR_STATIC( "DOWN" );
        case VK_DELETE: return ATLAS_XORSTR_STATIC( "DEL" );
        case VK_F1: return ATLAS_XORSTR_STATIC( "F1" );
        case VK_F2: return ATLAS_XORSTR_STATIC( "F2" );
        case VK_F3: return ATLAS_XORSTR_STATIC( "F3" );
        case VK_F4: return ATLAS_XORSTR_STATIC( "F4" );
        case VK_F5: return ATLAS_XORSTR_STATIC( "F5" );
        case VK_F6: return ATLAS_XORSTR_STATIC( "F6" );
        case VK_F7: return ATLAS_XORSTR_STATIC( "F7" );
        case VK_F8: return ATLAS_XORSTR_STATIC( "F8" );
        case VK_F9: return ATLAS_XORSTR_STATIC( "F9" );
        case VK_F10: return ATLAS_XORSTR_STATIC( "F10" );
        case VK_F11: return ATLAS_XORSTR_STATIC( "F11" );
        case VK_F12: return ATLAS_XORSTR_STATIC( "F12" );
        case VK_F13: return ATLAS_XORSTR_STATIC( "F13" );
        case VK_F14: return ATLAS_XORSTR_STATIC( "F14" );
        case VK_F15: return ATLAS_XORSTR_STATIC( "F15" );
        case VK_F16: return ATLAS_XORSTR_STATIC( "F16" );
        case VK_F17: return ATLAS_XORSTR_STATIC( "F17" );
        case VK_F18: return ATLAS_XORSTR_STATIC( "F18" );
        case VK_F19: return ATLAS_XORSTR_STATIC( "F19" );
        case VK_F20: return ATLAS_XORSTR_STATIC( "F20" );
        case VK_F21: return ATLAS_XORSTR_STATIC( "F21" );
        case VK_F22: return ATLAS_XORSTR_STATIC( "F22" );
        case VK_F23: return ATLAS_XORSTR_STATIC( "F23" );
        case VK_F24: return ATLAS_XORSTR_STATIC( "F24" );
        default: break;
    }

    if ( ( key >= '0' && key <= '9' ) || ( key >= 'A' && key <= 'Z' ) )
    {
        printable[ 0 ] = static_cast< char >( key );
        printable[ 1 ] = 0;
        return printable;
    }

    return ATLAS_XORSTR_STATIC( "UNK" );
}

namespace gui
{
    const char *key_name( u32 key )
    {
        return ::key_name( key );
    }

    i32 measure_range( const char *text, usize first, usize last )
    {
        return ::measure_range( text, first, last );
    }

    void draw_icon_glyph( const char *glyph, point_t center, color_t value )
    {
        draw_icon_at( glyph,
                      point_t{ center.x - icon_ink_width( glyph ) / 2,
                               center.y - MENU_ICON_SIZE / 2 },
                      g_gui.color( value ) );
    }

    void fill_horizontal_gradient( rect_t bounds, color_t left, color_t right )
    {
        const i32 width{ bounds.right - bounds.left };

        if ( width <= 0 || bounds.bottom <= bounds.top )
        {
            return;
        }

        const i32 divisor{ width > 1 ? width - 1 : 1 };

        for ( i32 column{ }; column < width; ++column )
        {
            const u8      amount{ static_cast< u8 >( column * 255 / divisor ) };
            const color_t strip{ mix_color( left, right, amount ) };
            g_render.box( rect_t{ bounds.left + column, bounds.top, bounds.left + column + 1,
                                  bounds.bottom },
                          g_gui.color( strip ), true );
        }
    }

    inline constexpr i32   RING_STEPS{ 48 };
    inline constexpr float RING_STEP_ANGLE{ 0.13089969f }; // one turn / RING_STEPS
    inline constexpr float RING_STEP_COS{ 0.99144486f };
    inline constexpr float RING_STEP_SIN{ 0.13052619f };

    static float disc_coverage( float dx, float dy, float radius )
    {
        if ( radius <= 0.0f )
        {
            return 0.0f;
        }

        return math::clamp( radius + 0.5f - math::sqrt( dx * dx + dy * dy ), 0.0f, 1.0f );
    }

    inline constexpr float STROKE_FEATHER{ 0.55f };

    static float stroke_coverage( float dx, float dy, float outer, float inner )
    {
        const float middle{ ( outer + inner ) * 0.5f };
        const float half{ ( outer - inner ) * 0.5f };
        const float edge{ math::abs( math::sqrt( dx * dx + dy * dy ) - middle ) };

        return math::clamp( ( half + STROKE_FEATHER - edge ) / ( 2.0f * STROKE_FEATHER ), 0.0f,
                            1.0f );
    }

    static float band_coverage( float dx, float dy, float outer, float inner )
    {
        return inner > 0.0f ? stroke_coverage( dx, dy, outer, inner )
                            : disc_coverage( dx, dy, outer );
    }

    static void paint_pixel( i32 column, i32 row, color_t value, float coverage )
    {
        if ( coverage <= 0.0f || !value.a )
        {
            return;
        }

        const u8 amount{ static_cast< u8 >( coverage >= 1.0f ? 255.0f
                                                             : coverage * 255.0f + 0.5f ) };

        g_render.box( rect_t{ column, row, column + 1, row + 1 }, with_alpha( value, amount ),
                      true );
    }

    static void paint_arc_row( i32 row, i32 first, i32 last, float dx_bias, float dy, float outer,
                               float inner, color_t value )
    {
        i32 run{ -1 };

        for ( i32 column{ first }; column <= last; ++column )
        {
            const float coverage{ column < last
                                      ? band_coverage( static_cast< float >( column ) + dx_bias,
                                                       dy, outer, inner )
                                      : 0.0f };

            if ( coverage >= 1.0f )
            {
                if ( run < 0 )
                {
                    run = column;
                }

                continue;
            }

            if ( run >= 0 )
            {
                g_render.box( rect_t{ run, row, column, row + 1 }, value, true );
                run = -1;
            }

            paint_pixel( column, row, value, coverage );
        }
    }

    static void paint_corner( rect_t block, point_t center, float outer, float inner,
                              color_t value )
    {
        const float dx_bias{ 0.5f - static_cast< float >( center.x ) };

        for ( i32 row{ block.top }; row < block.bottom; ++row )
        {
            paint_arc_row( row, block.left, block.right, dx_bias,
                           static_cast< float >( row ) + 0.5f - static_cast< float >( center.y ),
                           outer, inner, value );
        }
    }

    void fill_rounded_box( rect_t bounds, i32 radius, color_t value )
    {
        const i32 width{ bounds.right - bounds.left };
        const i32 height{ bounds.bottom - bounds.top };

        if ( width <= 0 || height <= 0 || !value.a )
        {
            return;
        }

        radius = math::clamp( radius, 0, math::smaller( width, height ) / 2 );

        if ( !radius )
        {
            g_render.box( bounds, value, true );
            return;
        }

        const i32   left{ bounds.left + radius };
        const i32   right{ bounds.right - radius };
        const i32   top{ bounds.top + radius };
        const i32   bottom{ bounds.bottom - radius };
        const float span{ static_cast< float >( radius ) };

        g_render.box( rect_t{ bounds.left, top, bounds.right, bottom }, value, true );
        g_render.box( rect_t{ left, bounds.top, right, top }, value, true );
        g_render.box( rect_t{ left, bottom, right, bounds.bottom }, value, true );

        paint_corner( rect_t{ bounds.left, bounds.top, left, top }, point_t{ left, top }, span,
                      0.0f, value );
        paint_corner( rect_t{ right, bounds.top, bounds.right, top }, point_t{ right, top }, span,
                      0.0f, value );
        paint_corner( rect_t{ bounds.left, bottom, left, bounds.bottom }, point_t{ left, bottom },
                      span, 0.0f, value );
        paint_corner( rect_t{ right, bottom, bounds.right, bounds.bottom },
                      point_t{ right, bottom }, span, 0.0f, value );
    }

    void stroke_rounded_box( rect_t bounds, i32 radius, color_t value, i32 thickness )
    {
        const i32 width{ bounds.right - bounds.left };
        const i32 height{ bounds.bottom - bounds.top };

        if ( width <= 0 || height <= 0 || thickness <= 0 || !value.a )
        {
            return;
        }

        const i32 limit{ math::smaller( width, height ) / 2 };

        radius    = math::clamp( radius, 0, limit );
        thickness = math::clamp( thickness, 1, limit > 0 ? limit : 1 );

        const i32   left{ bounds.left + radius };
        const i32   right{ bounds.right - radius };
        const i32   top{ bounds.top + radius };
        const i32   bottom{ bounds.bottom - radius };
        const float span{ static_cast< float >( radius ) };
        const float hole{ static_cast< float >( radius - thickness ) };

        g_render.box( rect_t{ left, bounds.top, right, bounds.top + thickness }, value, true );
        g_render.box( rect_t{ left, bounds.bottom - thickness, right, bounds.bottom }, value,
                      true );
        g_render.box( rect_t{ bounds.left, top, bounds.left + thickness, bottom }, value, true );
        g_render.box( rect_t{ bounds.right - thickness, top, bounds.right, bottom }, value, true );

        if ( !radius )
        {
            return;
        }

        paint_corner( rect_t{ bounds.left, bounds.top, left, top }, point_t{ left, top }, span,
                      hole, value );
        paint_corner( rect_t{ right, bounds.top, bounds.right, top }, point_t{ right, top }, span,
                      hole, value );
        paint_corner( rect_t{ bounds.left, bottom, left, bounds.bottom }, point_t{ left, bottom },
                      span, hole, value );
        paint_corner( rect_t{ right, bottom, bounds.right, bounds.bottom },
                      point_t{ right, bottom }, span, hole, value );
    }

    inline constexpr i32   WAVE_RUN{ 3 }; // pixels a straight edge shades at a time
    inline constexpr float QUARTER_TURN{ 1.5707963f };

    static u8 wave_amount( float position, float phase )
    {
        float distance{ position - phase };

        distance -= static_cast< float >( static_cast< i32 >( distance ) );

        if ( distance < 0.0f )
        {
            distance += 1.0f;
        }

        if ( distance > 0.5f )
        {
            distance = 1.0f - distance;
        }

        return static_cast< u8 >( math::smooth_step( 1.0f - distance * 2.0f ) * 255.0f + 0.5f );
    }

    struct wave_walk_t
    {
        color_t m_low{ };
        color_t m_high{ };
        float   m_total{ };
        float   m_phase{ };

        color_t shade( float travelled ) const
        {
            return mix_color( this->m_low, this->m_high,
                              wave_amount( travelled / this->m_total, this->m_phase ) );
        }
    };

    static void paint_row_wave( rect_t edge, const wave_walk_t &wave, float base, bool reverse )
    {
        const i32 length{ edge.right - edge.left };

        for ( i32 offset{ }; offset < length; offset += WAVE_RUN )
        {
            const i32   step{ math::smaller( WAVE_RUN, length - offset ) };
            const float middle{ static_cast< float >( offset ) +
                                static_cast< float >( step ) * 0.5f };
            const float travelled{ base + ( reverse ? static_cast< float >( length ) - middle
                                                    : middle ) };

            g_render.box( rect_t{ edge.left + offset, edge.top, edge.left + offset + step,
                                  edge.bottom },
                          wave.shade( travelled ), true );
        }
    }

    static void paint_column_wave( rect_t edge, const wave_walk_t &wave, float base, bool reverse )
    {
        const i32 length{ edge.bottom - edge.top };

        for ( i32 offset{ }; offset < length; offset += WAVE_RUN )
        {
            const i32   step{ math::smaller( WAVE_RUN, length - offset ) };
            const float middle{ static_cast< float >( offset ) +
                                static_cast< float >( step ) * 0.5f };
            const float travelled{ base + ( reverse ? static_cast< float >( length ) - middle
                                                    : middle ) };

            g_render.box( rect_t{ edge.left, edge.top + offset, edge.right,
                                  edge.top + offset + step },
                          wave.shade( travelled ), true );
        }
    }

    static void paint_corner_wave( rect_t block, point_t center, float outer, float inner,
                                   const wave_walk_t &wave, float base, float arc, bool from_x )
    {
        for ( i32 row{ block.top }; row < block.bottom; ++row )
        {
            const float dy{ static_cast< float >( row ) + 0.5f -
                            static_cast< float >( center.y ) };

            for ( i32 column{ block.left }; column < block.right; ++column )
            {
                const float dx{ static_cast< float >( column ) + 0.5f -
                                static_cast< float >( center.x ) };
                const float coverage{ band_coverage( dx, dy, outer, inner ) };

                if ( coverage <= 0.0f )
                {
                    continue;
                }

                const float span{ math::abs( dx ) + math::abs( dy ) };
                const float along{ span > 0.0f
                                       ? ( from_x ? math::abs( dx ) : math::abs( dy ) ) / span
                                       : 0.0f };

                paint_pixel( column, row, wave.shade( base + along * arc ), coverage );
            }
        }
    }

    void stroke_rounded_wave( rect_t bounds, i32 radius, color_t low, color_t high, float phase,
                              i32 thickness )
    {
        const i32 width{ bounds.right - bounds.left };
        const i32 height{ bounds.bottom - bounds.top };

        if ( width <= 0 || height <= 0 || thickness <= 0 )
        {
            return;
        }

        const i32 limit{ math::smaller( width, height ) / 2 };

        radius    = math::clamp( radius, 0, limit );
        thickness = math::clamp( thickness, 1, limit > 0 ? limit : 1 );

        const i32 left{ bounds.left + radius };
        const i32 right{ bounds.right - radius };
        const i32 top{ bounds.top + radius };
        const i32 bottom{ bounds.bottom - radius };

        const float run_x{ static_cast< float >( right - left ) };
        const float run_y{ static_cast< float >( bottom - top ) };
        const float arc{ QUARTER_TURN * static_cast< float >( radius ) };

        wave_walk_t wave{ };

        wave.m_low   = low;
        wave.m_high  = high;
        wave.m_total = 2.0f * run_x + 2.0f * run_y + 4.0f * arc;
        wave.m_phase = phase;

        if ( wave.m_total <= 0.0f )
        {
            return;
        }

        paint_row_wave( rect_t{ left, bounds.top, right, bounds.top + thickness }, wave, 0.0f,
                        false );
        paint_column_wave( rect_t{ bounds.right - thickness, top, bounds.right, bottom }, wave,
                           run_x + arc, false );
        paint_row_wave( rect_t{ left, bounds.bottom - thickness, right, bounds.bottom }, wave,
                        run_x + 2.0f * arc + run_y, true );
        paint_column_wave( rect_t{ bounds.left, top, bounds.left + thickness, bottom }, wave,
                           2.0f * run_x + 3.0f * arc + run_y, true );

        if ( !radius )
        {
            return;
        }

        const float rim{ static_cast< float >( radius ) };
        const float hole{ static_cast< float >( radius - thickness ) };

        paint_corner_wave( rect_t{ right, bounds.top, bounds.right, top }, point_t{ right, top },
                           rim, hole, wave, run_x, arc, true );
        paint_corner_wave( rect_t{ right, bottom, bounds.right, bounds.bottom },
                           point_t{ right, bottom }, rim, hole, wave, run_x + arc + run_y, arc,
                           false );
        paint_corner_wave( rect_t{ bounds.left, bottom, left, bounds.bottom },
                           point_t{ left, bottom }, rim, hole, wave,
                           2.0f * run_x + 2.0f * arc + run_y, arc, true );
        paint_corner_wave( rect_t{ bounds.left, bounds.top, left, top }, point_t{ left, top },
                           rim, hole, wave, 2.0f * run_x + 3.0f * arc + 2.0f * run_y, arc,
                           false );
    }

    static bool same_color( color_t left, color_t right )
    {
        return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
    }

    static u8 sector_amount( float dx, float dy, float lead_x, float lead_y, bool wide )
    {
        const float opening{ math::clamp( 0.5f + dx, 0.0f, 1.0f ) };
        const float closing{ math::clamp( 0.5f - ( lead_x * dy - lead_y * dx ), 0.0f, 1.0f ) };
        const float amount{ wide ? math::smaller( 1.0f, opening + closing )
                                 : math::smaller( opening, closing ) };

        return static_cast< u8 >( amount * 255.0f + 0.5f );
    }

    void fill_ring( point_t center, i32 outer, i32 thickness, float progress, color_t track,
                    color_t fill, bool between_columns )
    {
        if ( outer <= 0 || thickness <= 0 )
        {
            return;
        }

        thickness = math::clamp( thickness, 1, outer );
        progress  = math::clamp( progress, 0.0f, 1.0f );

        const float rim{ static_cast< float >( outer ) };
        const float hole{ static_cast< float >( outer - thickness ) };
        const bool  empty{ progress <= 0.0f };
        const bool  full{ progress >= 1.0f };
        const bool  wide{ progress > 0.5f };

        float lead_x{ };
        float lead_y{ -1.0f };

        const float turns{ progress * static_cast< float >( RING_STEPS ) };
        const i32   whole{ static_cast< i32 >( turns ) };

        for ( i32 step{ }; step < whole; ++step )
        {
            const float turned{ lead_x * RING_STEP_COS - lead_y * RING_STEP_SIN };

            lead_y = lead_x * RING_STEP_SIN + lead_y * RING_STEP_COS;
            lead_x = turned;
        }

        const float rest{ ( turns - static_cast< float >( whole ) ) * RING_STEP_ANGLE };
        const float rest_cos{ 1.0f - rest * rest * 0.5f };
        const float turned{ lead_x * rest_cos - lead_y * rest };

        lead_y = lead_x * rest + lead_y * rest_cos;
        lead_x = turned;

        const float bias{ between_columns ? 0.5f : 0.0f };

        const i32 first{ center.x - outer - 1 };
        const i32 last{ center.x + outer + 1 }; // sentinel column, flushes the last run

        for ( i32 row{ center.y - outer - 1 }; row <= center.y + outer; ++row )
        {
            const float dy{ static_cast< float >( row - center.y ) + 0.5f };

            i32     run{ -1 };
            color_t run_value{ };

            for ( i32 column{ first }; column <= last; ++column )
            {
                const bool  inside{ column < last };
                const float dx{ static_cast< float >( column - center.x ) + bias };
                const float coverage{ inside ? band_coverage( dx, dy, rim, hole ) : 0.0f };

                const u8 amount{ empty  ? static_cast< u8 >( 0 )
                                 : full ? static_cast< u8 >( 255 )
                                        : sector_amount( dx, dy, lead_x, lead_y, wide ) };

                const color_t value{ mix_color( track, fill, amount ) };

                if ( coverage >= 1.0f && ( run < 0 || same_color( run_value, value ) ) )
                {
                    if ( run < 0 )
                    {
                        run       = column;
                        run_value = value;
                    }

                    continue;
                }

                if ( run >= 0 )
                {
                    g_render.box( rect_t{ run, row, column, row + 1 }, run_value, true );
                    run = -1;
                }

                paint_pixel( column, row, value, coverage );
            }
        }
    }
} // namespace gui

static constexpr i32 icon_slot_for_char( char value )
{
    switch ( value )
    {
        case '!': return render::icons::ID_USER;
        case '\"': return render::icons::ID_CROSSHAIR;
        case '#': return render::icons::ID_EYE;
        case '$': return render::icons::ID_WRENCH;
        case '%': return render::icons::ID_SETTINGS;
        case '&': return render::icons::ID_CODE;
        case '*': return render::icons::ID_SAVE;
        case '+': return render::icons::ID_CHEVRON_DOWN;
        case ',': return render::icons::ID_SEARCH;
        case '-': return render::icons::ID_CHEVRON_UP;
        case '.': return render::icons::ID_CROSS;
        case '/': return render::icons::ID_SPARKLES;
        case '0': return render::icons::ID_ZAP;
        case '1': return render::icons::ID_DOLLAR;
        case '2': return render::icons::ID_BAN;
        case '3': return render::icons::ID_FLAME;
        case '4': return render::icons::ID_PLAYERS;
        case '5': return render::icons::ID_LOADER;
        case '6': return render::icons::ID_TRIANGLE_ALERT;
        case '7': return render::icons::ID_RULER;
        case '8': return render::icons::ID_RADIO;
        case '9': return render::icons::ID_MUSIC;
        default: return -1;
    }
}

namespace gui
{
    static i32 icon_slot_for( const char *glyph )
    {
        return glyph && glyph[ 0 ] ? icon_slot_for_char( glyph[ 0 ] ) : -1;
    }

    i32 icon_ink_width( const char *glyph, i32 size )
    {
        const i32 slot{ icon_slot_for( glyph ) };

        if ( slot < 0 )
        {
            return 0;
        }

        const u8 ink{ size >= MENU_ICON_SIZE_LARGE
                          ? render::icons::INK_LARGE[ slot ]
                          : render::icons::INK_SMALL[ slot ] };

        return ink ? static_cast< i32 >( ink ) : size;
    }

    void draw_icon_at( const char *glyph, point_t origin, color_t value, i32 size )
    {
        const i32 slot{ icon_slot_for( glyph ) };

        if ( slot < 0 )
        {
            return;
        }

        g_render.icon( static_cast< u32 >( slot ),
                       size >= MENU_ICON_SIZE_LARGE ? render::icons::KIND_LARGE
                                                    : render::icons::KIND_SMALL,
                       origin, value );
    }

    void draw_logo( point_t origin, color_t value )
    {
        g_render.icon( render::icons::ID_LOGO_A, render::icons::KIND_LOGO, origin, value );
    }

    static color_t shadow_tint( color_t tint, float reach )
    {
        if ( reach >= 1.0f )
        {
            return color_t{ 0, 0, 0, 0 };
        }

        tint.a = static_cast< u8 >( math::smooth_step( 1.0f - reach ) *
                                        static_cast< float >( tint.a ) +
                                    0.5f );
        return tint;
    }

    static void draw_shadow_corner( point_t origin, i32 toward_x, i32 toward_y, i32 spread_x,
                                    i32 spread_y, color_t tint )
    {
        for ( i32 down{ 1 }; down <= spread_y; ++down )
        {
            const float reach_y{ static_cast< float >( down ) /
                                 static_cast< float >( spread_y ) };
            const i32   row{ toward_y < 0 ? origin.y - down : origin.y + down - 1 };

            i32 run_first{ };
            u8  run_alpha{ };

            for ( i32 out{ 1 }; out <= spread_x + 1; ++out )
            {
                color_t step{ 0, 0, 0, 0 };

                if ( out <= spread_x )
                {
                    const float reach_x{ static_cast< float >( out ) /
                                         static_cast< float >( spread_x ) };

                    step = shadow_tint( tint,
                                        math::sqrt( reach_x * reach_x + reach_y * reach_y ) );
                }

                if ( step.a == run_alpha )
                {
                    continue;
                }

                if ( run_alpha )
                {
                    const i32    last{ out - 1 };
                    const rect_t span{ toward_x < 0 ? origin.x - last : origin.x + run_first - 1,
                                       row,
                                       toward_x < 0 ? origin.x - run_first + 1
                                                    : origin.x + last,
                                       row + 1 };

                    color_t value{ tint };
                    value.a = run_alpha;
                    g_render.box( span, value, true );
                }

                run_first = out;
                run_alpha = step.a;
            }
        }
    }

    void draw_drop_shadow( rect_t bounds, i32 spread, i32 drop, color_t tint )
    {
        const i32 above{ spread - drop };
        const i32 below{ spread + drop };

        if ( spread <= 0 || above <= 0 || !tint.a )
        {
            return;
        }

        for ( i32 step{ 1 }; step <= spread; ++step )
        {
            const color_t side{ shadow_tint( tint, static_cast< float >( step ) /
                                                       static_cast< float >( spread ) ) };

            if ( !side.a )
            {
                continue;
            }

            g_render.box( rect_t{ bounds.left - step, bounds.top, bounds.left - step + 1,
                                  bounds.bottom },
                          side, true );
            g_render.box( rect_t{ bounds.right + step - 1, bounds.top, bounds.right + step,
                                  bounds.bottom },
                          side, true );
        }

        for ( i32 step{ 1 }; step <= above; ++step )
        {
            const color_t top{ shadow_tint( tint, static_cast< float >( step ) /
                                                      static_cast< float >( above ) ) };

            if ( top.a )
            {
                g_render.box( rect_t{ bounds.left, bounds.top - step, bounds.right,
                                      bounds.top - step + 1 },
                              top, true );
            }
        }

        for ( i32 step{ 1 }; step <= below; ++step )
        {
            const color_t bottom{ shadow_tint( tint, static_cast< float >( step ) /
                                                         static_cast< float >( below ) ) };

            if ( bottom.a )
            {
                g_render.box( rect_t{ bounds.left, bounds.bottom + step - 1, bounds.right,
                                      bounds.bottom + step },
                              bottom, true );
            }
        }

        draw_shadow_corner( point_t{ bounds.left, bounds.top }, -1, -1, spread, above, tint );
        draw_shadow_corner( point_t{ bounds.right, bounds.top }, 1, -1, spread, above, tint );
        draw_shadow_corner( point_t{ bounds.left, bounds.bottom }, -1, 1, spread, below, tint );
        draw_shadow_corner( point_t{ bounds.right, bounds.bottom }, 1, 1, spread, below, tint );
    }

    void draw_falloff_band( rect_t bounds, bool from_top, color_t tint )
    {
        const i32 height{ bounds.bottom - bounds.top };

        if ( !tint.a || height <= 0 )
        {
            return;
        }

        for ( i32 row{ }; row < height; ++row )
        {
            const i32 distance{ from_top ? row : height - 1 - row };
            const i32 remaining{ height - distance };
            const u32 falloff{ static_cast< u32 >( remaining * remaining ) * 255u /
                               static_cast< u32 >( height * height ) };

            g_render.box( rect_t{ bounds.left, bounds.top + row, bounds.right,
                                  bounds.top + row + 1 },
                          with_alpha( tint, static_cast< u8 >( falloff ) ), true );
        }
    }
} // namespace gui
