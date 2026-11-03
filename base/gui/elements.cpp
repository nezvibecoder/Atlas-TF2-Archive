#include "input/input.hpp"
#include "math/math.hpp"
#include "render/render.hpp"
#include "util/format.hpp"
#include "util/string.hpp"
#include "util/windefs.hpp"

#include "gui.hpp"
#include "theme.hpp"

namespace gui
{
    static bool contains( rect_t bounds, point_t point )
    {
        return point.x >= bounds.left && point.x < bounds.right && point.y >= bounds.top &&
               point.y < bounds.bottom;
    }

    static bool row_hovered( rect_t bounds )
    {
        return g_gui.can_interact( ) &&
               g_input.in_area( bounds.left, bounds.top, bounds.right - bounds.left - 1,
                                bounds.bottom - bounds.top - 1 );
    }

    struct tooltip_metrics_t
    {
        i32 width{ };
        i32 lines{ 1 };
    };

    static tooltip_metrics_t measure_tooltip( string_view text )
    {
        tooltip_metrics_t result{ };
        usize             line_start{ };

        for ( usize index{ }; index <= text.size( ); ++index )
        {
            if ( index < text.size( ) && text[ index ] != '\n' )
            {
                continue;
            }

            result.width = math::bigger(
                result.width,
                estimate_text_width( text.substr( line_start, index - line_start ) ) );

            if ( index < text.size( ) )
            {
                ++result.lines;
                line_start = index + 1;
            }
        }

        return result;
    }

    static bool row_think( row_state_t &state, bool selected, bool hovered )
    {
        const u32 delta{ animation_delta( state.m_last_tick ) };

        step_mix( state.m_active_mix, selected, delta, WIDGET_TOGGLE_DURATION );
        step_mix( state.m_hover_mix, hovered, delta, WIDGET_HOVER_DURATION );

        if ( hovered )
        {
            g_gui.set_cursor_state( CURSOR_STATE_CLICK_HAND );
        }

        if ( hovered && !state.m_hovered )
        {
            play_sound( SOUND_HOVER );
        }

        state.m_hovered = hovered;

        if ( !hovered || !g_input.pressed( VK_LBUTTON ) )
        {
            return false;
        }

        play_sound( SOUND_CLICK );
        return true;
    }

    static void row_draw( rect_t bounds, string_view label, const row_state_t &state,
                          u8 opacity, bool bar )
    {
        const i32 row_height{ bounds.bottom - bounds.top };
        const u8  active_mix{ eased_mix( state.m_active_mix ) };
        const u8  hover_mix{ eased_mix( state.m_hover_mix ) };

        color_t background{ mix_color( color_t{ 0, 0, 0, 0 }, POPUP_ROW_HOVER, hover_mix ) };
        background = mix_color( background, g_theme.m_row_selected_background, active_mix );

        if ( background.a )
        {
            g_render.box( bounds, g_gui.color( with_alpha( background, opacity ) ), true );
        }

        i32 left{ bounds.left + g_gui.scaled( WIDGET_LIST_PADDING ) };

        if ( bar && active_mix )
        {
            color_t edge{ g_theme.m_accent };
            edge.a = active_mix;
            g_render.box( rect_t{ bounds.left, bounds.top,
                                  bounds.left + g_gui.scaled( MENU_SELECTION_BAR ),
                                  bounds.bottom },
                          g_gui.color( with_alpha( edge, opacity ) ), true );
        }

        color_t label_color{ mix_color( LIST_TEXT_IDLE, MENU_TEXT, hover_mix ) };
        label_color = mix_color( label_color, LIST_TEXT_ACTIVE, active_mix );

        g_render.text( MENU_FONT( ),
                       point_t{ left, g_gui.text_center_y( bounds.top, row_height ) },
                       g_gui.color( with_alpha( label_color, opacity ) ), label );
    }

    rect_t baseobject::hover_strip( i32 height ) const
    {
        const i32 bleed{ g_gui.scaled( WIDGET_ROW_BLEED ) };
        const i32 right{ this->m_parent
                             ? this->m_parent->bounds( ).right - g_gui.scaled( CARD_PADDING_X )
                             : this->m_bounds.right };

        return rect_t{ this->m_bounds.left - bleed, this->m_bounds.top, right + bleed,
                       this->m_bounds.top + height };
    }

    void baseobject::step_tooltip( u32 delta )
    {
        const bool wanted{ this->m_hovered &&
                           ( !this->m_tooltip.empty( ) || !this->m_script_name.empty( ) ) };

        if ( wanted )
        {
            this->m_tooltip_anchor = g_input.mouse( );
        }

        step_mix( this->m_tooltip_mix, wanted, delta,
                  wanted ? TOOLTIP_FADE_IN_DURATION : TOOLTIP_FADE_OUT_DURATION );
    }

    void baseobject::set_cursor( ) const
    {
        g_gui.set_cursor_state( CURSOR_STATE_CLICK_HAND );
    }

    void baseobject::draw_tooltip( ) const
    {
        const u8 alpha{ eased_mix( this->m_tooltip_mix ) };

        if ( !alpha )
        {
            return;
        }

        tooltip_metrics_t metrics{ 0, 0 };

        if ( !this->m_tooltip.empty( ) )
        {
            metrics = measure_tooltip( this->m_tooltip );
        }

        const i32 script_lines{ this->m_script_name.empty( ) ? 0 : 1 };

        if ( script_lines )
        {
            metrics.width = math::bigger( metrics.width, estimate_text_width( this->m_script_name ) );
        }

        const i32    padding{ g_gui.scaled( TOOLTIP_PADDING_X ) };
        const i32    line_height{ g_gui.scaled( TOOLTIP_HEIGHT ) };
        const i32    line_advance{ g_gui.scaled( TOOLTIP_LINE_ADVANCE ) };
        const i32    height{ line_height + ( metrics.lines + script_lines - 1 ) * line_advance };
        const i32    width{ metrics.width + padding * 2 };
        const rect_t room{ g_gui.screen_room( ) };

        const point_t anchor{ this->m_tooltip_anchor };

        i32 left{ anchor.x + g_gui.scaled( TOOLTIP_OFFSET_X ) };
        i32 top{ anchor.y + g_gui.scaled( TOOLTIP_OFFSET_Y ) };

        if ( left + width > room.right )
        {
            left = anchor.x - g_gui.scaled( TOOLTIP_OFFSET_X ) - width;
        }

        if ( top + height > room.bottom )
        {
            top = anchor.y - g_gui.scaled( TOOLTIP_OFFSET_Y ) - height;
        }

        if ( width <= room.right - room.left )
        {
            left = math::clamp( left, room.left, room.right - width );
        }

        if ( height <= room.bottom - room.top )
        {
            top = math::clamp( top, room.top, room.bottom - height );
        }

        const rect_t bounds{ left, top, left + width, top + height };

        g_render.box( bounds, g_gui.color( with_alpha( TOOLTIP_BACKGROUND, alpha ) ), true );
        g_render.box( rect_t{ bounds.left, bounds.top, bounds.right - 1, bounds.bottom - 1 },
                      g_gui.color( with_alpha( TOOLTIP_BORDER, alpha ) ) );

        if ( metrics.lines )
        {
            g_render.text( MENU_FONT( ),
                           point_t{ bounds.left + padding,
                                    g_gui.text_center_y( bounds.top, line_height ) },
                           g_gui.color( with_alpha( TOOLTIP_TEXT, alpha ) ), this->m_tooltip );
        }

        if ( script_lines )
        {
            g_render.text( MENU_FONT( ),
                           point_t{ bounds.left + padding,
                                    g_gui.text_center_y( bounds.top + metrics.lines * line_advance,
                                                         line_height ) },
                           g_gui.color( with_alpha( MENU_TEXT_MUTED, alpha ) ),
                           this->m_script_name );
        }
    }

    void checkbox::setup( )
    {
        this->m_active_mix = this->m_value && *this->m_value ? 255 : 0;
    }

    i32 checkbox::measure( ) const
    {
        return g_gui.scaled( WIDGET_ROW_HEIGHT );
    }

    i32 checkbox::control_height( ) const
    {
        return g_gui.scaled( WIDGET_ROW_HEIGHT );
    }

    rect_t checkbox::hit_row( ) const
    {
        return rect_t{ this->m_bounds.left, this->m_bounds.top, this->module_edge( ),
                       this->m_bounds.top + g_gui.scaled( WIDGET_ROW_HEIGHT ) };
    }

    void checkbox::think( )
    {
        if ( !this->m_value )
        {
            return;
        }

        const rect_t row{ this->hit_row( ) };

        this->m_hovered = g_gui.hit_test( row );
        if ( this->m_hovered )
        {
            set_cursor( );
        }

        if ( this->m_hovered && g_input.pressed( VK_LBUTTON ) )
        {
            *this->m_value = !*this->m_value;
            play_sound( *this->m_value ? SOUND_TOGGLE_ON : SOUND_TOGGLE_OFF );
        }

        const u32 delta{ animation_delta( this->m_last_tick ) };
        step_mix( this->m_active_mix, *this->m_value, delta, WIDGET_TOGGLE_DURATION );
        step_mix( this->m_hover_mix, this->m_hovered, delta, WIDGET_HOVER_DURATION );
    }

