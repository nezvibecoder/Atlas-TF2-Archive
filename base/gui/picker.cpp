#include "input/input.hpp"
#include "libs/lazy_importer.hpp"
#include "math/math.hpp"
#include "render/render.hpp"
#include "util/string.hpp"
#include "util/windefs.hpp"

#include "gui.hpp"
#include "theme.hpp"

namespace gui
{
    struct hsv_t
    {
        float m_hue{ };
        float m_saturation{ };
        float m_brightness{ };
    };

    static bool contains( rect_t bounds, point_t point )
    {
        return point.x >= bounds.left && point.x < bounds.right && point.y >= bounds.top &&
               point.y < bounds.bottom;
    }

    static bool over( rect_t bounds )
    {
        return g_gui.can_interact( ) &&
               g_input.in_area( bounds.left, bounds.top, bounds.right - bounds.left - 1,
                                bounds.bottom - bounds.top - 1 );
    }

    static float fraction( i32 position, i32 first, i32 last )
    {
        const i32 span{ last - first };

        if ( span <= 0 )
        {
            return 0.0f;
        }

        return math::clamp( static_cast< float >( position - first ) /
                                static_cast< float >( span ),
                            0.0f, 1.0f );
    }

    static u8 channel_of( float value )
    {
        return static_cast< u8 >( math::clamp( value, 0.0f, 1.0f ) * 255.0f + 0.5f );
    }

    static hsv_t to_hsv( color_t value )
    {
        const float red{ static_cast< float >( value.r ) / 255.0f };
        const float green{ static_cast< float >( value.g ) / 255.0f };
        const float blue{ static_cast< float >( value.b ) / 255.0f };

        float high{ red > green ? red : green };
        high = high > blue ? high : blue;

        float low{ red < green ? red : green };
        low = low < blue ? low : blue;

        hsv_t result{ };

        result.m_brightness = high;

        const float span{ high - low };

        if ( high <= 0.0f || span <= 0.0f )
        {
            return result;
        }

        result.m_saturation = span / high;

        if ( high == red )
        {
            result.m_hue = ( green - blue ) / span;
        } else if ( high == green )
        {
            result.m_hue = 2.0f + ( blue - red ) / span;
        } else
        {
            result.m_hue = 4.0f + ( red - green ) / span;
        }

        result.m_hue /= 6.0f;

        if ( result.m_hue < 0.0f )
        {
            result.m_hue += 1.0f;
        }

        return result;
    }

    static color_t from_hsv( float hue, float saturation, float brightness, u8 alpha )
    {
        hue -= static_cast< float >( static_cast< i32 >( hue ) );

        if ( hue < 0.0f )
        {
            hue += 1.0f;
        }

        saturation = math::clamp( saturation, 0.0f, 1.0f );
        brightness = math::clamp( brightness, 0.0f, 1.0f );

        const float walked{ hue * 6.0f };
        const i32   sector{ static_cast< i32 >( walked ) };
        const float offset{ walked - static_cast< float >( sector ) };

        const float peak{ brightness };
        const float base{ brightness * ( 1.0f - saturation ) };
        const float falling{ brightness * ( 1.0f - saturation * offset ) };
        const float rising{ brightness * ( 1.0f - saturation * ( 1.0f - offset ) ) };

        float red{ peak };
        float green{ base };
        float blue{ base };

        switch ( sector % 6 )
        {
            case 0:
                green = rising;
                break;

            case 1:
                red   = falling;
                green = peak;
                break;

            case 2:
                red   = base;
                green = peak;
                blue  = rising;
                break;

            case 3:
                red   = base;
                green = falling;
                blue  = peak;
                break;

            case 4:
                red   = rising;
                green = base;
                blue  = peak;
                break;

            default:
                green = base;
                blue  = falling;
                break;
        }

        return color_t{ channel_of( red ), channel_of( green ), channel_of( blue ), alpha };
    }

    static bool hex_value( char glyph, u8 &out )
    {
        if ( glyph >= '0' && glyph <= '9' )
        {
            out = static_cast< u8 >( glyph - '0' );
            return true;
        }

        if ( glyph >= 'a' && glyph <= 'f' )
        {
            out = static_cast< u8 >( glyph - 'a' + 10 );
            return true;
        }

        if ( glyph >= 'A' && glyph <= 'F' )
        {
            out = static_cast< u8 >( glyph - 'A' + 10 );
            return true;
        }

        return false;
    }