    void checkbox::draw( ) const
    {
        const i32 row_height{ g_gui.scaled( WIDGET_ROW_HEIGHT ) };
        const i32 box_size{ g_gui.scaled( WIDGET_CHECKBOX_SIZE ) };
        const i32 left{ this->m_bounds.left };
        const i32 top{ this->m_bounds.top };

        const u8 active_mix{ eased_mix( this->m_active_mix ) };
        const u8 hover_mix{ eased_mix( this->m_hover_mix ) };

        if ( hover_mix )
        {
            g_render.box( this->hover_strip( row_height ),
                          g_gui.color( with_alpha( ROW_HOVER_BACKGROUND, hover_mix ) ), true );
        }

        const i32 box_top{ top + ( row_height - box_size ) / 2 };

        const color_t fill_on{ mix_color( g_theme.m_checkbox_top,
                                          g_theme.m_checkbox_top_hover, hover_mix ) };
        const color_t fill_off{ mix_color( CHECKBOX_BOX_IDLE, CONTROL_BACKGROUND_HOVER,
                                           hover_mix ) };
        const color_t border_off{ mix_color( CHECKBOX_BORDER_IDLE, CONTROL_BORDER_HOVER,
                                             hover_mix ) };

        g_render.box( rect_t{ left, box_top, left + box_size, box_top + box_size },
                      g_gui.color( mix_color( fill_off, fill_on, active_mix ) ), true );
        g_render.box( rect_t{ left, box_top, left + box_size - 1, box_top + box_size - 1 },
                      g_gui.color( mix_color( border_off, fill_on, active_mix ) ) );

        color_t label_color{ mix_color( CHECKBOX_TEXT_IDLE, CHECKBOX_TEXT_HOVER, hover_mix ) };
        label_color = mix_color( label_color, CHECKBOX_TEXT_ACTIVE, active_mix );

        g_render.text( MENU_FONT( ),
                       point_t{ left + g_gui.scaled( WIDGET_LABEL_INDENT ),
                                g_gui.text_center_y( top, row_height ) },
                       g_gui.color( label_color ), this->m_name );
    }

    static const char *bind_mode_name( bind_mode_t mode )
    {
        switch ( mode )
        {
            case bind_mode_t::toggle:
                return ATLAS_XORSTR_STATIC( "Toggle" );
            case bind_mode_t::always:
                return ATLAS_XORSTR_STATIC( "Always" );
            default:
                return ATLAS_XORSTR_STATIC( "Hold" );
        }
    }

    bool bind_active( bind_t &bind )
    {
        if ( bind.m_mode == bind_mode_t::always )
        {
            return true;
        }

        if ( !bind.m_key )
        {
            return false;
        }

        if ( bind.m_mode == bind_mode_t::hold )
        {
            return g_input.down_batch( bind.m_key );
        }

        // a toggle flips once per input batch, which is the only unit both timelines agree on:
        // a command-keyed guard replays one press onto every command of a slow frame, and a
        // render-keyed one asks before the commands have published the press
        if ( bind.m_batch != g_input.batch( ) )
        {
            bind.m_batch = g_input.batch( );

            if ( g_input.pressed_batch( bind.m_key ) )
            {
                bind.m_toggled = !bind.m_toggled;
            }
        }

        return bind.m_toggled;
    }

    rect_t keybind::chip_bounds( ) const
    {
        const i32 row_height{ this->is_module( ) ? this->m_bounds.bottom - this->m_bounds.top
                                                 : g_gui.scaled( WIDGET_KEYBIND_HEIGHT ) };
        const i32 chip_height{ g_gui.scaled( WIDGET_CHIP_HEIGHT ) };
        const i32 width{ this->m_chip_width > 0 ? this->m_chip_width
                                                : g_gui.scaled( WIDGET_KEYBIND_MIN_WIDTH ) };
        const i32 top{ this->m_bounds.top + ( row_height - chip_height ) / 2 };

        return rect_t{ this->m_bounds.right - width, top, this->m_bounds.right,
                       top + chip_height };
    }

    rect_t keybind::occupied( ) const
    {
        return this->chip_bounds( );
    }

    i32 keybind::measure( ) const
    {
        return g_gui.scaled( WIDGET_KEYBIND_HEIGHT );
    }

    i32 keybind::popup_height( ) const
    {
        return WIDGET_BIND_MODE_COUNT * g_gui.scaled( WIDGET_LIST_ROW_HEIGHT ) + 2;
    }

    void keybind::collapse( )
    {
        this->m_expanded = false;

        if ( g_gui.open_popup( ) == this )
        {
            g_gui.set_open_popup( nullptr );
        }
    }

    void keybind::think( )
    {
        if ( !this->m_bind )
        {
            return;
        }

        if ( this->m_expanded && g_gui.open_popup( ) != this )
        {
            this->m_expanded = false;
        }

        if ( g_gui.listening( ) == this && g_gui.listen_armed( ) )
        {
            // Allow unbinding with backspace (quake style).
            if ( g_input.pressed( VK_ESCAPE ) || g_input.pressed( VK_BACK ) )
            {
                this->m_bind->m_key = 0;
                g_gui.set_focused_input( nullptr );
                g_gui.set_listening( nullptr );
            } else
            {
                for ( u32 scan{ 1 }; scan < 256; ++scan )
                {
                    if ( scan == VK_INSERT || scan == VK_F3 )
                    {
                        continue;
                    }

                    if ( g_input.pressed( scan ) )
                    {
                        this->m_bind->m_key = scan;
                        g_gui.set_listening( nullptr );
                        break;
                    }
                }
            }
        }

        this->m_listening = g_gui.listening( ) == this;

        const char *chip_text{ this->chip_label( ) };
        const i32   text_width{ estimate_tiny_text_width( chip_text ) };
        const i32   padded{ text_width + g_gui.scaled( WIDGET_CHIP_PADDING ) * 2 +
                            ( this->mode_marked( ) ? g_gui.scaled( MENU_SELECTION_BAR ) : 0 ) };

        i32 target_width{ g_gui.scaled( WIDGET_KEYBIND_MIN_WIDTH ) };

        if ( padded > target_width )
        {
            target_width = padded;
        }

        if ( !this->m_tween_ready )
        {
            this->m_chip_tween  = static_cast< float >( target_width );
            this->m_tween_ready = true;
        }

        const u32 delta{ animation_delta( this->m_last_tick ) };

        this->m_chip_tween = approach_float( this->m_chip_tween,
                                             static_cast< float >( target_width ), delta, 110.0f );
        this->m_chip_width = static_cast< i32 >( this->m_chip_tween + 0.5f );

        const rect_t chip{ this->chip_bounds( ) };

        this->m_hovered = g_gui.hit_test( chip );
        if ( this->m_hovered )
        {
            set_cursor( );
        }

        if ( !this->m_listening && this->m_hovered && g_input.pressed( VK_LBUTTON ) )
        {
            this->collapse( );
            g_gui.set_listening( this );
            this->m_listening = true;
            play_sound( SOUND_CLICK );
        }

        if ( !this->m_listening && this->m_hovered && g_input.pressed( VK_RBUTTON ) )
        {
            this->m_expanded = !this->m_expanded;
            g_gui.set_open_popup( this->m_expanded ? this : nullptr );
        }

        step_mix( this->m_hover_mix, this->m_hovered, delta, WIDGET_HOVER_DURATION );
        step_mix( this->m_expand_mix, this->m_expanded, delta,
                  this->m_expanded ? COMBO_EXPAND_DURATION : COMBO_COLLAPSE_DURATION );

        this->m_listen_pulse = this->m_listening ? pulse( clock( ), 900u )
                                                 : static_cast< u8 >( 0 );

        this->m_progress = this->m_expanded ? eased_out( this->m_expand_mix )
                                            : eased_mix( this->m_expand_mix );

        const i32 full{ this->popup_height( ) };
        const i32 width{ g_gui.scaled( WIDGET_BIND_POPUP_WIDTH ) };

        this->m_revealed = ( full * static_cast< i32 >( this->m_progress ) + 127 ) / 255;

        const rect_t room{ g_gui.content_room( ) };

        this->m_upward = chip.bottom + full > room.bottom && chip.top - full >= room.top;
        this->m_popup  = this->m_upward
                             ? rect_t{ chip.right - width, chip.top - this->m_revealed,
                                       chip.right, chip.top }
                             : rect_t{ chip.right - width, chip.bottom, chip.right,
                                       chip.bottom + this->m_revealed };

        if ( this->m_revealed > 0 )
        {
            g_gui.block_hits( this->m_popup );
        }

        if ( !this->m_expanded || this->m_revealed <= 0 )
        {
            return;
        }

        const i32    row_height{ g_gui.scaled( WIDGET_LIST_ROW_HEIGHT ) };
        const rect_t full_rect{ this->m_popup.left,
                                this->m_upward ? this->m_popup.bottom - full
                                               : this->m_popup.top,
                                this->m_popup.right,
                                this->m_upward ? this->m_popup.bottom
                                               : this->m_popup.top + full };

        for ( usize index{ }; index < this->m_rows.size( ); ++index )
        {
            const i32 row_top{ full_rect.top + 1 + static_cast< i32 >( index ) * row_height };
            const i32 shown_top{ row_top > this->m_popup.top ? row_top : this->m_popup.top };
            const i32 shown_bottom{ row_top + row_height < this->m_popup.bottom
                                        ? row_top + row_height
                                        : this->m_popup.bottom };

            if ( shown_bottom <= shown_top )
            {
                continue;
            }

            const rect_t row_bounds{ full_rect.left + 1, shown_top, full_rect.right - 1,
                                     shown_bottom };
            const bool   picked{ static_cast< usize >( this->m_bind->m_mode ) == index };

            if ( !row_think( this->m_rows[ index ], picked, row_hovered( row_bounds ) ) )
            {
                continue;
            }

            this->m_bind->m_mode = static_cast< bind_mode_t >( index );

            if ( this->m_bind->m_mode != bind_mode_t::toggle )
            {
                this->m_bind->m_toggled = false;
            }

            this->collapse( );
            return;
        }

        if ( !g_input.pressed( VK_LBUTTON ) && !g_input.pressed( VK_RBUTTON ) )
        {
            return;
        }

        const point_t mouse{ g_input.mouse( ) };

        if ( !contains( this->m_popup, mouse ) && !contains( chip, mouse ) )
        {
            this->collapse( );
        }
    }

    bool keybind::mode_marked( ) const
    {
        return this->m_bind && this->m_bind->m_mode != bind_mode_t::hold;
    }

    const char *keybind::chip_label( ) const
    {
        if ( this->m_listening )
        {
            return "...";
        }

        if ( this->m_bind->m_mode == bind_mode_t::always )
        {
            return ATLAS_XORSTR_STATIC( "ON" );
        }

        return key_name( this->m_bind->m_key );
    }

    void keybind::draw( ) const
    {
        const rect_t chip{ this->chip_bounds( ) };
        const i32    chip_height{ chip.bottom - chip.top };

        const u8    hover_mix{ eased_mix( this->m_hover_mix ) };
        const char *chip_text{ this->chip_label( ) };

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

        g_render.box( chip,
                      g_gui.color( mix_color( CHIP_IDLE_BACKGROUND, CHIP_HOVER_BACKGROUND,
                                              hover_mix ) ),
                      true );

        const i32 bar{ g_gui.scaled( MENU_SELECTION_BAR ) };

        if ( this->mode_marked( ) )
        {
            g_render.box( rect_t{ chip.left, chip.top, chip.left + bar, chip.bottom },
                          g_gui.color( g_theme.m_accent ), true );
        }

        const color_t border_color{
            this->m_listening || this->m_expanded
                ? mix_color( mix_color( g_theme.m_accent, CONTROL_BORDER, 150 ), g_theme.m_accent,
                             this->m_listening ? this->m_listen_pulse
                                               : static_cast< u8 >( 255 ) )
                : mix_color( CONTROL_BORDER, CONTROL_BORDER_HOVER, hover_mix )
        };

        g_render.box( rect_t{ chip.left, chip.top, chip.right - 1, chip.bottom - 1 },
                      g_gui.color( border_color ) );

        color_t text_color{ };

        if ( this->m_listening )
        {
            text_color = mix_color( CHIP_IDLE_TEXT, g_theme.m_accent, this->m_listen_pulse );
        } else if ( this->m_bind->m_key || this->m_bind->m_mode == bind_mode_t::always )
        {
            text_color = CHIP_ACTIVE_TEXT;
        } else
        {
            text_color = mix_color( CHIP_IDLE_TEXT, CHIP_ACTIVE_TEXT, hover_mix );
        }

        const i32 text_left{ chip.left + ( this->mode_marked( ) ? bar : 0 ) };

        g_render.text( MENU_FONT_TINY( ),
                       point_t{ ( text_left + chip.right ) / 2,
                                g_gui.tiny_center_y( chip.top, chip_height ) },
                       g_gui.color( text_color ), chip_text, false,
                       render::text_align::center );
    }

    void keybind::draw_last( ) const
    {
        if ( this->m_revealed <= 0 || !this->m_bind )
        {
            return;
        }

        const i32    row_height{ g_gui.scaled( WIDGET_LIST_ROW_HEIGHT ) };
        const i32    full{ this->popup_height( ) };
        const rect_t full_rect{ this->m_popup.left,
                                this->m_upward ? this->m_popup.bottom - full
                                               : this->m_popup.top,
                                this->m_popup.right,
                                this->m_upward ? this->m_popup.bottom
                                               : this->m_popup.top + full };

        g_render.box( this->m_popup, g_gui.color( POPUP_BACKGROUND ), true );

        g_render.set_clip( this->m_popup );

        for ( usize index{ }; index < this->m_rows.size( ); ++index )
        {
            const i32    row_top{ full_rect.top + 1 + static_cast< i32 >( index ) * row_height };
            const rect_t row_bounds{ full_rect.left + 1, row_top, full_rect.right - 1,
                                     row_top + row_height };

            if ( row_bounds.bottom <= this->m_popup.top ||
                 row_bounds.top >= this->m_popup.bottom )
            {
                continue;
            }

            row_draw( row_bounds, bind_mode_name( static_cast< bind_mode_t >( index ) ),
                      this->m_rows[ index ], 255, true );
        }

        g_render.reset_clip( );

        g_render.box( rect_t{ this->m_popup.left, this->m_popup.top, this->m_popup.right - 1,
                              this->m_popup.bottom - 1 },
                      g_gui.color( CONTROL_BORDER ) );
    }

    static rect_t slider_track( rect_t bounds )
    {
        const i32 top{ bounds.top + g_gui.scaled( WIDGET_BLOCK_PADDING_TOP ) +
                       g_gui.scaled( WIDGET_LABEL_HEIGHT ) + g_gui.scaled( WIDGET_LABEL_GAP ) };

        return rect_t{ bounds.left, top, bounds.right,
                       top + g_gui.scaled( WIDGET_SLIDER_HEIGHT ) };
    }

    static i32 slider_measure( )
    {
        return g_gui.scaled( WIDGET_BLOCK_PADDING_TOP ) + g_gui.scaled( WIDGET_LABEL_HEIGHT ) +
               g_gui.scaled( WIDGET_LABEL_GAP ) + g_gui.scaled( WIDGET_SLIDER_HEIGHT ) +
               g_gui.scaled( WIDGET_BLOCK_PADDING_BOTTOM );
    }

    static i32 slider_span( rect_t track )
    {
        const i32 width{ track.right - track.left };
        return width > 2 ? width - 2 : 1;
    }

    static float slider_grab( rect_t track )
    {
        const i32 span{ slider_span( track ) };
        const i32 offset{ math::clamp( g_input.mouse( ).x - track.left - 1, 0, span ) };

        return static_cast< float >( offset ) / static_cast< float >( span );
    }

    static i32 slider_step( float value, float minimum, u8 precision )
    {
        float scale{ 1.0f };

        for ( u8 digit{ }; digit < precision && digit < 6; ++digit )
        {
            scale *= 10.0f;
        }

        return static_cast< i32 >( ( value - minimum ) * scale + 0.5f );
    }

    static bool slider_capture( baseobject *self, bool hovered )
    {
        if ( hovered && g_input.pressed( VK_LBUTTON ) )
        {
            g_gui.set_active_slider( self );
        }

        if ( g_gui.active_slider( ) != self )
        {
            return false;
        }

        if ( !g_input.down( VK_LBUTTON ) || !g_gui.can_interact( ) )
        {
            g_gui.set_active_slider( nullptr );
            return false;
        }

        return true;
    }

    static void slider_paint( rect_t bounds, string_view label, float progress,
                              string_view value_text, bool dragging, u8 hover_mix )
    {
        const rect_t track{ slider_track( bounds ) };
        const i32    span{ slider_span( track ) };
        const i32    label_top{ bounds.top + g_gui.scaled( WIDGET_BLOCK_PADDING_TOP ) };
        const i32    label_height{ g_gui.scaled( WIDGET_LABEL_HEIGHT ) };

        g_render.text( MENU_FONT( ),
                       point_t{ bounds.left, g_gui.text_center_y( label_top, label_height ) },
                       g_gui.color( CONTROL_LABEL ), label );

        g_render.text( MENU_FONT_TINY( ),
                       point_t{ bounds.right, g_gui.tiny_center_y( label_top, label_height ) },
                       g_gui.color( SLIDER_VALUE_COLOR ), value_text, false,
                       render::text_align::right );

        g_render.box( track, g_gui.color( CONTROL_BACKGROUND ), true );

        const i32 fill_width{ static_cast< i32 >(
            math::clamp( progress, 0.0f, 1.0f ) * static_cast< float >( span ) + 0.5f ) };

        if ( fill_width > 0 )
        {
            g_render.box( rect_t{ track.left + 1, track.top + 1, track.left + 1 + fill_width,
                                  track.bottom - 1 },
                          g_gui.color( dragging ? g_theme.m_slider_drag : g_theme.m_accent ),
                          true );
        }

        const color_t border_color{
            dragging ? g_theme.m_accent
                     : mix_color( SLIDER_BORDER, CONTROL_BORDER_HOVER, hover_mix )
        };

        g_render.box( rect_t{ track.left, track.top, track.right - 1, track.bottom - 1 },
                      g_gui.color( border_color ) );
    }

    i32 slider_int::measure( ) const
    {
        return slider_measure( );
    }

    void slider_int::think( )
    {
        if ( !this->m_value || this->m_maximum <= this->m_minimum )
        {
            return;
        }

        const rect_t track{ slider_track( this->m_bounds ) };

        this->m_hovered = g_gui.hit_test( track );
        if ( this->m_hovered )
        {
            set_cursor( );
        }

        if ( slider_capture( this, this->m_hovered ) )
        {
            const i32 span{ slider_span( track ) };
            const i32 offset{ math::clamp( g_input.mouse( ).x - track.left - 1, 0, span ) };
            const i32 previous{ *this->m_value };

            *this->m_value = this->m_minimum +
                             ( offset * ( this->m_maximum - this->m_minimum ) + span / 2 ) /
                                 span;

            if ( *this->m_value != previous )
            {
                play_sound( SOUND_SLIDER );
            }
        }

        *this->m_value   = math::clamp( *this->m_value, this->m_minimum, this->m_maximum );
        this->m_dragging = g_gui.active_slider( ) == this;

        step_mix( this->m_hover_mix, this->m_hovered || this->m_dragging,
                  animation_delta( this->m_last_tick ), WIDGET_HOVER_DURATION );
    }

    void slider_int::draw( ) const
    {
        char value_text[ 32 ]{ };
        str::format_to( value_text, sizeof( value_text ), "{}", *this->m_value );

        if ( !this->m_suffix.empty( ) )
        {
            str::append( value_text, sizeof( value_text ), this->m_suffix.data( ),
                         this->m_suffix.size( ) );
        }

        const float progress{ static_cast< float >( *this->m_value - this->m_minimum ) /
                              static_cast< float >( this->m_maximum - this->m_minimum ) };

        slider_paint( this->m_bounds, this->m_name, progress, value_text, this->m_dragging,
                      eased_mix( this->m_hover_mix ) );
    }

    i32 slider_float::measure( ) const
    {
        return slider_measure( );
    }