    static char hex_digit( u8 nibble )
    {
        return nibble < 10 ? static_cast< char >( '0' + nibble )
                           : static_cast< char >( 'A' + ( nibble - 10 ) );
    }

    static bool copy_hex( const char *text, usize length )
    {
        const usize bytes{ length + 2 };
        HGLOBAL     memory{ I( GlobalAlloc )( GMEM_MOVEABLE, bytes ) };

        if ( !memory )
        {
            return false;
        }

        char *clipboard{ static_cast< char * >( I( GlobalLock )( memory ) ) };

        if ( !clipboard )
        {
            I( GlobalFree )( memory );
            return false;
        }

        clipboard[ 0 ] = '#';

        for ( usize index{ }; index < length; ++index )
        {
            clipboard[ index + 1 ] = text[ index ];
        }

        clipboard[ length + 1 ] = 0;
        I( GlobalUnlock )( memory );

        if ( !I( OpenClipboard )( static_cast< HWND >( g_input.window( ) ) ) )
        {
            I( GlobalFree )( memory );
            return false;
        }

        bool copied{ };

        if ( I( EmptyClipboard )( ) )
        {
            copied = I( SetClipboardData )( CF_TEXT, memory ) != nullptr;
        }

        I( CloseClipboard )( );

        if ( !copied )
        {
            I( GlobalFree )( memory );
        }

        return copied;
    }