    void slider_float::think( )
    {
        if ( !this->m_value || this->m_maximum <= this->m_minimum )
        {
            return;
        }

        const rect_t track{ slider_track( this->m_bounds ) };

        this->m_hovered = g_gui.hit_test( track );
        if ( this->m_hovered )
        {
            set_cursor( );
        }

        if ( slider_capture( this, this->m_hovered ) )
        {
            const i32 previous{ slider_step( *this->m_value, this->m_minimum, this->m_precision ) };

            *this->m_value = this->m_minimum +
                             slider_grab( track ) * ( this->m_maximum - this->m_minimum );

            if ( slider_step( *this->m_value, this->m_minimum, this->m_precision ) != previous )
            {
                play_sound( SOUND_SLIDER );
            }
        }

        *this->m_value   = math::clamp( *this->m_value, this->m_minimum, this->m_maximum );
        this->m_dragging = g_gui.active_slider( ) == this;

        step_mix( this->m_hover_mix, this->m_hovered || this->m_dragging,
                  animation_delta( this->m_last_tick ), WIDGET_HOVER_DURATION );
    }

    void slider_float::draw( ) const
    {
        char spec[ 8 ]{ "{:.1f}" };
        spec[ 3 ] = static_cast< char >(
            static_cast< i32 >( 48 ) + ( this->m_precision > 6 ? 6 : this->m_precision ) );

        char value_text[ 32 ]{ };
        str::format_to( value_text, sizeof( value_text ), spec, *this->m_value );

        if ( !this->m_suffix.empty( ) )
        {
            str::append( value_text, sizeof( value_text ), this->m_suffix.data( ),
                         this->m_suffix.size( ) );
        }

        const float progress{ ( *this->m_value - this->m_minimum ) /
                              ( this->m_maximum - this->m_minimum ) };

        slider_paint( this->m_bounds, this->m_name, progress, value_text, this->m_dragging,
                      eased_mix( this->m_hover_mix ) );
    }

    i32 label::measure( ) const
    {
        return g_gui.scaled( WIDGET_ROW_HEIGHT ) * static_cast< i32 >( this->m_lines );
    }

    void label::think( )
    {
    }

    void label::draw( ) const
    {
        const char *text{ this->m_text ? this->m_text( ) : nullptr };

        if ( !text || !text[ 0 ] )
        {
            return;
        }

        const i32 text_y{ g_gui.text_center_y( this->m_bounds.top, g_gui.scaled( WIDGET_ROW_HEIGHT ) ) };

        if ( this->m_pair )
        {
            g_render.text( MENU_FONT( ), point_t{ this->m_bounds.left, text_y }, g_gui.color( CONTROL_LABEL ), this->m_name );
            g_render.text( MENU_FONT( ), point_t{ this->m_bounds.right, text_y }, g_gui.color( CONTROL_VALUE ), text, false,
                           render::text_align::right );
            return;
        }

        g_render.text( MENU_FONT( ), point_t{ this->m_bounds.left, text_y }, g_gui.color( CONTROL_LABEL ), text );
    }

    i32 button::measure( ) const
    {
        return g_gui.scaled( WIDGET_BUTTON_HEIGHT );
    }

    // @ only matters when the button rides a row as a module; a full-width button ignores it
    i32 button::measure_width( ) const
    {
        const i32 fitted{ estimate_text_width( this->m_name.view( ) ) +
                          g_gui.scaled( INPUT_PADDING_X ) * 2 };
        const i32 minimum{ g_gui.scaled( WIDGET_BUTTON_WIDTH ) };

        return fitted > minimum ? fitted : minimum;
    }

    void button::think( )
    {
        this->m_hovered = g_gui.hit_test( this->m_bounds );
        this->m_pressed = this->m_hovered && g_input.down( VK_LBUTTON );

        if ( this->m_hovered )
        {
            set_cursor( );
        }

        if ( this->m_hovered && g_input.pressed( VK_LBUTTON ) && this->m_callback )
        {
            play_sound( SOUND_CLICK );
            this->m_callback( );
        }

        const u32 delta{ animation_delta( this->m_last_tick ) };
        step_mix( this->m_hover_mix, this->m_hovered, delta, WIDGET_HOVER_DURATION );
    }

    void button::draw( ) const
    {
        const i32 width{ this->m_bounds.right - this->m_bounds.left };
        const i32 height{ this->m_bounds.bottom - this->m_bounds.top };
        const u8  hover_mix{ eased_mix( this->m_hover_mix ) };

        color_t background{ mix_color( BUTTON_BACKGROUND, BUTTON_BACKGROUND_HOVER, hover_mix ) };
        color_t border{ mix_color( BUTTON_BORDER, CONTROL_BORDER_HOVER, hover_mix ) };
        color_t text_color{ mix_color( BUTTON_TEXT, BUTTON_TEXT_HOVER, hover_mix ) };

        if ( this->m_pressed )
        {
            background = BUTTON_BACKGROUND_PRESS;
            border     = g_theme.m_accent;
            text_color = BUTTON_TEXT_PRESS;
        }

        g_render.box( this->m_bounds, g_gui.color( background ), true );
        g_render.box( rect_t{ this->m_bounds.left, this->m_bounds.top, this->m_bounds.right - 1,
                              this->m_bounds.bottom - 1 },
                      g_gui.color( border ) );

        g_render.text( MENU_FONT( ),
                       point_t{ this->m_bounds.left + width / 2,
                                g_gui.text_center_y( this->m_bounds.top, height ) +
                                    ( this->m_pressed ? 1 : 0 ) },
                       g_gui.color( text_color ), this->m_name,
                       false, render::text_align::center );
    }

    void text_buffer_t::erase( usize index )
    {
        if ( index >= this->m_length )
        {
            return;
        }

        for ( usize cursor{ index }; cursor + 1 < this->m_length; ++cursor )
        {
            this->m_data[ cursor ] = this->m_data[ cursor + 1 ];
        }

        --this->m_length;
        this->m_data[ this->m_length ] = 0;
    }

    bool text_buffer_t::insert( usize index, char value )
    {
        if ( this->m_length >= this->m_limit || index > this->m_length )
        {
            return false;
        }

        for ( usize cursor{ this->m_length }; cursor > index; --cursor )
        {
            this->m_data[ cursor ] = this->m_data[ cursor - 1 ];
        }

        this->m_data[ index ] = value;
        ++this->m_length;
        this->m_data[ this->m_length ] = 0;
        return true;
    }

    edit_result_t edit_text( text_buffer_t &buffer, char ( *filter )( char ) )
    {
        bool moved{ };

        for ( usize index{ }; index < g_input.typed_count( ); ++index )
        {
            const char value{ g_input.typed( index ) };

            // @ enter commits, escape gives up. either way the field stops owning the
            // keyboard, and nothing later in the burst still belongs to it
            if ( value == '\r' || value == '\n' || value == 0x1B )
            {
                return edit_result_t::released;
            }

            if ( value == '\b' )
            {
                if ( buffer.m_caret )
                {
                    buffer.erase( buffer.m_caret - 1 );
                    --buffer.m_caret;
                    moved = true;
                }

                continue;
            }

            if ( value == 0x7F )
            {
                while ( buffer.m_caret && buffer.m_data[ buffer.m_caret - 1 ] == ' ' )
                {
                    buffer.erase( --buffer.m_caret );
                    moved = true;
                }

                while ( buffer.m_caret && buffer.m_data[ buffer.m_caret - 1 ] != ' ' )
                {
                    buffer.erase( --buffer.m_caret );
                    moved = true;
                }

                continue;
            }

            // everything else below space is a control code we have no use for
            if ( static_cast< u8 >( value ) < 0x20 || static_cast< u8 >( value ) > 0x7E )
            {
                continue;
            }

            const char kept{ filter ? filter( value ) : value };

            if ( !kept )
            {
                continue;
            }

            if ( buffer.insert( buffer.m_caret, kept ) )
            {
                ++buffer.m_caret;
                moved = true;
            }
        }

        // caret keys produce no character, so they come off the key state instead
        if ( g_input.pressed( VK_LEFT ) && buffer.m_caret )
        {
            --buffer.m_caret;
            moved = true;
        }

        if ( g_input.pressed( VK_RIGHT ) && buffer.m_caret < buffer.m_length )
        {
            ++buffer.m_caret;
            moved = true;
        }

        if ( g_input.pressed( VK_HOME ) && buffer.m_caret )
        {
            buffer.m_caret = 0;
            moved          = true;
        }

        if ( g_input.pressed( VK_END ) && buffer.m_caret < buffer.m_length )
        {
            buffer.m_caret = buffer.m_length;
            moved          = true;
        }

        if ( g_input.pressed( VK_DELETE ) && buffer.m_caret < buffer.m_length )
        {
            buffer.erase( buffer.m_caret );
            moved = true;
        }

        return moved ? edit_result_t::moved : edit_result_t::idle;
    }

    void combo_box::setup( )
    {
        this->m_rows.resize( this->m_item_count );
    }

    rect_t combo_box::well_bounds( ) const
    {
        const i32 top{ this->m_bounds.top + g_gui.scaled( WIDGET_BLOCK_PADDING_TOP ) +
                       g_gui.scaled( WIDGET_LABEL_HEIGHT ) + g_gui.scaled( WIDGET_LABEL_GAP ) };

        return rect_t{ this->m_bounds.left, top, this->m_bounds.right,
                       top + g_gui.scaled( WIDGET_WELL_HEIGHT ) };
    }

    usize combo_box::visible_rows( ) const
    {
        return this->m_item_count > COMBO_POPUP_MAX_ROWS ? COMBO_POPUP_MAX_ROWS
                                                         : this->m_item_count;
    }

    i32 combo_box::full_popup_height( ) const
    {
        return static_cast< i32 >( this->visible_rows( ) ) *
                   g_gui.scaled( WIDGET_LIST_ROW_HEIGHT ) +
               1;
    }

    i32 combo_box::popup_inset( ) const
    {
        return this->m_upward ? 1 : 0;
    }

    i32 combo_box::measure( ) const
    {
        return g_gui.scaled( WIDGET_BLOCK_PADDING_TOP ) + g_gui.scaled( WIDGET_LABEL_HEIGHT ) +
               g_gui.scaled( WIDGET_LABEL_GAP ) + g_gui.scaled( WIDGET_WELL_HEIGHT ) +
               g_gui.scaled( WIDGET_BLOCK_PADDING_BOTTOM );
    }

    void combo_box::collapse( )
    {
        this->m_expanded = false;

        // the hit block stays: clearing it here hands the very click that picked a row to
        // whatever widget sits under the popup. It keeps renewing while the popup shrinks
        // and expires on its own once it stops.
        if ( g_gui.open_popup( ) == this )
        {
            g_gui.set_open_popup( nullptr );
        }
    }

    void combo_box::think( )
    {
        if ( !this->m_items || !this->m_item_count )
        {
            return;
        }

        if ( this->m_selected && *this->m_selected >= this->m_item_count )
        {
            *this->m_selected = 0;
        }

        // another combo took the popup slot out from under us
        if ( this->m_expanded && g_gui.open_popup( ) != this )
        {
            this->m_expanded = false;
        }

        const rect_t well{ this->well_bounds( ) };

        this->m_hovered = g_gui.hit_test( well );
        if ( this->m_hovered )
        {
            set_cursor( );
        }

        if ( this->m_hovered && g_input.pressed( VK_LBUTTON ) )
        {
            this->m_expanded = !this->m_expanded;
            g_gui.set_open_popup( this->m_expanded ? this : nullptr );
        }

        const u32 delta{ animation_delta( this->m_last_tick ) };
        step_mix( this->m_hover_mix, this->m_hovered, delta, WIDGET_HOVER_DURATION );
        step_mix( this->m_expand_mix, this->m_expanded, delta,
                  this->m_expanded ? COMBO_EXPAND_DURATION : COMBO_COLLAPSE_DURATION );

        this->m_progress = this->m_expanded ? eased_out( this->m_expand_mix )
                                            : eased_mix( this->m_expand_mix );

        const i32 full{ this->full_popup_height( ) };

        this->m_revealed = ( full * static_cast< i32 >( this->m_progress ) + 127 ) / 255;

        const rect_t room{ g_gui.content_room( ) };

        this->m_upward = well.bottom + full > room.bottom && well.top - full >= room.top;
        this->m_popup  = this->m_upward
                             ? rect_t{ well.left, well.top - this->m_revealed, well.right,
                                       well.top }
                             : rect_t{ well.left, well.bottom, well.right,
                                       well.bottom + this->m_revealed };

        if ( this->m_revealed > 0 )
        {
            g_gui.block_hits( this->m_popup );
        }

        if ( !this->m_expanded || this->m_revealed <= 0 )
        {
            return;
        }

        const i32    row_height{ g_gui.scaled( WIDGET_LIST_ROW_HEIGHT ) };
        const usize  rows{ this->visible_rows( ) };
        const i32    inset{ this->popup_inset( ) };
        const rect_t full_rect{ this->m_popup.left,
                                this->m_upward ? this->m_popup.bottom - full
                                               : this->m_popup.top,
                                this->m_popup.right,
                                this->m_upward ? this->m_popup.bottom
                                               : this->m_popup.top + full };

        for ( usize index{ }; index < rows && index < this->m_rows.size( ); ++index )
        {
            const i32 row_top{ full_rect.top + inset +
                               static_cast< i32 >( index ) * row_height };
            const i32 shown_top{ row_top > this->m_popup.top ? row_top : this->m_popup.top };
            const i32 shown_bottom{ row_top + row_height < this->m_popup.bottom
                                        ? row_top + row_height
                                        : this->m_popup.bottom };

            if ( shown_bottom <= shown_top )
            {
                continue;
            }

            const rect_t row_bounds{ full_rect.left + 1, shown_top, full_rect.right - 1,
                                     shown_bottom };

            const bool picked{ this->m_selected ? *this->m_selected == index
                                                : this->m_selections[ index ] };

            if ( !row_think( this->m_rows[ index ], picked, row_hovered( row_bounds ) ) )
            {
                continue;
            }

            if ( !this->m_selected )
            {
                this->m_selections[ index ] = !this->m_selections[ index ];
                continue;
            }

            *this->m_selected = index;
            this->collapse( );
            return;
        }

        // a click that is neither on the list nor on the well that owns it closes
        if ( !g_input.pressed( VK_LBUTTON ) )
        {
            return;
        }

        const point_t mouse{ g_input.mouse( ) };

        if ( !contains( this->m_popup, mouse ) && !contains( well, mouse ) )
        {
            this->collapse( );
        }
    }

    void combo_box::build_summary( char ( &buffer )[ 96 ], char ( &count_text )[ 32 ],
                                   const char *&text, usize &selected_count ) const
    {
        selected_count = 0;
        buffer[ 0 ]    = 0;

        if ( this->m_selected )
        {
            text = this->m_items[ *this->m_selected ];
            return;
        }

        const rect_t well{ this->well_bounds( ) };
        const i32    room{ well.right - g_gui.scaled( MENU_ICON_SIZE + WIDGET_WELL_PADDING ) -
                           ( well.left + g_gui.scaled( WIDGET_WELL_PADDING ) ) };

        for ( usize index{ }; index < this->m_item_count; ++index )
        {
            if ( !this->m_selections[ index ] )
            {
                continue;
            }

            if ( selected_count )
            {
                str::append( buffer, sizeof( buffer ), ", " );
            }

            str::append( buffer, sizeof( buffer ), this->m_items[ index ] );
            ++selected_count;
        }

        text = buffer;

        if ( !selected_count )
        {
            text = "None";
        } else if ( estimate_text_width( buffer ) > room )
        {
            str::format_to( count_text, sizeof( count_text ), "{} selected", selected_count );
            text = count_text;
        }
    }

    void combo_box::draw( ) const
    {
        if ( !this->m_items || !this->m_item_count )
        {
            return;
        }

        const rect_t well{ this->well_bounds( ) };
        const i32    well_height{ well.bottom - well.top };
        const u8     hover_mix{ eased_mix( this->m_hover_mix ) };
        const i32    label_top{ this->m_bounds.top + g_gui.scaled( WIDGET_BLOCK_PADDING_TOP ) };

        g_render.text( MENU_FONT( ),
                       point_t{ this->m_bounds.left,
                                g_gui.text_center_y( label_top,
                                                     g_gui.scaled( WIDGET_LABEL_HEIGHT ) ) },
                       g_gui.color( CONTROL_LABEL ), this->m_name );

        g_render.box( well,
                      g_gui.color( mix_color( CONTROL_BACKGROUND, CONTROL_BACKGROUND_HOVER,
                                              hover_mix ) ),
                      true );

        const color_t border{ this->m_expanded ? g_theme.m_accent
                                               : mix_color( CONTROL_BORDER, CONTROL_BORDER_HOVER,
                                                            hover_mix ) };

        g_render.box( rect_t{ well.left, well.top, well.right - 1, well.bottom - 1 },
                      g_gui.color( border ) );

        char        summary[ 96 ]{ };
        char        count_text[ 32 ]{ };
        const char *summary_text{ };
        usize       selected_count{ };

        this->build_summary( summary, count_text, summary_text, selected_count );

        g_render.text( MENU_FONT( ),
                       point_t{ well.left + g_gui.scaled( WIDGET_WELL_PADDING ),
                                g_gui.text_center_y( well.top, well_height ) },
                       g_gui.color( this->m_selected || selected_count ? CONTROL_VALUE
                                                                       : MENU_TEXT_DIM ),
                       summary_text );

        const point_t chevron{ well.right - g_gui.scaled( WIDGET_WELL_PADDING +
                                                          MENU_ICON_SIZE / 2 ),
                               well.top + well_height / 2 };
        const color_t chevron_color{ mix_color( MENU_TEXT_DIM, g_theme.m_accent,
                                                this->m_progress ) };

        if ( this->m_progress < 255 )
        {
            draw_icon_glyph( ICON_CHEVRON_DOWN, chevron,
                             with_alpha( chevron_color,
                                         static_cast< u8 >( 255 - this->m_progress ) ) );
        }

        if ( this->m_progress )
        {
            draw_icon_glyph( ICON_CHEVRON_UP, chevron,
                             with_alpha( chevron_color, this->m_progress ) );
        }
    }

    void combo_box::draw_last( ) const
    {
        if ( this->m_revealed <= 0 || !this->m_items )
        {
            return;
        }

        const i32    row_height{ g_gui.scaled( WIDGET_LIST_ROW_HEIGHT ) };
        const i32    full{ this->full_popup_height( ) };
        const usize  rows{ this->visible_rows( ) };
        const i32    inset{ this->popup_inset( ) };
        const rect_t full_rect{ this->m_popup.left,
                                this->m_upward ? this->m_popup.bottom - full
                                               : this->m_popup.top,
                                this->m_popup.right,
                                this->m_upward ? this->m_popup.bottom
                                               : this->m_popup.top + full };

        g_render.box( this->m_popup, g_gui.color( POPUP_BACKGROUND ), true );

        g_render.set_clip( this->m_popup );

        for ( usize index{ }; index < rows && index < this->m_rows.size( ); ++index )
        {
            const i32    row_top{ full_rect.top + inset +
                                  static_cast< i32 >( index ) * row_height };
            const rect_t row_bounds{ full_rect.left + 1, row_top, full_rect.right - 1,
                                     row_top + row_height };

            if ( row_bounds.bottom <= this->m_popup.top ||
                 row_bounds.top >= this->m_popup.bottom )
            {
                continue;
            }

            row_draw( row_bounds, this->m_items[ index ], this->m_rows[ index ], 255, true );
        }

        g_render.reset_clip( );

        const color_t frame{ g_gui.color( CONTROL_BORDER ) };

        g_render.line( point_t{ this->m_popup.left, this->m_popup.top },
                       point_t{ this->m_popup.left, this->m_popup.bottom - 1 }, frame );
        g_render.line( point_t{ this->m_popup.right - 1, this->m_popup.top },
                       point_t{ this->m_popup.right - 1, this->m_popup.bottom - 1 }, frame );

        if ( this->m_upward )
        {
            g_render.line( point_t{ this->m_popup.left, this->m_popup.top },
                           point_t{ this->m_popup.right - 1, this->m_popup.top }, frame );
        } else
        {
            g_render.line( point_t{ this->m_popup.left, this->m_popup.bottom - 1 },
                           point_t{ this->m_popup.right - 1, this->m_popup.bottom - 1 }, frame );
        }
    }