    static usize paste_hex( char *out, usize capacity )
    {
        if ( !capacity || !I( OpenClipboard )( static_cast< HWND >( g_input.window( ) ) ) )
        {
            return 0;
        }

        HGLOBAL memory{ static_cast< HGLOBAL >( I( GetClipboardData )( CF_TEXT ) ) };

        if ( !memory )
        {
            I( CloseClipboard )( );
            return 0;
        }

        const char *clipboard{ static_cast< const char * >( I( GlobalLock )( memory ) ) };

        if ( !clipboard )
        {
            I( CloseClipboard )( );
            return 0;
        }

        const char *cursor{ clipboard };
        const char *hash{ };

        for ( const char *scan{ clipboard }; *scan; ++scan )
        {
            if ( *scan == '#' )
            {
                hash = scan;
                break;
            }
        }

        if ( hash )
        {
            cursor = hash + 1;
        } else
        {
            while ( *cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n' )
            {
                ++cursor;
            }
        }

        usize length{ };

        while ( *cursor && length < capacity )
        {
            u8 nibble{ };

            if ( !hex_value( *cursor, nibble ) )
            {
                break;
            }

            out[ length++ ] = hex_digit( nibble );
            ++cursor;
        }

        out[ length ] = 0;
        I( GlobalUnlock )( memory );
        I( CloseClipboard )( );
        return length;
    }

    static void write_hex( char ( &buffer )[ 12 ], color_t value, bool with_alpha )
    {
        const u8    channels[ 4 ]{ value.r, value.g, value.b, value.a };
        const usize count{ with_alpha ? 4u : 3u };

        usize cursor{ };

        for ( usize index{ }; index < count; ++index )
        {
            buffer[ cursor++ ] = hex_digit( static_cast< u8 >( channels[ index ] >> 4 ) );
            buffer[ cursor++ ] = hex_digit( static_cast< u8 >( channels[ index ] & 0x0F ) );
        }

        buffer[ cursor ] = 0;
    }

    static bool read_hex( const char *text, usize length, color_t &out )
    {
        const usize first{ length && text[ 0 ] == '#' ? 1u : 0u };
        const usize digits{ length - first };

        if ( digits != 6 && digits != 8 )
        {
            return false;
        }

        u8 channels[ 4 ]{ 0, 0, 0, out.a };

        for ( usize index{ }; index * 2 < digits; ++index )
        {
            u8 high{ };
            u8 low{ };

            if ( !hex_value( text[ first + index * 2 ], high ) ||
                 !hex_value( text[ first + index * 2 + 1 ], low ) )
            {
                return false;
            }

            channels[ index ] = static_cast< u8 >( ( high << 4 ) | low );
        }

        out = color_t{ channels[ 0 ], channels[ 1 ], channels[ 2 ], channels[ 3 ] };
        return true;
    }

    static void fill_vertical_gradient( rect_t bounds, color_t top, color_t bottom )
    {
        const i32 height{ bounds.bottom - bounds.top };

        if ( height <= 0 || bounds.right <= bounds.left )
        {
            return;
        }

        const i32 divisor{ height > 1 ? height - 1 : 1 };

        for ( i32 row{ }; row < height; ++row )
        {
            const u8 amount{ static_cast< u8 >( row * 255 / divisor ) };

            g_render.box( rect_t{ bounds.left, bounds.top + row, bounds.right,
                                  bounds.top + row + 1 },
                          g_gui.color( mix_color( top, bottom, amount ) ), true );
        }
    }

    static void fill_hue_strip( rect_t bounds )
    {
        const i32 height{ bounds.bottom - bounds.top };

        if ( height <= 0 || bounds.right <= bounds.left )
        {
            return;
        }

        const i32 divisor{ height > 1 ? height - 1 : 1 };

        for ( i32 row{ }; row < height; ++row )
        {
            const float hue{ static_cast< float >( row ) / static_cast< float >( divisor ) };

            g_render.box( rect_t{ bounds.left, bounds.top + row, bounds.right,
                                  bounds.top + row + 1 },
                          g_gui.color( from_hsv( hue, 1.0f, 1.0f, 255 ) ), true );
        }
    }

    static void fill_checker( rect_t bounds )
    {
        const i32 scaled{ g_gui.scaled( PICKER_CHECKER_SIZE ) };
        const i32 cell{ scaled > 0 ? scaled : 1 };

        g_render.box( bounds, g_gui.color( PICKER_CHECKER_LIGHT ), true );

        i32 parity{ };

        for ( i32 top{ bounds.top }; top < bounds.bottom; top += cell, parity ^= 1 )
        {
            const i32 bottom{ math::smaller( top + cell, bounds.bottom ) };

            for ( i32 left{ bounds.left + parity * cell }; left < bounds.right;
                  left += cell * 2 )
            {
                g_render.box( rect_t{ left, top, math::smaller( left + cell, bounds.right ),
                                      bottom },
                              g_gui.color( PICKER_CHECKER_DARK ), true );
            }
        }
    }

    static void stroke_well( rect_t bounds, color_t value )
    {
        g_render.box( rect_t{ bounds.left, bounds.top, bounds.right - 1, bounds.bottom - 1 },
                      g_gui.color( value ) );
    }

    static void draw_bar_knob( rect_t bar, float progress )
    {
        const i32 bleed{ g_gui.scaled( PICKER_KNOB_BLEED ) };
        const i32 height{ g_gui.scaled( PICKER_KNOB_HEIGHT ) };
        const i32 travel{ bar.bottom - 1 - bar.top };
        const i32 center{ bar.top +
                          static_cast< i32 >( progress * static_cast< float >( travel ) +
                                              0.5f ) };
        const i32 top{ center - height / 2 };

        const rect_t knob{ bar.left - bleed, top, bar.right + bleed, top + height };

        g_render.box( knob, g_gui.color( PICKER_CURSOR_SHADOW ), true );
        g_render.box( rect_t{ knob.left + 1, knob.top + 1, knob.right - 1, knob.bottom - 1 },
                      g_gui.color( PICKER_CURSOR_COLOR ), true );
    }

    void color_picker::setup( )
    {
        this->pull( );
    }

    i32 color_picker::measure( ) const
    {
        return g_gui.scaled( WIDGET_KEYBIND_HEIGHT );
    }

    i32 color_picker::measure_width( ) const
    {
        return g_gui.scaled( PICKER_SWATCH_WIDTH );
    }

    rect_t color_picker::occupied( ) const
    {
        return this->swatch_bounds( );
    }

    rect_t color_picker::swatch_bounds( ) const
    {
        const i32 row{ this->is_module( ) ? this->m_bounds.bottom - this->m_bounds.top
                                          : g_gui.scaled( WIDGET_KEYBIND_HEIGHT ) };
        const i32 width{ g_gui.scaled( PICKER_SWATCH_WIDTH ) };
        const i32 height{ g_gui.scaled( PICKER_SWATCH_HEIGHT ) };
        const i32 top{ this->m_bounds.top + ( row - height ) / 2 };

        return rect_t{ this->m_bounds.right - width, top, this->m_bounds.right, top + height };
    }

    rect_t color_picker::risen_popup( ) const
    {
        const i32 travel{ g_gui.scaled( PICKER_POPUP_RISE ) *
                          ( 255 - static_cast< i32 >( this->m_progress ) ) / 255 };
        const i32 shift{ this->m_upward ? travel : -travel };

        return rect_t{ this->m_popup.left, this->m_popup.top + shift, this->m_popup.right,
                       this->m_popup.bottom + shift };
    }

    rect_t color_picker::place_popup( rect_t swatch )
    {
        const i32 padding{ g_gui.scaled( PICKER_POPUP_PADDING ) };
        const i32 field{ g_gui.scaled( PICKER_FIELD_SIZE ) };
        const i32 bar{ g_gui.scaled( PICKER_BAR_WIDTH ) };
        const i32 gap{ g_gui.scaled( PICKER_BAR_GAP ) };
        const i32 drop{ g_gui.scaled( PICKER_POPUP_GAP ) };

        i32 width{ padding * 2 + field + gap + bar };

        if ( this->m_show_alpha )
        {
            width += gap + bar;
        }

        const i32 height{ padding * 2 + field + g_gui.scaled( PICKER_ROW_GAP ) +
                          g_gui.scaled( PICKER_FOOTER_HEIGHT ) };

        const rect_t room{ g_gui.content_room( ) };

        i32 left{ swatch.right - width };

        if ( left + width > room.right )
        {
            left = room.right - width;
        }

        if ( left < room.left )
        {
            left = room.left;
        }

        this->m_upward = swatch.bottom + drop + height > room.bottom &&
                         swatch.top - drop - height >= room.top;

        const i32 top{ this->m_upward ? swatch.top - drop - height : swatch.bottom + drop };

        return rect_t{ left, top, left + width, top + height };
    }

    rect_t color_picker::field_bounds( rect_t popup ) const
    {
        const i32 padding{ g_gui.scaled( PICKER_POPUP_PADDING ) };
        const i32 size{ g_gui.scaled( PICKER_FIELD_SIZE ) };

        return rect_t{ popup.left + padding, popup.top + padding, popup.left + padding + size,
                       popup.top + padding + size };
    }

    rect_t color_picker::hue_bounds( rect_t popup ) const
    {
        const rect_t field{ this->field_bounds( popup ) };
        const i32    left{ field.right + g_gui.scaled( PICKER_BAR_GAP ) };

        return rect_t{ left, field.top, left + g_gui.scaled( PICKER_BAR_WIDTH ), field.bottom };
    }

    rect_t color_picker::alpha_bounds( rect_t popup ) const
    {
        const rect_t hue{ this->hue_bounds( popup ) };
        const i32    left{ hue.right + g_gui.scaled( PICKER_BAR_GAP ) };

        return rect_t{ left, hue.top, left + g_gui.scaled( PICKER_BAR_WIDTH ), hue.bottom };
    }

    rect_t color_picker::preview_bounds( rect_t popup ) const
    {
        const i32 padding{ g_gui.scaled( PICKER_POPUP_PADDING ) };
        const i32 height{ g_gui.scaled( PICKER_FOOTER_HEIGHT ) };
        const i32 top{ popup.bottom - padding - height };

        return rect_t{ popup.left + padding, top,
                       popup.left + padding + g_gui.scaled( PICKER_PREVIEW_WIDTH ),
                       top + height };
    }

    rect_t color_picker::hex_bounds( rect_t popup ) const
    {
        const rect_t preview{ this->preview_bounds( popup ) };

        return rect_t{ preview.right + g_gui.scaled( PICKER_BAR_GAP ), preview.top,
                       popup.right - g_gui.scaled( PICKER_POPUP_PADDING ), preview.bottom };
    }

    void color_picker::pull( )
    {
        if ( !this->m_value )
        {
            return;
        }

        const color_t current{ *this->m_value };

        if ( this->m_synced && current.r == this->m_mirror.r &&
             current.g == this->m_mirror.g && current.b == this->m_mirror.b )
        {
            return;
        }

        const hsv_t read{ to_hsv( current ) };

        if ( read.m_saturation > 0.0f )
        {
            this->m_hue = read.m_hue;
        }

        if ( read.m_brightness > 0.0f )
        {
            this->m_saturation = read.m_saturation;
        }

        this->m_brightness = read.m_brightness;
        this->m_mirror     = current;
        this->m_synced     = true;
    }

    void color_picker::push( u8 alpha )
    {
        *this->m_value = from_hsv( this->m_hue, this->m_saturation, this->m_brightness,
                                   this->m_show_alpha ? alpha : static_cast< u8 >( 255 ) );

        this->m_mirror = *this->m_value;
        this->m_synced = true;

        if ( this->m_callback )
        {
            this->m_callback( );
        }
    }

    void color_picker::adopt( color_t value )
    {
        *this->m_value = value;
        this->m_synced = false;

        this->pull( );

        if ( this->m_callback )
        {
            this->m_callback( );
        }
    }

    void color_picker::begin_hex( )
    {
        char text[ 12 ]{ };

        write_hex( text, *this->m_value, this->m_show_alpha );

        const usize length{ str::length( text ) };

        for ( usize index{ }; index <= length; ++index )
        {
            this->m_hex.m_data[ index ] = text[ index ];
        }

        this->m_hex.m_length      = length;
        this->m_hex.m_caret       = length;
        this->m_hex.m_scroll      = 0;
        this->m_hex.m_caret_ready = false;
        this->m_hex.m_edit_tick   = clock( );

        this->m_hex_focused = true;

        g_gui.set_focused_input( this );
        g_gui.set_listening( nullptr );
    }

    void color_picker::release_hex( )
    {
        if ( !this->m_hex_focused )
        {
            return;
        }

        this->m_hex_focused = false;

        if ( g_gui.focused_input( ) == this )
        {
            g_gui.set_focused_input( nullptr );
        }
    }

    void color_picker::trim_hex( )
    {
        const usize limit{ this->m_show_alpha ? 8u : 6u };
        const usize caret{ this->m_hex.m_caret };

        usize cursor{ };
        usize dropped{ };

        for ( usize index{ }; index < this->m_hex.m_length; ++index )
        {
            u8 nibble{ };

            if ( cursor >= limit || !hex_value( this->m_hex.m_data[ index ], nibble ) )
            {
                dropped += index < caret ? 1u : 0u;
                continue;
            }

            this->m_hex.m_data[ cursor++ ] = hex_digit( nibble );
        }

        this->m_hex.m_length         = cursor;
        this->m_hex.m_data[ cursor ] = 0;
        this->m_hex.m_caret          = caret > dropped ? caret - dropped : 0;

        if ( this->m_hex.m_caret > cursor )
        {
            this->m_hex.m_caret = cursor;
        }
    }

    void color_picker::apply_hex( )
    {
        color_t parsed{ *this->m_value };

        if ( !read_hex( this->m_hex.m_data, this->m_hex.m_length, parsed ) )
        {
            return;
        }

        if ( !this->m_show_alpha )
        {
            parsed.a = 255;
        }

        this->adopt( parsed );
    }

    void color_picker::think_hex( u32 delta )
    {
        const rect_t field{ this->hex_bounds( this->m_popup ) };
        const bool   hovered{ over( field ) };

        this->m_hex_focused = g_gui.focused_input( ) == this;

        if ( g_input.pressed( VK_LBUTTON ) )
        {
            if ( hovered && !this->m_hex_focused )
            {
                this->begin_hex( );
            } else if ( !hovered && this->m_hex_focused )
            {
                this->release_hex( );
            }
        }

        if ( this->m_hex_focused )
        {
            bool pasted{ };

            if ( g_input.down( VK_CONTROL ) && g_input.pressed( 'C' ) )
            {
                copy_hex( this->m_hex.m_data, this->m_hex.m_length );
            }

            if ( g_input.down( VK_CONTROL ) && g_input.pressed( 'V' ) )
            {
                char        clipboard[ 9 ]{ };
                const usize length{ paste_hex( clipboard, 8 ) };

                if ( length == 6 || length == 8 )
                {
                    const usize kept{ this->m_show_alpha ? length : 6u };

                    for ( usize index{ }; index < kept; ++index )
                    {
                        this->m_hex.m_data[ index ] = clipboard[ index ];
                    }

                    this->m_hex.m_data[ kept ] = 0;
                    this->m_hex.m_length       = kept;
                    this->m_hex.m_caret        = kept;
                    this->m_hex.m_scroll       = 0;
                    this->m_hex.m_caret_ready  = false;
                    pasted                     = true;
                }
            }

            const edit_result_t result{ edit_text( this->m_hex ) };

            if ( pasted || result != edit_result_t::idle )
            {
                this->m_hex.m_edit_tick = clock( );

                this->trim_hex( );
                this->apply_hex( );
            }

            if ( result == edit_result_t::released )
            {
                this->release_hex( );
            }
        }

        const i32 caret_target{ measure_range( this->m_hex.m_data, 0, this->m_hex.m_caret ) };

        if ( !this->m_hex.m_caret_ready )
        {
            this->m_hex.m_caret_x     = static_cast< float >( caret_target );
            this->m_hex.m_caret_ready = true;
        }

        this->m_hex.m_caret_x = approach_float( this->m_hex.m_caret_x,
                                                static_cast< float >( caret_target ), delta,
                                                INPUT_CARET_GLIDE );

        step_mix( this->m_hex_hover_mix, hovered, delta, WIDGET_HOVER_DURATION );
        step_mix( this->m_hex_focus_mix, this->m_hex_focused, delta, INPUT_FOCUS_DURATION );
    }

    void color_picker::think_grab( )
    {
        const rect_t field{ this->field_bounds( this->m_popup ) };
        const rect_t hue{ this->hue_bounds( this->m_popup ) };
        const rect_t alpha{ this->alpha_bounds( this->m_popup ) };

        if ( this->m_grab == grab_t::none && g_input.pressed( VK_LBUTTON ) )
        {
            if ( over( field ) )
            {
                this->m_grab = grab_t::field;
            } else if ( over( hue ) )
            {
                this->m_grab = grab_t::hue;
            } else if ( this->m_show_alpha && over( alpha ) )
            {
                this->m_grab = grab_t::alpha;
            }

            if ( this->m_grab != grab_t::none )
            {
                this->release_hex( );

                g_gui.set_active_slider( this );
            }
        }

        if ( this->m_grab == grab_t::none )
        {
            return;
        }

        if ( !g_input.down( VK_LBUTTON ) || !g_gui.can_interact( ) )
        {
            this->m_grab = grab_t::none;

            if ( g_gui.active_slider( ) == this )
            {
                g_gui.set_active_slider( nullptr );
            }

            return;
        }

        const point_t mouse{ g_input.mouse( ) };

        switch ( this->m_grab )
        {
            case grab_t::field:
                this->m_saturation = fraction( mouse.x, field.left, field.right - 1 );
                this->m_brightness = 1.0f - fraction( mouse.y, field.top, field.bottom - 1 );
                this->push( this->m_value->a );
                break;

            case grab_t::hue:
                this->m_hue = fraction( mouse.y, hue.top, hue.bottom - 1 );
                this->push( this->m_value->a );
                break;

            case grab_t::alpha:
                this->push(
                    channel_of( 1.0f - fraction( mouse.y, alpha.top, alpha.bottom - 1 ) ) );
                break;

            default:
                break;
        }
    }

    void color_picker::collapse( )
    {
        this->m_expanded = false;
        this->m_grab     = grab_t::none;

        this->release_hex( );

        if ( g_gui.open_popup( ) == this )
        {
            g_gui.set_open_popup( nullptr );
        }
    }

    void color_picker::think( )
    {
        if ( !this->m_value )
        {
            return;
        }

        this->pull( );

        if ( this->m_expanded && g_gui.open_popup( ) != this )
        {
            this->collapse( );
        }

        const rect_t swatch{ this->swatch_bounds( ) };

        this->m_hovered = g_gui.hit_test( swatch );

        if ( this->m_hovered && g_input.pressed( VK_LBUTTON ) )
        {
            if ( this->m_expanded )
            {
                this->collapse( );
            } else
            {
                this->m_expanded = true;
                g_gui.set_open_popup( this );
            }
        }

        const u32 delta{ animation_delta( this->m_last_tick ) };

        step_mix( this->m_hover_mix, this->m_hovered, delta, WIDGET_HOVER_DURATION );
        step_mix( this->m_expand_mix, this->m_expanded, delta,
                  this->m_expanded ? PICKER_EXPAND_DURATION : PICKER_COLLAPSE_DURATION );

        this->m_progress = eased_out( this->m_expand_mix );
        this->m_popup    = this->place_popup( swatch );

        if ( !this->m_expanded )
        {
            this->m_grab = grab_t::none;

            this->release_hex( );

            step_mix( this->m_hex_hover_mix, false, delta, WIDGET_HOVER_DURATION );
            step_mix( this->m_hex_focus_mix, false, delta, INPUT_FOCUS_DURATION );

            if ( this->m_expand_mix )
            {
                g_gui.block_hits( this->risen_popup( ) );
            }

            return;
        }

        g_gui.block_hits( this->risen_popup( ) );

        this->think_hex( delta );
        this->think_grab( );

        if ( this->m_grab == grab_t::none && g_input.pressed( VK_LBUTTON ) )
        {
            const point_t mouse{ g_input.mouse( ) };

            if ( !contains( this->m_popup, mouse ) && !contains( swatch, mouse ) )
            {
                this->collapse( );
            }
        }
    }

    void color_picker::draw( ) const
    {
        if ( !this->m_value )
        {
            return;
        }

        const rect_t swatch{ this->swatch_bounds( ) };
        const u8     hover_mix{ eased_mix( this->m_hover_mix ) };

        if ( !this->is_module( ) )
        {
            const i32 row_height{ g_gui.scaled( WIDGET_KEYBIND_HEIGHT ) };

            if ( hover_mix )
            {
                g_render.box( this->hover_strip( row_height ),
                              g_gui.color( with_alpha( ROW_HOVER_BACKGROUND, hover_mix ) ),
                              true );
            }

            g_render.text( MENU_FONT( ),
                           point_t{ this->m_bounds.left,
                                    g_gui.text_center_y( this->m_bounds.top, row_height ) },
                           g_gui.color( CONTROL_LABEL ), this->m_name );
        }

        if ( this->m_value->a != 255 )
        {
            fill_checker( swatch );
        }

        g_render.box( swatch, g_gui.color( *this->m_value ), true );

        stroke_well( swatch, this->m_expanded ? g_theme.m_accent
                                              : mix_color( SWATCH_BORDER, CONTROL_BORDER_HOVER,
                                                           hover_mix ) );
    }

    void color_picker::draw_last( ) const
    {
        if ( !this->m_value || !this->m_expand_mix )
        {
            return;
        }

        const i32 travel{ g_gui.scaled( PICKER_POPUP_RISE ) *
                          ( 255 - static_cast< i32 >( this->m_progress ) ) / 255 };
        const i32 shift{ this->m_upward ? travel : -travel };

        const rect_t popup{ this->risen_popup( ) };

        const u8 settled{ g_gui.fade( ) };

        g_gui.set_fade( static_cast< u8 >(
            ( static_cast< u32 >( settled ) * this->m_progress + 127u ) / 255u ) );

        this->draw_popup( popup );

        g_gui.set_fade( settled );
    }

    void color_picker::draw_popup( rect_t popup ) const
    {
        g_render.box( popup, g_gui.color( PICKER_POPUP_BACKGROUND ), true );

        this->draw_field( this->field_bounds( popup ) );
        this->draw_hue( this->hue_bounds( popup ) );

        if ( this->m_show_alpha )
        {
            this->draw_alpha( this->alpha_bounds( popup ) );
        }

        this->draw_footer( popup );

        stroke_well( popup, g_theme.m_accent );
    }

    void color_picker::draw_field( rect_t field ) const
    {
        constexpr color_t white{ 255, 255, 255 };
        constexpr color_t clear_black{ 0, 0, 0, 0 };
        constexpr color_t solid_black{ 0, 0, 0, 255 };

        fill_horizontal_gradient( field, white, from_hsv( this->m_hue, 1.0f, 1.0f, 255 ) );
        fill_vertical_gradient( field, clear_black, solid_black );

        stroke_well( field, SLIDER_BORDER );

        const point_t cursor{
            field.left +
                static_cast< i32 >(
                    this->m_saturation *
                        static_cast< float >( field.right - 1 - field.left ) +
                    0.5f ),
            field.top +
                static_cast< i32 >(
                    ( 1.0f - this->m_brightness ) *
                        static_cast< float >( field.bottom - 1 - field.top ) +
                    0.5f )
        };

        const i32     radius{ g_gui.scaled( PICKER_CURSOR_RADIUS ) };
        const color_t none{ 0, 0, 0, 0 };

        fill_ring( cursor, radius + 1, 1, 1.0f, none, g_gui.color( PICKER_CURSOR_SHADOW ),
                   true );
        fill_ring( cursor, radius, 2, 1.0f, none, g_gui.color( PICKER_CURSOR_COLOR ), true );
    }

    void color_picker::draw_hue( rect_t bar ) const
    {
        fill_hue_strip( bar );
        stroke_well( bar, SLIDER_BORDER );

        draw_bar_knob( bar, this->m_hue );
    }

    void color_picker::draw_alpha( rect_t bar ) const
    {
        const color_t opaque{ this->m_value->r, this->m_value->g, this->m_value->b, 255 };
        const color_t clear{ this->m_value->r, this->m_value->g, this->m_value->b, 0 };

        fill_checker( bar );
        fill_vertical_gradient( bar, opaque, clear );
        stroke_well( bar, SLIDER_BORDER );

        draw_bar_knob( bar, 1.0f - static_cast< float >( this->m_value->a ) / 255.0f );
    }

    void color_picker::draw_footer( rect_t popup ) const
    {
        const rect_t preview{ this->preview_bounds( popup ) };
        const rect_t field{ this->hex_bounds( popup ) };

        if ( this->m_value->a != 255 )
        {
            fill_checker( preview );
        }

        g_render.box( preview, g_gui.color( *this->m_value ), true );
        stroke_well( preview, SLIDER_BORDER );

        const u8 hover_mix{ eased_mix( this->m_hex_hover_mix ) };
        const u8 focus_mix{ eased_out( this->m_hex_focus_mix ) };
        const u8 lit{ hover_mix > focus_mix ? hover_mix : focus_mix };

        color_t background{ mix_color( PICKER_HEX_BACKGROUND, CONTROL_BACKGROUND_HOVER,
                                       hover_mix ) };
        background = mix_color( background, INPUT_BACKGROUND_FOCUS, focus_mix );

        color_t border{ mix_color( CONTROL_BORDER, CONTROL_BORDER_HOVER, hover_mix ) };
        border = mix_color( border, g_theme.m_input_focus_border, focus_mix );

        g_render.box( field, g_gui.color( background ), true );
        stroke_well( field, border );

        char text[ 12 ]{ };

        if ( !this->m_hex_focused )
        {
            write_hex( text, *this->m_value, this->m_show_alpha );
        }

        const string_view body{ this->m_hex_focused
                                    ? string_view{ this->m_hex.m_data, this->m_hex.m_length }
                                    : string_view{ text, str::length( text ) } };

        const i32 padding{ g_gui.scaled( PICKER_HEX_PADDING ) };
        const i32 text_y{ g_gui.text_center_y( field.top, field.bottom - field.top ) };
        const i32 body_left{ field.left + padding + estimate_text_width( "#" ) };

        g_render.text( MENU_FONT( ), point_t{ field.left + padding, text_y },
                       g_gui.color( MENU_TEXT_DIM ), "#" );

        g_render.text( MENU_FONT( ), point_t{ body_left, text_y },
                       g_gui.color( mix_color( MENU_TEXT_MUTED, INPUT_TEXT, lit ) ), body );

        if ( !focus_mix )
        {
            return;
        }

        const u64 now{ clock( ) };
        const u64 since{ now >= this->m_hex.m_edit_tick ? now - this->m_hex.m_edit_tick : 0 };
        const u8  blink{ since < INPUT_CARET_SOLID
                             ? static_cast< u8 >( 255 )
                             : pulse( since - INPUT_CARET_SOLID + INPUT_CARET_PERIOD / 2,
                                      INPUT_CARET_PERIOD ) };
        const u8  caret_alpha{ static_cast< u8 >(
            ( static_cast< u32 >( blink ) * focus_mix + 127u ) / 255u ) };

        const i32 caret_x{ body_left + static_cast< i32 >( this->m_hex.m_caret_x + 0.5f ) };
        const i32 caret_top{ text_y + g_gui.scaled( LABEL_FONT_INK_TOP ) - 1 };
        const i32 thickness{ g_gui.scaled( 1 ) > 0 ? g_gui.scaled( 1 ) : 1 };

        g_render.box( rect_t{ caret_x, caret_top, caret_x + thickness,
                              caret_top + g_gui.scaled( LABEL_FONT_INK_HEIGHT ) + 2 },
                      g_gui.color( with_alpha( g_theme.m_accent, caret_alpha ) ), true );
    }
} // namespace gui