    searchable_list::searchable_list( ) :
        m_search{ ATLAS_XORSTR_STATIC( "Search" ), ICON_SEARCH }
    {
    }

    i32 searchable_list::search_height( ) const
    {
        return this->m_search.measure( ) + g_gui.scaled( CARD_BLOCK_GAP );
    }

    bool searchable_list::think_search( )
    {
        this->m_search.set_bounds( rect_t{ this->m_bounds.left, this->m_bounds.top,
                                           this->m_bounds.right,
                                           this->m_bounds.top + this->m_search.measure( ) } );
        this->m_search.think( );

        if ( this->m_search.value( ) == this->m_filter.view( ) )
        {
            return false;
        }

        this->m_filter.assign( this->m_search.value( ) );
        this->m_parent->reset_scroll( );
        g_gui.invalidate_layout( );
        return true;
    }

    bool list_box::matches( usize index ) const
    {
        return label_matches( string_view{ this->m_items[ index ] }, this->m_filter.view( ) );
    }

    usize list_box::visible_count( ) const
    {
        usize count{ };
        for ( usize index{ }; this->m_items && index < this->m_item_count; ++index )
        {
            count += this->matches( index );
        }
        return count;
    }

    void list_box::setup( )
    {
        this->m_rows.resize( this->m_item_count );
    }

    void list_box::set_items( const char *const *items, usize count )
    {
        if ( count != this->m_item_count )
        {
            g_gui.invalidate_layout( );
        }

        this->m_items      = items;
        this->m_item_count = count;
        this->m_rows.resize( count );

        if ( *this->m_selected >= count )
        {
            *this->m_selected = 0;
        }
    }

    i32 list_box::box_top( ) const
    {
        return this->m_bounds.top + this->search_height( ) + g_gui.scaled( WIDGET_LIST_MARGIN_TOP );
    }

    i32 list_box::measure( ) const
    {
        return this->search_height( ) + g_gui.scaled( WIDGET_LIST_MARGIN_TOP ) +
               static_cast< i32 >( this->visible_count( ) ) *
                   g_gui.scaled( WIDGET_LIST_ROW_HEIGHT ) +
               2 + g_gui.scaled( WIDGET_LIST_MARGIN_BOTTOM );
    }

    void list_box::think( )
    {
        if ( this->think_search( ) )
        {
            return;
        }

        if ( !this->m_items || !this->m_item_count || !this->m_selected )
        {
            return;
        }

        if ( *this->m_selected >= this->m_item_count )
        {
            *this->m_selected = 0;
        }

        const i32 row_height{ g_gui.scaled( WIDGET_LIST_ROW_HEIGHT ) };

        usize visible{ };
        for ( usize index{ }; index < this->m_item_count && index < this->m_rows.size( );
              ++index )
        {
            if ( !this->matches( index ) )
            {
                continue;
            }

            const i32    row_top{ this->box_top( ) + 1 +
                                  static_cast< i32 >( visible++ ) * row_height };
            const rect_t row_bounds{ this->m_bounds.left + 1, row_top, this->m_bounds.right - 1,
                                     row_top + row_height };

            if ( row_think( this->m_rows[ index ], index == *this->m_selected,
                            g_gui.hit_test( row_bounds ) ) )
            {
                *this->m_selected = index;
            }
        }
    }

    void list_box::draw( ) const
    {
        this->m_search.draw( );

        if ( !this->m_items || !this->m_item_count )
        {
            return;
        }

        const i32    row_height{ g_gui.scaled( WIDGET_LIST_ROW_HEIGHT ) };
        const rect_t box{ this->m_bounds.left, this->box_top( ), this->m_bounds.right,
                          this->box_top( ) + static_cast< i32 >( this->visible_count( ) ) * row_height +
                              2 };

        g_render.box( box, g_gui.color( CONTROL_BACKGROUND ), true );

        usize visible{ };
        for ( usize index{ }; index < this->m_item_count && index < this->m_rows.size( );
              ++index )
        {
            if ( !this->matches( index ) )
            {
                continue;
            }

            const i32    row_top{ box.top + 1 + static_cast< i32 >( visible++ ) * row_height };
            const rect_t row_bounds{ box.left + 1, row_top, box.right - 1,
                                     row_top + row_height };

            row_draw( row_bounds, this->m_items[ index ], this->m_rows[ index ], 255, false );
        }

        g_render.box( rect_t{ box.left, box.top, box.right - 1, box.bottom - 1 },
                      g_gui.color( CONTROL_BORDER ) );
    }

    bool dynamic_list_box::matches( usize index ) const
    {
        return label_matches( this->m_elements[ index ].m_name.view( ), this->m_filter.view( ) );
    }

    usize dynamic_list_box::visible_count( ) const
    {
        usize count{ };
        for ( usize index{ }; index < this->m_elements.size( ); ++index )
        {
            count += this->matches( index );
        }
        return count;
    }

    i32 dynamic_list_box::measure( ) const
    {
        return this->search_height( ) + g_gui.scaled( WIDGET_LIST_MARGIN_TOP ) +
               static_cast< i32 >( this->visible_count( ) ) *
                   g_gui.scaled( WIDGET_LIST_ROW_HEIGHT ) +
               2 + g_gui.scaled( WIDGET_LIST_MARGIN_BOTTOM );
    }

    void dynamic_list_box::think( )
    {
        if ( this->think_search( ) )
        {
            return;
        }

        if ( this->m_item_count == 0 )
        {
            return;
        }

        if ( *this->m_selected >= this->m_item_count )
        {
            *this->m_selected = 0;
        }

        const i32 row_height{ g_gui.scaled( WIDGET_LIST_ROW_HEIGHT ) };

        usize visible{ };
        for ( usize index{ }; index < this->m_item_count && index < this->m_elements.size( );
              ++index )
        {
            if ( !this->matches( index ) )
            {
                continue;
            }

            const i32    row_top{ this->box_top( ) + 1 +
                                  static_cast< i32 >( visible++ ) * row_height };
            const rect_t row_bounds{ this->m_bounds.left + 1, row_top, this->m_bounds.right - 1,
                                     row_top + row_height };
            auto        &row_state{ this->m_elements[ index ].m_row_state };

            if ( row_think( row_state, index == *this->m_selected,
                            g_gui.hit_test( row_bounds ) ) )
            {
                *this->m_selected = index;
            }
        }
    }

    void dynamic_list_box::draw( ) const
    {
        this->m_search.draw( );

        if ( this->m_item_count == 0 )
        {
            return;
        }

        const i32    row_height{ g_gui.scaled( WIDGET_LIST_ROW_HEIGHT ) };
        const rect_t box{ this->m_bounds.left, this->box_top( ), this->m_bounds.right,
                          this->box_top( ) + static_cast< i32 >( this->visible_count( ) ) * row_height +
                              2 };

        g_render.box( box, g_gui.color( CONTROL_BACKGROUND ), true );

        usize visible{ };
        for ( usize index{ }; index < this->m_item_count && index < this->m_elements.size( );
              ++index )
        {
            if ( !this->matches( index ) )
            {
                continue;
            }

            const i32    row_top{ box.top + 1 + static_cast< i32 >( visible++ ) * row_height };
            const rect_t row_bounds{ box.left + 1, row_top, box.right - 1,
                                     row_top + row_height };
            auto        &element{ this->m_elements[ index ] };

            row_draw( row_bounds, element.m_name, element.m_row_state, 255, false );
        }

        g_render.box( rect_t{ box.left, box.top, box.right - 1, box.bottom - 1 },
                      g_gui.color( CONTROL_BORDER ) );
    }

    bool dynamic_list_box::insert_element( const char *element_name )
    {
        if ( !element_name )
        {
            return false;
        }

        auto element{ this->m_elements.emplace_back( ) };

        if ( !element )
        {
            return false;
        }

        if ( !element->m_name.assign( string_view{ element_name } ) )
        {
            this->m_elements.pop_back( );
            return false;
        }

        ++this->m_item_count;

        // re-measure so we dont paint out of bounds
        g_gui.invalidate_layout( );

        return true;
    }

    i32 dynamic_list_box::box_top( ) const
    {
        return this->m_bounds.top + this->search_height( ) + g_gui.scaled( WIDGET_LIST_MARGIN_TOP );
    }

    rect_t segment_bar::strip_bounds( ) const
    {
        const i32 top{ this->m_bounds.top + g_gui.scaled( WIDGET_BLOCK_PADDING_TOP ) };

        return rect_t{ this->m_bounds.left, top, this->m_bounds.right,
                       top + g_gui.scaled( WIDGET_SEGMENT_HEIGHT ) };
    }

    rect_t segment_bar::segment_bounds( usize index ) const
    {
        const rect_t strip{ this->strip_bounds( ) };
        const i32    span{ strip.right - strip.left };
        const i32    count{ static_cast< i32 >( this->m_count ) };

        return rect_t{ strip.left + static_cast< i32 >( index ) * span / count,
                       strip.top,
                       strip.left + static_cast< i32 >( index + 1 ) * span / count,
                       strip.bottom };
    }

    void segment_bar::setup( )
    {
        this->m_segments.resize( this->m_count );
    }

    i32 segment_bar::measure( ) const
    {
        return g_gui.scaled( WIDGET_BLOCK_PADDING_TOP ) + g_gui.scaled( WIDGET_SEGMENT_HEIGHT ) +
               g_gui.scaled( WIDGET_SEGMENT_GAP );
    }

    void segment_bar::think( )
    {
        if ( !this->m_value || !this->m_count )
        {
            return;
        }

        for ( usize index{ }; index < this->m_count && index < this->m_segments.size( ); ++index )
        {
            if ( row_think( this->m_segments[ index ], index == *this->m_value,
                            g_gui.hit_test( this->segment_bounds( index ) ) ) )
            {
                *this->m_value = index;
            }
        }
    }

    void segment_bar::draw( ) const
    {
        if ( !this->m_count )
        {
            return;
        }

        const i32 bar{ g_gui.scaled( MENU_SELECTION_BAR ) };

        for ( usize index{ }; index < this->m_count && index < this->m_segments.size( ); ++index )
        {
            const rect_t segment{ this->segment_bounds( index ) };

            const u8 active_mix{ eased_mix( this->m_segments[ index ].m_active_mix ) };
            const u8 hover_mix{ eased_mix( this->m_segments[ index ].m_hover_mix ) };

            color_t background{ mix_color( CHIP_IDLE_BACKGROUND, CHIP_HOVER_BACKGROUND,
                                           hover_mix ) };
            background = mix_color( background, g_theme.m_row_selected_background, active_mix );

            g_render.box( segment, g_gui.color( background ), true );

            color_t label_color{ mix_color( CHIP_IDLE_TEXT, CHIP_ACTIVE_TEXT, hover_mix ) };
            label_color = mix_color( label_color, LIST_TEXT_ACTIVE, active_mix );

            g_render.text( MENU_FONT_TINY( ),
                           point_t{ ( segment.left + segment.right ) / 2,
                                    g_gui.tiny_center_y( segment.top,
                                                         segment.bottom - segment.top ) },
                           g_gui.color( label_color ), this->m_items[ index ], false,
                           render::text_align::center );

            if ( active_mix )
            {
                g_render.box( rect_t{ segment.left, segment.bottom - bar, segment.right,
                                      segment.bottom },
                              g_gui.color( with_alpha( g_theme.m_accent, active_mix ) ), true );
            }

            const bool last{ index + 1 == this->m_count };

            g_render.box( rect_t{ segment.left, segment.top,
                                  last ? segment.right - 1 : segment.right,
                                  segment.bottom - 1 },
                          g_gui.color( CONTROL_BORDER ) );
        }
    }

    static const char *group_label( const target_picker::group_t &group )
    {
        return group.m_label ? group.m_label : "";
    }

    const target_picker::group_t *target_picker::current( ) const
    {
        if ( !this->m_groups || !this->m_group_count )
        {
            return nullptr;
        }

        const usize index{ this->m_group && *this->m_group < this->m_group_count
                               ? *this->m_group
                               : 0 };

        return &this->m_groups[ index ];
    }

    rect_t target_picker::strip_bounds( ) const
    {
        const i32 top{ this->m_bounds.top + g_gui.scaled( WIDGET_BLOCK_PADDING_TOP ) };

        return rect_t{ this->m_bounds.left, top, this->m_bounds.right,
                       top + g_gui.scaled( WIDGET_SEGMENT_HEIGHT ) };
    }

    rect_t target_picker::list_bounds( ) const
    {
        const group_t *group{ this->current( ) };
        const i32      rows{ group ? static_cast< i32 >( group->m_item_count ) : 0 };
        const i32      top{ this->strip_bounds( ).bottom + g_gui.scaled( WIDGET_SEGMENT_GAP ) };

        return rect_t{ this->m_bounds.left, top, this->m_bounds.right,
                       top + rows * g_gui.scaled( WIDGET_LIST_ROW_HEIGHT ) + 2 };
    }

    void target_picker::setup( )
    {
        this->m_segments.resize( this->m_group_count );

        usize widest{ };

        for ( usize index{ }; index < this->m_group_count; ++index )
        {
            if ( this->m_groups[ index ].m_item_count > widest )
            {
                widest = this->m_groups[ index ].m_item_count;
            }
        }

        this->m_rows.resize( widest );
    }

    i32 target_picker::measure( ) const
    {
        return this->list_bounds( ).bottom - this->m_bounds.top +
               g_gui.scaled( WIDGET_SEGMENT_GAP );
    }

    void target_picker::think( )
    {
        const group_t *group{ this->current( ) };

        if ( !group || !this->m_group || !this->m_item )
        {
            return;
        }

        const rect_t strip{ this->strip_bounds( ) };
        const i32    span{ strip.right - strip.left };

        for ( usize index{ }; index < this->m_group_count && index < this->m_segments.size( );
              ++index )
        {
            const i32    left{ strip.left + static_cast< i32 >( index ) * span /
                                                static_cast< i32 >( this->m_group_count ) };
            const i32    right{ strip.left + static_cast< i32 >( index + 1 ) * span /
                                                 static_cast< i32 >( this->m_group_count ) };
            const rect_t segment{ left, strip.top, right, strip.bottom };

            if ( row_think( this->m_segments[ index ], index == *this->m_group,
                            g_gui.hit_test( segment ) ) &&
                 index != *this->m_group )
            {
                *this->m_group = index;
                *this->m_item  = 0;
                g_gui.invalidate_layout( );
                return;
            }
        }

        if ( *this->m_item >= group->m_item_count )
        {
            *this->m_item = 0;
        }

        const rect_t list{ this->list_bounds( ) };
        const i32    row_height{ g_gui.scaled( WIDGET_LIST_ROW_HEIGHT ) };

        for ( usize index{ }; index < group->m_item_count && index < this->m_rows.size( );
              ++index )
        {
            const i32    row_top{ list.top + 1 + static_cast< i32 >( index ) * row_height };
            const rect_t row{ list.left + 1, row_top, list.right - 1, row_top + row_height };

            if ( row_think( this->m_rows[ index ], index == *this->m_item,
                            g_gui.hit_test( row ) ) )
            {
                *this->m_item = index;
            }
        }
    }

    void target_picker::draw( ) const
    {
        const group_t *group{ this->current( ) };

        if ( !group )
        {
            return;
        }

        const rect_t strip{ this->strip_bounds( ) };
        const i32    span{ strip.right - strip.left };
        const i32    bar{ g_gui.scaled( MENU_SELECTION_BAR ) };

        for ( usize index{ }; index < this->m_group_count && index < this->m_segments.size( );
              ++index )
        {
            const i32    left{ strip.left + static_cast< i32 >( index ) * span /
                                                static_cast< i32 >( this->m_group_count ) };
            const i32    right{ strip.left + static_cast< i32 >( index + 1 ) * span /
                                                 static_cast< i32 >( this->m_group_count ) };
            const rect_t segment{ left, strip.top, right, strip.bottom };

            const u8 active_mix{ eased_mix( this->m_segments[ index ].m_active_mix ) };
            const u8 hover_mix{ eased_mix( this->m_segments[ index ].m_hover_mix ) };

            color_t background{ mix_color( CHIP_IDLE_BACKGROUND, CHIP_HOVER_BACKGROUND,
                                           hover_mix ) };
            background = mix_color( background, g_theme.m_row_selected_background, active_mix );

            g_render.box( segment, g_gui.color( background ), true );

            color_t label_color{ mix_color( CHIP_IDLE_TEXT, CHIP_ACTIVE_TEXT, hover_mix ) };
            label_color = mix_color( label_color, LIST_TEXT_ACTIVE, active_mix );

            g_render.text( MENU_FONT_TINY( ),
                           point_t{ ( segment.left + segment.right ) / 2,
                                    g_gui.tiny_center_y( segment.top,
                                                         segment.bottom - segment.top ) },
                           g_gui.color( label_color ), group_label( this->m_groups[ index ] ),
                           false, render::text_align::center );

            if ( active_mix )
            {
                g_render.box( rect_t{ segment.left, segment.bottom - bar, segment.right,
                                      segment.bottom },
                              g_gui.color( with_alpha( g_theme.m_accent, active_mix ) ), true );
            }

            const bool last{ index + 1 == this->m_group_count };

            g_render.box( rect_t{ segment.left, segment.top, last ? segment.right - 1 : segment.right,
                                  segment.bottom - 1 },
                          g_gui.color( CONTROL_BORDER ) );
        }

        const rect_t list{ this->list_bounds( ) };
        const i32    row_height{ g_gui.scaled( WIDGET_LIST_ROW_HEIGHT ) };
        const i32    mark{ g_gui.scaled( WIDGET_MARK_SIZE ) };

        g_render.box( list, g_gui.color( CONTROL_BACKGROUND ), true );

        for ( usize index{ }; index < group->m_item_count && index < this->m_rows.size( );
              ++index )
        {
            const i32    row_top{ list.top + 1 + static_cast< i32 >( index ) * row_height };
            const rect_t row{ list.left + 1, row_top, list.right - 1, row_top + row_height };

            const u8 active_mix{ eased_mix( this->m_rows[ index ].m_active_mix ) };
            const u8 hover_mix{ eased_mix( this->m_rows[ index ].m_hover_mix ) };

            color_t background{ mix_color( color_t{ 0, 0, 0, 0 }, POPUP_ROW_HOVER, hover_mix ) };
            background = mix_color( background, g_theme.m_row_selected_background, active_mix );

            if ( background.a )
            {
                g_render.box( row, g_gui.color( background ), true );
            }

            if ( active_mix )
            {
                g_render.box( rect_t{ row.left, row.top, row.left + bar, row.bottom },
                              g_gui.color( with_alpha( g_theme.m_accent, active_mix ) ), true );
            }

            color_t label_color{ mix_color( LIST_TEXT_IDLE, MENU_TEXT, hover_mix ) };
            label_color = mix_color( label_color, LIST_TEXT_ACTIVE, active_mix );

            g_render.text( MENU_FONT( ),
                           point_t{ row.left + g_gui.scaled( WIDGET_LIST_PADDING ),
                                    g_gui.text_center_y( row.top, row_height ) },
                           g_gui.color( label_color ), group->m_items[ index ] );

            if ( this->m_marked && this->m_marked( static_cast< usize >( this->m_group
                                                                             ? *this->m_group
                                                                             : 0 ),
                                                   index ) )
            {
                const i32 mark_left{ row.right - g_gui.scaled( WIDGET_LIST_PADDING ) - mark };
                const i32 mark_top{ row.top + ( row_height - mark ) / 2 };

                g_render.box( rect_t{ mark_left, mark_top, mark_left + mark, mark_top + mark },
                              g_gui.color( g_theme.m_accent ), true );
            }
        }

        g_render.box( rect_t{ list.left, list.top, list.right - 1, list.bottom - 1 },
                      g_gui.color( CONTROL_BORDER ) );
    }

    void text_input::setup( )
    {
        this->m_empty_mix = 255;
    }

    i32 text_input::measure( ) const
    {
        return g_gui.scaled( WIDGET_WELL_HEIGHT );
    }

    void text_input::set_cursor( ) const
    {
        g_gui.set_cursor_state( CURSOR_STATE_TEXT_BEAM );
    }

    i32 text_input::text_left( ) const
    {
        const i32 icon_size{ this->m_icon ? g_gui.scaled( MENU_ICON_SIZE ) : 0 };
        const i32 icon_gap{ this->m_icon ? g_gui.scaled( INPUT_ICON_GAP ) : 0 };

        return this->m_bounds.left + g_gui.scaled( INPUT_PADDING_X ) + icon_size + icon_gap;
    }

    i32 text_input::text_room( ) const
    {
        const i32 right{ this->m_bounds.right - g_gui.scaled( INPUT_PADDING_X ) };
        const i32 left{ this->text_left( ) };

        return right > left ? right - left : 1;
    }

    void text_input::think( )
    {
        const i32  left{ this->text_left( ) };
        const i32  room{ this->text_room( ) };
        const u64  now{ clock( ) };
        const bool hovered{ g_gui.hit_test( this->m_bounds ) };

        this->m_hovered = hovered;
        if ( this->m_hovered )
        {
            set_cursor( );
        }

        this->m_focused = g_gui.focused_input( ) == this;

        if ( hovered && g_input.pressed( VK_LBUTTON ) )
        {
            if ( !this->m_focused )
            {
                this->m_focused = true;
                g_gui.set_focused_input( this );
                g_gui.set_listening( nullptr );
            }

            // walk the visible run and drop the caret on the nearest gap
            const i32 target{ g_input.mouse( ).x - left };
            usize     caret{ this->m_buffer.m_scroll };
            i32       walked{ };

            while ( caret < this->m_buffer.m_length )
            {
                const i32 advance{ measure_range( this->m_buffer.m_data, caret, caret + 1 ) };

                if ( walked + advance / 2 >= target )
                {
                    break;
                }

                walked += advance;
                ++caret;
            }

            this->m_buffer.m_caret     = caret;
            this->m_buffer.m_edit_tick = now;
        } else if ( this->m_focused && g_input.pressed( VK_LBUTTON ) )
        {
            this->m_focused = false;
            g_gui.set_focused_input( nullptr );
        }

        if ( this->m_focused && !g_gui.can_interact( ) )
        {
            this->m_focused = false;
            g_gui.set_focused_input( nullptr );
        }

        if ( this->m_focused )
        {
            const edit_result_t result{ edit_text( this->m_buffer, this->m_filter ) };

            if ( result != edit_result_t::idle )
            {
                this->m_buffer.m_edit_tick = now;
            }

            if ( result == edit_result_t::released )
            {
                this->m_focused = false;
                g_gui.set_focused_input( nullptr );
            }
        }

        const u32 delta{ animation_delta( this->m_last_tick ) };
        step_mix( this->m_hover_mix, hovered, delta, WIDGET_HOVER_DURATION );
        step_mix( this->m_focus_mix, this->m_focused, delta, INPUT_FOCUS_DURATION );
        step_mix( this->m_empty_mix, !this->m_buffer.m_length, delta,
                  INPUT_PLACEHOLDER_DURATION );

        if ( this->m_buffer.m_scroll > this->m_buffer.m_caret )
        {
            this->m_buffer.m_scroll = this->m_buffer.m_caret;
        }

        while ( this->m_buffer.m_scroll < this->m_buffer.m_caret &&
                measure_range( this->m_buffer.m_data, this->m_buffer.m_scroll,
                               this->m_buffer.m_caret ) > room )
        {
            ++this->m_buffer.m_scroll;
        }

        while ( this->m_buffer.m_scroll &&
                measure_range( this->m_buffer.m_data, this->m_buffer.m_scroll - 1,
                               this->m_buffer.m_length ) <= room )
        {
            --this->m_buffer.m_scroll;
        }

        const i32 caret_target{ measure_range( this->m_buffer.m_data, this->m_buffer.m_scroll,
                                               this->m_buffer.m_caret ) };

        if ( !this->m_buffer.m_caret_ready )
        {
            this->m_buffer.m_caret_x     = static_cast< float >( caret_target );
            this->m_buffer.m_caret_ready = true;
        }

        this->m_buffer.m_caret_x = approach_float( this->m_buffer.m_caret_x,
                                                   static_cast< float >( caret_target ), delta,
                                                   INPUT_CARET_GLIDE );
    }

    void text_input::draw( ) const
    {
        const i32 height{ this->m_bounds.bottom - this->m_bounds.top };
        const i32 left{ this->text_left( ) };
        const i32 room{ this->text_room( ) };

        const u8 hover_mix{ eased_mix( this->m_hover_mix ) };
        const u8 focus_mix{ eased_out( this->m_focus_mix ) };
        const u8 empty_mix{ eased_mix( this->m_empty_mix ) };

        color_t well_background{ mix_color( this->m_background, CONTROL_BACKGROUND_HOVER,
                                            hover_mix ) };
        well_background   = mix_color( well_background, INPUT_BACKGROUND_FOCUS, focus_mix );
        well_background.a = this->m_background.a;

        g_render.box( this->m_bounds, g_gui.color( well_background ), true );

        g_render.box( rect_t{ this->m_bounds.left, this->m_bounds.top, this->m_bounds.right - 1,
                              this->m_bounds.bottom - 1 },
                      g_gui.color( mix_color( mix_color( this->m_border, CONTROL_BORDER_HOVER,
                                                         hover_mix ),
                                              g_theme.m_input_focus_border, focus_mix ) ) );

        if ( focus_mix )
        {
            const i32 rail_half{ ( this->m_bounds.right - this->m_bounds.left - 2 ) / 2 *
                                 focus_mix / 255 };
            const i32 thickness{ g_gui.scaled( INPUT_RAIL_HEIGHT ) > 0
                                     ? g_gui.scaled( INPUT_RAIL_HEIGHT )
                                     : 1 };

            if ( rail_half > 0 )
            {
                const i32 center{ ( this->m_bounds.left + this->m_bounds.right ) / 2 };
                g_render.box( rect_t{ center - rail_half, this->m_bounds.bottom - thickness,
                                      center + rail_half, this->m_bounds.bottom },
                              g_gui.color( with_alpha( g_theme.m_accent, focus_mix ) ), true );
            }
        }

        if ( this->m_icon )
        {
            draw_icon_glyph( this->m_icon,
                             point_t{ this->m_bounds.left + g_gui.scaled( INPUT_PADDING_X ) +
                                          g_gui.scaled( MENU_ICON_SIZE ) / 2,
                                      this->m_bounds.top + height / 2 },
                             mix_color( MENU_TEXT_DIM, g_theme.m_accent, focus_mix ) );
        }

        const i32 text_y{ g_gui.text_center_y( this->m_bounds.top, height ) };

        if ( empty_mix && !this->m_name.empty( ) )
        {
            const i32 drift{ g_gui.scaled( 3 ) * ( 255 - empty_mix ) / 255 };
            const u8  hint_mix{ static_cast< u8 >( static_cast< u32 >( empty_mix ) *
                                                   ( 255u - focus_mix / 3u ) / 255u ) };

            g_render.text( MENU_FONT( ), point_t{ left + drift, text_y },
                           g_gui.color( with_alpha( MENU_TEXT_DIM, hint_mix ) ), this->m_name );
        }

        usize visible_end{ this->m_buffer.m_scroll };
        i32   visible_width{ };

        while ( visible_end < this->m_buffer.m_length )
        {
            const i32 advance{ measure_range( this->m_buffer.m_data, visible_end,
                                              visible_end + 1 ) };

            if ( visible_width + advance > room )
            {
                break;
            }

            visible_width += advance;
            ++visible_end;
        }

        if ( visible_end > this->m_buffer.m_scroll )
        {
            color_t value_color{ mix_color( MENU_TEXT_MUTED, MENU_TEXT, hover_mix ) };
            value_color = mix_color( value_color, INPUT_TEXT, focus_mix );

            g_render.text( MENU_FONT( ), point_t{ left, text_y }, g_gui.color( value_color ),
                           string_view{ this->m_buffer.m_data + this->m_buffer.m_scroll,
                                        visible_end - this->m_buffer.m_scroll } );
        }

        if ( !focus_mix )
        {
            return;
        }

        const u64 now{ clock( ) };
        const u64 since{ now >= this->m_buffer.m_edit_tick ? now - this->m_buffer.m_edit_tick
                                                           : 0 };
        const u8  blink{ since < INPUT_CARET_SOLID
                             ? static_cast< u8 >( 255 )
                             : pulse( since - INPUT_CARET_SOLID + INPUT_CARET_PERIOD / 2,
                                      INPUT_CARET_PERIOD ) };
        const u8  caret_alpha{ static_cast< u8 >(
            ( static_cast< u32 >( blink ) * focus_mix + 127u ) / 255u ) };
        const i32 caret_x{ math::clamp( left + static_cast< i32 >( this->m_buffer.m_caret_x +
                                                                   0.5f ),
                                        left, left + room - 1 ) };
        const i32 caret_top{ text_y + g_gui.scaled( LABEL_FONT_INK_TOP ) - 1 };
        const i32 caret_thickness{ g_gui.scaled( 1 ) > 0 ? g_gui.scaled( 1 ) : 1 };

        g_render.box( rect_t{ caret_x, caret_top, caret_x + caret_thickness,
                              caret_top + g_gui.scaled( LABEL_FONT_INK_HEIGHT ) + 2 },
                      g_gui.color( with_alpha( g_theme.m_accent, caret_alpha ) ), true );
    }
} // namespace gui
