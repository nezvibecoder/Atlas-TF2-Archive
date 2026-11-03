#include "input/input.hpp"
#include "libs/lazy_importer.hpp"
#include "math/math.hpp"
#include "render/render.hpp"
#include "runtime/assert.hpp"
#include "util/windefs.hpp"

#include "gui.hpp"
#include "particles.hpp"
#include "theme.hpp"

inline constexpr float MENU_COLLAPSED_SCALE{ 0.10f };
inline constexpr float MENU_FULL_SCALE{ 1.0f };
inline constexpr u32   MENU_OPEN_DURATION{ 210 };
inline constexpr u32   MENU_CLOSE_DURATION{ 150 };
inline constexpr float MENU_FADE_END_SCALE{ 0.78f };

ATLAS_NO_DESTROY gui::window g_gui{ };

gui::theme_t g_theme{ };

namespace gui
{
    void theme_t::set_accent( color_t accent )
    {
        constexpr color_t black{ 0, 0, 0 };
        constexpr color_t white{ 255, 255, 255 };

        this->m_accent     = accent;
        this->m_accent_alt = mix_color( accent, black, 20 );

        this->m_checkbox_top       = accent;
        this->m_checkbox_top_hover = mix_color( accent, white, 34 );

        this->m_slider_drag        = mix_color( accent, white, 40 );
        this->m_input_focus_border = mix_color( accent, black, 70 );

        this->m_selected_background     = mix_color( RAIL_ACTIVE_BACKGROUND, accent, 10 );
        this->m_row_selected_background = mix_color( POPUP_ROW_SELECTED, accent, 10 );
    }

    i32 window::scaled( i32 value ) const
    {
        return ( value * this->m_menu_width + MENU_BASE_WIDTH / 2 ) / MENU_BASE_WIDTH;
    }

    u8 window::opacity( ) const
    {
        const float progress{ ( this->m_scale - MENU_COLLAPSED_SCALE ) /
                              ( MENU_FADE_END_SCALE - MENU_COLLAPSED_SCALE ) };

        if ( progress <= 0.0f )
        {
            return 0;
        }

        if ( progress >= 1.0f )
        {
            return 255;
        }

        return static_cast< u8 >( math::smooth_step( progress ) * 255.0f + 0.5f );
    }

    color_t window::color( color_t value ) const
    {
        u32 alpha{ static_cast< u32 >( value.a ) * this->opacity( ) };
        alpha = ( alpha + 127u ) / 255u;

        if ( this->m_fade != 255 )
        {
            alpha = ( alpha * this->m_fade + 127u ) / 255u;
        }

        value.a = static_cast< u8 >( alpha );
        return value;
    }

    bool window::can_interact( ) const
    {
        return this->m_open && this->m_scale >= MENU_FULL_SCALE;
    }

    bool window::hit_test( rect_t bounds ) const
    {
        if ( !this->can_interact( ) )
        {
            return false;
        }

        const point_t mouse{ g_input.mouse( ) };

        if ( this->m_block_active && mouse.x >= this->m_block.left &&
             mouse.x < this->m_block.right && mouse.y >= this->m_block.top &&
             mouse.y < this->m_block.bottom )
        {
            return false; // an open popup owns this patch of screen
        }

        if ( this->m_hit_clip_active &&
             ( mouse.x < this->m_hit_clip.left || mouse.x >= this->m_hit_clip.right ||
               mouse.y < this->m_hit_clip.top || mouse.y >= this->m_hit_clip.bottom ) )
        {
            return false; // the visible part of a scrolled body ends here
        }

        return g_input.in_area( bounds.left, bounds.top, bounds.right - bounds.left - 1,
                                bounds.bottom - bounds.top - 1 );
    }

    i32 window::text_center_y( i32 top, i32 height ) const
    {
        return top + ( height - this->scaled( LABEL_FONT_INK_HEIGHT ) ) / 2 -
               this->scaled( LABEL_FONT_INK_TOP );
    }

    i32 window::tiny_center_y( i32 top, i32 height ) const
    {
        return top + ( height - this->scaled( TINY_FONT_INK_HEIGHT ) ) / 2 -
               this->scaled( TINY_FONT_INK_TOP );
    }
} // namespace gui

static float spring_position( float value, float target, float &velocity, u32 delta )
{
    u32   bounded_delta{ delta > 25u ? 25u : delta };
    float seconds{ static_cast< float >( bounded_delta ) / 1000.0f };
    float displacement{ target - value };
    float acceleration{ displacement * 240.0f - velocity * 24.0f };
    velocity += acceleration * seconds;
    value += velocity * seconds;

    if ( math::abs( target - value ) < 0.02f && math::abs( velocity ) < 0.02f )
    {
        value    = target;
        velocity = 0.0f;
    }

    return value;
}

namespace gui
{
    bool window::init( string_view title, string_view version )
    {
        if ( !g_arena.reserve( ) || !load_sounds( ) )
            return false;

        this->m_title.assign( title );
        this->m_version.assign( version );
        this->m_open = true;

        if ( !this->m_search )
        {
            this->m_search = make< text_input >( string_view{ ATLAS_XORSTR_STATIC( "search" ) }, ICON_SEARCH,
                                                 SEARCH_BACKGROUND, SEARCH_BORDER );
        }

        m_cursor_state = CURSOR_STATE_ARROW;

        return this->m_search != nullptr;
    }

    string_view window::search_filter( ) const
    {
        return this->m_search ? this->m_search->value( ) : string_view{ };
    }

    rect_t window::search_bounds( ) const
    {
        const i32 top{ this->m_bounds.top +
                       ( this->scaled( MENU_HEADER_HEIGHT ) -
                         this->scaled( MENU_SEARCH_HEIGHT ) ) /
                           2 };
        const i32 right{ this->m_bounds.right - this->scaled( MENU_SEARCH_MARGIN ) };

        return rect_t{ right - this->scaled( MENU_SEARCH_WIDTH ), top, right,
                       top + this->scaled( MENU_SEARCH_HEIGHT ) };
    }

    rect_t window::rail_bounds( ) const
    {
        return rect_t{ this->m_bounds.left,
                       this->m_bounds.top + this->scaled( MENU_HEADER_HEIGHT ),
                       this->m_bounds.left + this->scaled( MENU_RAIL_WIDTH ),
                       this->m_bounds.bottom - this->scaled( MENU_FOOTER_HEIGHT ) };
    }

    rect_t window::rail_foot_bounds( ) const
    {
        const rect_t rail{ this->rail_bounds( ) };
        const i32    top{ rail.top + this->scaled( MENU_RAIL_TOP_PADDING ) +
                          this->scaled( MENU_RAIL_FOOT_OFFSET ) };

        return rect_t{ rail.left, top, rail.right - 1, top + this->scaled( MENU_RAIL_ITEM_HEIGHT ) };
    }

    rect_t window::subnav_bounds( ) const
    {
        const rect_t rail{ this->rail_bounds( ) };

        return rect_t{ rail.right, rail.top,
                       rail.right + this->scaled( MENU_SUBNAV_WIDTH ), rail.bottom };
    }

    void window::draw_search( ) const
    {
        if ( this->m_search )
        {
            this->m_search->draw( );
        }
    }

    tab *window::add_tab( string_view label, const char *icon, bool rail_foot )
    {
        Assert( this->m_foot_tab == STRIP_CAPACITY );

        tab *child{ make< tab >( label, icon ) };

        if ( !child || !this->m_tabs.push_back( child ) )
            return nullptr;

        if ( rail_foot )
        {
            this->m_foot_tab = this->m_tabs.size( ) - 1;
        }

        this->m_layout_dirty = true;
        return child;
    }

    void window::shutdown( )
    {
        for ( tab *child : this->m_tabs )
        {
            child->destroy( );
            child->~tab( );
        }

        this->m_tabs.clear( );

        if ( this->m_search )
        {
            this->m_search->~text_input( );
            this->m_search = nullptr;
        }

        g_arena.release( );
    }

    sub_tab *window::current_subtab( ) const
    {
        if ( this->m_selected_tab >= this->m_tabs.size( ) )
        {
            return nullptr;
        }

        return this->m_tabs[ this->m_selected_tab ]->subtab( this->m_selected_subnav );
    }

    bool window::evaluate_all( )
    {
        bool changed{ };

        for ( tab *entry : this->m_tabs )
        {
            for ( usize index{ }; index < entry->subtab_count( ); ++index )
            {
                if ( sub_tab * child{ entry->subtab( index ) } )
                {
                    changed |= child->evaluate( );
                }
            }
        }

        return changed;
    }

    bool window::find_match( usize &tab_index, usize &subnav_index ) const
    {
        for ( usize outer{ }; outer < this->m_tabs.size( ); ++outer )
        {
            const tab *entry{ this->m_tabs[ outer ] };

            for ( usize inner{ }; inner < entry->subtab_count( ); ++inner )
            {
                const sub_tab *child{ entry->subtab( inner ) };

                if ( child && child->has_visible( ) )
                {
                    tab_index    = outer;
                    subnav_index = inner;
                    return true;
                }
            }
        }

        return false;
    }

    void window::update_visibility( )
    {
        if ( g_input.pressed( VK_INSERT ) || g_input.pressed( VK_F3 ) )
        {
            this->m_open          = !this->m_open;
            this->m_dragging      = false;
            this->m_focused_input = nullptr;
            this->m_listening     = nullptr;

            play_sound( this->m_open ? SOUND_MENU_OPEN : SOUND_MENU_CLOSE );
        }

        g_input.block( this->m_open );

        const u32 delta{ animation_delta( this->m_scale_tick ) };
        step_mix( this->m_scale_mix, this->m_open, delta,
                  this->m_open ? MENU_OPEN_DURATION : MENU_CLOSE_DURATION );

        const u8 eased{ eased_out( this->m_scale_mix ) };

        this->m_scale = eased == 255 ? MENU_FULL_SCALE
                        : eased == 0 ? MENU_COLLAPSED_SCALE
                                     : MENU_COLLAPSED_SCALE +
                                           ( MENU_FULL_SCALE - MENU_COLLAPSED_SCALE ) *
                                               static_cast< float >( eased ) / 255.0f;
    }

    void window::update_position( i32 screen_width, i32 screen_height, i32 menu_height )
    {
        const i32 maximum_x{ screen_width - this->m_menu_width };
        const i32 maximum_y{ screen_height - menu_height };

        if ( !this->m_position_initialized )
        {
            this->m_position.x           = maximum_x / 2;
            this->m_position.y           = maximum_y / 2;
            this->m_position_initialized = true;
        }

        this->m_position.x = math::clamp( this->m_position.x, 0, maximum_x );
        this->m_position.y = math::clamp( this->m_position.y, 0, maximum_y );

        const i32     drag_left{ this->m_position.x + this->scaled( MENU_HEADER_PADDING ) };
        const i32     drag_right{ this->m_position.x + this->m_menu_width -
                                  this->scaled( MENU_SEARCH_MARGIN + MENU_HEADER_GAP +
                                                MENU_SEARCH_WIDTH ) };
        const i32     header_height{ this->scaled( MENU_HEADER_HEIGHT ) };
        const point_t mouse{ g_input.mouse( ) };

        if ( !this->m_dragging && this->m_scale >= MENU_FULL_SCALE &&
             g_input.pressed( VK_LBUTTON ) && drag_right > drag_left &&
             g_input.in_area( drag_left, this->m_position.y, drag_right - drag_left - 1,
                              header_height - 1 ) )
        {
            this->m_drag_offset.x = mouse.x - this->m_position.x;
            this->m_drag_offset.y = mouse.y - this->m_position.y;
            this->m_dragging      = true;
        }

        if ( !this->m_dragging )
        {
            return;
        }

        if ( !g_input.down( VK_LBUTTON ) )
        {
            this->m_dragging = false;
            return;
        }

        this->m_position.x = math::clamp( mouse.x - this->m_drag_offset.x, 0, maximum_x );
        this->m_position.y = math::clamp( mouse.y - this->m_drag_offset.y, 0, maximum_y );
    }

    void window::layout( rect_t content )
    {
        if ( sub_tab * active{ this->current_subtab( ) } )
        {
            active->layout( content );
        }
    }

    void window::drive_rail( )
    {
        strip_state_t &state{ this->m_tab_state };

        const rect_t rail{ this->rail_bounds( ) };
        const usize  count{ this->m_tabs.size( ) };
        const i32    item_height{ this->scaled( MENU_RAIL_ITEM_HEIGHT ) };
        const i32    items_top{ rail.top + this->scaled( MENU_RAIL_TOP_PADDING ) };
        const bool   enabled{ this->can_interact( ) };

        const u32 delta{ animation_delta( state.m_last_tick ) };
        const u8  active_step{ animation_step( delta, STRIP_ACTIVE_DURATION ) };
        const u8  hover_step{ animation_step( delta, STRIP_HOVER_DURATION ) };

        const float indicator_target{ static_cast< float >(
            this->m_selected_tab == this->m_foot_tab
                ? MENU_RAIL_FOOT_OFFSET
                : static_cast< i32 >( this->m_selected_tab ) * MENU_RAIL_ITEM_HEIGHT ) };

        if ( !state.m_initialized )
        {
            state.m_indicator_offset   = indicator_target;
            state.m_indicator_velocity = 0.0f;
            state.m_initialized        = true;

            if ( this->m_selected_tab < STRIP_CAPACITY )
            {
                state.m_active_mix[ this->m_selected_tab ] = 255;
            }
        }

        state.m_indicator_offset = spring_position( state.m_indicator_offset, indicator_target,
                                                    state.m_indicator_velocity, delta );

        usize hovered_item{ STRIP_CAPACITY };

        for ( usize index{ }; index < count && index < STRIP_CAPACITY; ++index )
        {
            const i32    item_top{ items_top + static_cast< i32 >( index ) * item_height };
            const rect_t item{ index == this->m_foot_tab
                                   ? this->rail_foot_bounds( )
                                   : rect_t{ rail.left, item_top, rail.right - 1,
                                             item_top + item_height } };

            const bool hovered{ enabled && g_input.in_area( item.left, item.top, item.right - item.left, item.bottom - item.top - 1 ) };
            if ( hovered )
            {
                set_cursor_state( CURSOR_STATE_CLICK_HAND );
                hovered_item = index;
            }

            if ( hovered && g_input.pressed( VK_LBUTTON ) )
            {
                if ( index != this->m_selected_tab )
                {
                    play_sound( SOUND_TAB );
                }

                this->m_selected_tab    = index;
                this->m_selected_subnav = 0;
            }

            state.m_active_mix[ index ] =
                math::approach( state.m_active_mix[ index ],
                                static_cast< u8 >( index == this->m_selected_tab ? 255 : 0 ),
                                active_step );
            state.m_hover_mix[ index ] =
                math::approach( state.m_hover_mix[ index ],
                                static_cast< u8 >( hovered ? 255 : 0 ), hover_step );
        }

        if ( hovered_item != state.m_hovered && hovered_item != STRIP_CAPACITY )
        {
            play_sound( SOUND_HOVER );
        }

        state.m_hovered = hovered_item;
    }

    void window::drive_subnav( )
    {
        const tab *owner{ this->m_selected_tab < this->m_tabs.size( )
                              ? this->m_tabs[ this->m_selected_tab ]
                              : nullptr };

        if ( !owner )
        {
            return;
        }

        const usize count{ owner->subtab_count( ) };

        if ( this->m_selected_subnav >= count )
        {
            this->m_selected_subnav = 0;
        }

        strip_state_t &state{ this->m_subnav_state };

        const rect_t subnav{ this->subnav_bounds( ) };
        const i32    item_height{ this->scaled( MENU_SUBNAV_ITEM_HEIGHT ) };
        const i32    items_top{ subnav.top + this->scaled( MENU_SUBNAV_TOP_PADDING ) };
        const bool   enabled{ this->can_interact( ) };

        const u32 delta{ animation_delta( state.m_last_tick ) };
        const u8  active_step{ animation_step( delta, STRIP_ACTIVE_DURATION ) };
        const u8  hover_step{ animation_step( delta, STRIP_HOVER_DURATION ) };

        const float indicator_target{ static_cast< float >( this->m_selected_subnav ) *
                                      static_cast< float >( MENU_SUBNAV_ITEM_HEIGHT ) };

        if ( !state.m_initialized )
        {
            state.m_indicator_offset   = indicator_target;
            state.m_indicator_velocity = 0.0f;
            state.m_initialized        = true;

            if ( this->m_selected_subnav < STRIP_CAPACITY )
            {
                state.m_active_mix[ this->m_selected_subnav ] = 255;
            }
        }

        state.m_indicator_offset = spring_position( state.m_indicator_offset, indicator_target,
                                                    state.m_indicator_velocity, delta );

        usize hovered_item{ STRIP_CAPACITY };

        for ( usize index{ }; index < count && index < STRIP_CAPACITY; ++index )
        {
            const i32  item_top{ items_top + static_cast< i32 >( index ) * item_height };
            const bool hovered{ enabled && g_input.in_area( subnav.left, item_top, subnav.right - subnav.left - 1, item_height - 1 ) };

            if ( hovered )
            {
                set_cursor_state( CURSOR_STATE_CLICK_HAND );
                hovered_item = index;
            }

            if ( hovered && g_input.pressed( VK_LBUTTON ) )
            {
                if ( index != this->m_selected_subnav )
                {
                    play_sound( SOUND_TAB );
                }

                this->m_selected_subnav = index;
            }

            state.m_active_mix[ index ] =
                math::approach( state.m_active_mix[ index ],
                                static_cast< u8 >( index == this->m_selected_subnav ? 255 : 0 ),
                                active_step );
            state.m_hover_mix[ index ] =
                math::approach( state.m_hover_mix[ index ],
                                static_cast< u8 >( hovered ? 255 : 0 ), hover_step );
        }

        if ( hovered_item != state.m_hovered && hovered_item != STRIP_CAPACITY )
        {
            play_sound( SOUND_HOVER );
        }

        state.m_hovered = hovered_item;
    }

    void window::think( i32 screen_width, i32 screen_height )
    {
        this->m_screen_width  = screen_width;
        this->m_screen_height = screen_height;

        step_mix( this->m_background_mix, this->m_open,
                  animation_delta( this->m_background_tick ), BACKGROUND_FADE_DURATION );

        ++this->m_frame;
        // a popup that stopped renewing its block last frame releases the screen it owned
        if ( !this->m_block_renewed )
        {
            this->m_block_active = false;
        }

        this->m_block_renewed = false;

        this->update_visibility( );
        this->set_cursor_state( CURSOR_STATE_ARROW );

        const i32 available_width{ screen_width - MENU_SCREEN_MARGIN };
        const i32 available_height{ screen_height - MENU_SCREEN_MARGIN };

        if ( available_width <= 0 || available_height <= 0 )
        {
            return;
        }

        const i32 width_from_height{ available_height * MENU_BASE_WIDTH / MENU_BASE_HEIGHT };
        const i32 menu_width{ math::smaller(
            MENU_BASE_WIDTH, math::smaller( available_width, width_from_height ) ) };

        if ( menu_width <= 0 )
        {
            return;
        }

        this->m_menu_width = menu_width;

        const i32 menu_height{ ( MENU_BASE_HEIGHT * menu_width + MENU_BASE_WIDTH / 2 ) /
                               MENU_BASE_WIDTH };

        if ( !this->m_open && this->m_scale <= MENU_COLLAPSED_SCALE )
        {
            return;
        }

        if ( this->m_open )
        {
            this->update_position( screen_width, screen_height, menu_height );
        } else
        {
            this->m_dragging = false;
        }

        this->m_bounds = rect_t{ this->m_position.x, this->m_position.y,
                                 this->m_position.x + menu_width,
                                 this->m_position.y + menu_height };

        this->drive_rail( );
        this->drive_subnav( );

        bool query_changed{ };

        if ( this->m_search )
        {
            this->m_search->set_bounds( this->search_bounds( ) );
            this->m_search->think( );

            const usize length{ this->m_search->value( ).size( ) };

            if ( length != this->m_search_length )
            {
                // remember where the user was, so clearing the box puts them back
                if ( !this->m_search_length )
                {
                    this->m_search_return_tab    = this->m_selected_tab;
                    this->m_search_return_subnav = this->m_selected_subnav;
                }

                this->m_search_length = length;
                this->m_layout_dirty  = true;
                query_changed         = true;
            }
        }

        const rect_t content{
            this->m_bounds.left + this->scaled( MENU_RAIL_WIDTH + MENU_SUBNAV_WIDTH +
                                                MENU_CONTENT_PADDING ),
            this->m_bounds.top + this->scaled( MENU_HEADER_HEIGHT + MENU_CONTENT_PADDING ),
            this->m_bounds.right - this->scaled( MENU_CONTENT_PADDING_RIGHT ),
            this->m_bounds.bottom - this->scaled( MENU_FOOTER_HEIGHT + MENU_CONTENT_PADDING )
        };

        this->m_content_room = rect_t{ content.left,
                                       this->m_bounds.top + this->scaled( MENU_HEADER_HEIGHT ),
                                       content.right,
                                       this->m_bounds.bottom -
                                           this->scaled( MENU_FOOTER_HEIGHT ) };

        const bool searching{ this->m_search_length > 0 };

        if ( searching || query_changed )
        {
            if ( this->evaluate_all( ) )
            {
                this->m_layout_dirty = true;
            }
        } else if ( sub_tab * current{ this->current_subtab( ) } )
        {
            if ( current->evaluate( ) )
            {
                this->m_layout_dirty = true;
            }
        }

        if ( query_changed )
        {
            if ( searching )
            {
                // only walk the user somewhere else if where they are came up empty
                const sub_tab *here{ this->current_subtab( ) };

                if ( !here || !here->has_visible( ) )
                {
                    usize found_tab{ };
                    usize found_subnav{ };

                    if ( this->find_match( found_tab, found_subnav ) )
                    {
                        this->m_selected_tab    = found_tab;
                        this->m_selected_subnav = found_subnav;
                    }
                }
            } else
            {
                this->m_selected_tab    = this->m_search_return_tab;
                this->m_selected_subnav = this->m_search_return_subnav;
            }
        }

        sub_tab *active{ this->current_subtab( ) };

        if ( !active )
        {
            return;
        }

        const bool selection_moved{ this->m_layout_tab != this->m_selected_tab ||
                                    this->m_layout_subnav != this->m_selected_subnav };
        const bool content_moved{ content.left != this->m_layout_content.left ||
                                  content.top != this->m_layout_content.top ||
                                  content.right != this->m_layout_content.right ||
                                  content.bottom != this->m_layout_content.bottom };

        if ( this->m_layout_dirty || content_moved || selection_moved )
        {
            this->layout( content );
            this->m_layout_content = content;
            this->m_layout_tab     = this->m_selected_tab;
            this->m_layout_subnav  = this->m_selected_subnav;
            this->m_layout_dirty   = false;
        }

        if ( selection_moved )
        {
            this->m_content_mix = 0; // the new page slides in rather than snapping
        }

        step_mix( this->m_content_mix, true, animation_delta( this->m_content_tick ),
                  CONTENT_ENTER_DURATION );

        active->think( );

        if ( baseobject * panel{ active->has_visible( ) ? active->panel( ) : nullptr } )
        {
            const rect_t bounds{ this->panel_bounds( active ) };

            if ( g_theme.m_dock_panels )
            {
                this->m_panel.place( point_t{ bounds.left, bounds.top } );
            } else
            {
                this->m_panel.drag( rect_t{ bounds.left, bounds.top, bounds.right, bounds.top + this->scaled( PANEL_HEADER_HEIGHT ) },
                                    this->m_screen_width, this->m_screen_height, bounds.right - bounds.left, bounds.bottom - bounds.top );
                this->m_panel.push_out( this->m_bounds, this->scaled( PANEL_DOCK_GAP ), this->m_screen_width, this->m_screen_height,
                                        bounds.right - bounds.left, bounds.bottom - bounds.top );
            }

            panel->set_bounds( this->panel_body( this->panel_bounds( active ) ) );
            panel->think( );
        }
    }

    void window::draw_background( )
    {
        if ( !this->m_open && this->m_scale <= MENU_COLLAPSED_SCALE )
        {
            return;
        }

        const rect_t screen{ 0, 0, this->m_screen_width, this->m_screen_height };
        const u8     background{ eased_mix( this->m_background_mix ) };

        if ( g_theme.m_blur_background )
        {
            g_render.blur( screen, background );
        }

        draw_menu_particles( screen, background );
    }

    void window::draw( )
    {
        if ( !this->m_open && this->m_scale <= MENU_COLLAPSED_SCALE )
        {
            return;
        }

        const u8 shown{ this->opacity( ) };

        const point_t origin{ ( this->m_bounds.left + this->m_bounds.right ) / 2,
                              ( this->m_bounds.top + this->m_bounds.bottom ) / 2 };

        g_render.set_transform( origin, this->m_scale );

        const sub_tab *active{ this->current_subtab( ) };
        const bool     panelled{ active && active->panel( ) && active->has_visible( ) };

        const i32     spread{ this->scaled( MENU_SHADOW_SPREAD ) };
        const i32     drop{ this->scaled( MENU_SHADOW_DROP ) };
        const color_t shadow{ this->color( g_theme.m_shadow ) };

        draw_drop_shadow( this->m_bounds, spread, drop, shadow );

        if ( panelled )
        {
            draw_drop_shadow( this->panel_bounds( active ), spread, drop, shadow );
        }

        g_render.blur( this->m_bounds, shown );

        this->draw_shell( );
        this->draw_header( );
        this->draw_rail( );
        this->draw_subnav( );

        if ( active )
        {
            this->m_fade = eased_mix( this->m_content_mix );

            if ( active->has_visible( ) )
            {
                active->draw( );
            } else
            {
                this->draw_empty_page( );
            }

            if ( panelled )
            {
                this->draw_panel( active );
            }

            this->m_fade = 255;
        }

        this->draw_footer( );
        this->draw_frame( );

        draw_menu_decorations( this->m_bounds, this->subnav_bounds( ),
                               this->scaled( MENU_DECORATION_SIZE ), shown );

        if ( active && active->has_visible( ) )
        {
            this->m_fade = eased_mix( this->m_content_mix );

            active->draw_last( );

            if ( panelled )
            {
                active->panel( )->draw_last( );
            }

            this->m_fade = 255;
        }

        g_render.reset_transform( );
    }

    // @ a page with nothing on it says so instead of leaving the reader staring at the shell.
    // It rides the same enter animation the cards do, so switching onto it does not snap
    void window::draw_empty_page( ) const
    {
        const u8  entered{ eased_mix( this->m_content_mix ) };
        const i32 rise{ this->scaled( CONTENT_ENTER_RISE ) * ( 255 - entered ) / 255 };

        const i32 left{ this->m_content_room.left + this->scaled( MENU_CONTENT_PADDING ) };
        const i32 top{ this->m_content_room.top + this->scaled( MENU_CONTENT_PADDING ) + rise };

        if ( this->m_selected_tab < this->m_tabs.size( ) )
        {
            g_render.text( MENU_FONT( ), point_t{ left, top },
                           this->color( MENU_TEXT_DIM ),
                           this->m_tabs[ this->m_selected_tab ]->label( ) );
        }

        g_render.text( MENU_FONT( ),
                       point_t{ left, top + this->scaled( WIDGET_ROW_HEIGHT ) },
                       this->color( MENU_TEXT_FAINT ),
                       ATLAS_XORSTR( "Nothing to configure here yet." ).get( ) );
    }

    void window::draw_shell( ) const
    {
        const rect_t bounds{ this->m_bounds };
        const rect_t rail{ this->rail_bounds( ) };
        const rect_t subnav{ this->subnav_bounds( ) };
        const i32    header_bottom{ bounds.top + this->scaled( MENU_HEADER_HEIGHT ) };
        const i32    footer_top{ bounds.bottom - this->scaled( MENU_FOOTER_HEIGHT ) };

        g_render.box( bounds, this->color( MENU_BACKGROUND ), true );
        g_render.box( rect_t{ bounds.left, bounds.top, bounds.right, header_bottom },
                      this->color( MENU_HEADER_BACKGROUND ), true );
        g_render.box( rail, this->color( MENU_RAIL_BACKGROUND ), true );
        g_render.box( subnav, this->color( MENU_SUBNAV_BACKGROUND ), true );
        g_render.box( rect_t{ bounds.left, footer_top, bounds.right, bounds.bottom },
                      this->color( MENU_HEADER_BACKGROUND ), true );
    }

    void window::draw_frame( ) const
    {
        const rect_t bounds{ this->m_bounds };
        const rect_t rail{ this->rail_bounds( ) };
        const rect_t subnav{ this->subnav_bounds( ) };
        const i32    header_bottom{ bounds.top + this->scaled( MENU_HEADER_HEIGHT ) };
        const i32    footer_top{ bounds.bottom - this->scaled( MENU_FOOTER_HEIGHT ) };

        g_render.line( point_t{ bounds.left + 1, header_bottom - 1 },
                       point_t{ bounds.right - 2, header_bottom - 1 },
                       this->color( MENU_SECTION_SEPARATOR ) );
        g_render.line( point_t{ rail.right - 1, rail.top },
                       point_t{ rail.right - 1, rail.bottom - 1 },
                       this->color( MENU_SUBNAV_SEPARATOR ) );
        g_render.line( point_t{ subnav.right - 1, subnav.top },
                       point_t{ subnav.right - 1, subnav.bottom - 1 },
                       this->color( MENU_SUBNAV_SEPARATOR ) );
        g_render.line( point_t{ bounds.left + 1, footer_top },
                       point_t{ bounds.right - 2, footer_top },
                       this->color( MENU_SECTION_SEPARATOR ) );

        g_render.box( rect_t{ bounds.left, bounds.top, bounds.right - 1, bounds.bottom - 1 },
                      this->color( MENU_OUTER_BORDER ) );
    }

    void window::draw_header( ) const
    {
        const rect_t bounds{ this->m_bounds };
        const i32    height{ this->scaled( MENU_HEADER_HEIGHT ) };
        const i32    gap{ this->scaled( MENU_HEADER_GAP ) };

        i32 cursor{ bounds.left + this->scaled( MENU_HEADER_PADDING ) };

        draw_logo( point_t{ cursor, bounds.top +
                                        ( height - this->scaled( MENU_LOGO_SIZE ) ) / 2 },
                   this->color( MENU_LOGO_GLYPH ) );

        cursor += this->scaled( MENU_LOGO_SIZE ) + gap;

        const i32 text_y{ this->text_center_y( bounds.top, height ) };

        g_render.text( MENU_FONT_BOLD( ), point_t{ cursor, text_y },
                       this->color( MENU_WORDMARK ), this->m_title );

        cursor += estimate_bold_text_width( this->m_title ) + gap;

        if ( !this->m_version.empty( ) )
        {
            g_render.text( MENU_FONT_TINY( ),
                           point_t{ cursor, this->tiny_center_y( bounds.top, height ) },
                           this->color( MENU_TEXT_FAINT ), this->m_version );
        }

        this->draw_search( );
    }

    void window::draw_rail( ) const
    {
        const strip_state_t &state{ this->m_tab_state };
        const rect_t         rail{ this->rail_bounds( ) };

        const usize count{ this->m_tabs.size( ) };
        const i32   item_height{ this->scaled( MENU_RAIL_ITEM_HEIGHT ) };
        const i32   items_top{ rail.top + this->scaled( MENU_RAIL_TOP_PADDING ) };
        const i32   bar{ this->scaled( MENU_SELECTION_BAR ) };

        for ( usize index{ }; index < count && index < STRIP_CAPACITY; ++index )
        {
            const tab   *entry{ this->m_tabs[ index ] };
            const i32    item_top{ items_top + static_cast< i32 >( index ) * item_height };
            const rect_t item{ index == this->m_foot_tab
                                   ? this->rail_foot_bounds( )
                                   : rect_t{ rail.left, item_top, rail.right - 1,
                                             item_top + item_height } };

            const u8 active_mix{ eased_mix( state.m_active_mix[ index ] ) };
            const u8 hover_mix{ eased_mix( state.m_hover_mix[ index ] ) };

            color_t background{ mix_color( color_t{ 0, 0, 0, 0 }, RAIL_HOVER_BACKGROUND,
                                           hover_mix ) };
            background = mix_color( background, g_theme.m_selected_background, active_mix );

            if ( background.a )
            {
                g_render.box( item, this->color( background ), true );
            }

            color_t item_color{ mix_color( RAIL_IDLE_COLOR, RAIL_HOVER_COLOR, hover_mix ) };
            item_color = mix_color( item_color, g_theme.m_accent, active_mix );

            draw_icon_glyph( entry->icon( ),
                             point_t{ ( item.left + bar + item.right ) / 2,
                                      ( item.top + item.bottom ) / 2 },
                             this->color( item_color ) );
        }

        if ( !count )
        {
            return;
        }

        const float maximum_offset{ static_cast< float >(
            this->m_foot_tab < count
                ? MENU_RAIL_FOOT_OFFSET
                : static_cast< i32 >( count - 1u ) * MENU_RAIL_ITEM_HEIGHT ) };
        const float visual_offset{ math::clamp( state.m_indicator_offset, 0.0f,
                                                maximum_offset ) };
        const i32   bar_top{ items_top +
                             this->scaled( static_cast< i32 >( visual_offset + 0.5f ) ) };

        g_render.box( rect_t{ rail.left, bar_top, rail.left + bar, bar_top + item_height },
                      this->color( g_theme.m_accent ), true );
    }

    void window::draw_subnav( ) const
    {
        const tab *owner{ this->m_selected_tab < this->m_tabs.size( )
                              ? this->m_tabs[ this->m_selected_tab ]
                              : nullptr };

        if ( !owner || !owner->subtab_count( ) )
        {
            return;
        }

        const strip_state_t &state{ this->m_subnav_state };
        const rect_t         subnav{ this->subnav_bounds( ) };

        const usize count{ owner->subtab_count( ) };
        const i32   item_height{ this->scaled( MENU_SUBNAV_ITEM_HEIGHT ) };
        const i32   items_top{ subnav.top + this->scaled( MENU_SUBNAV_TOP_PADDING ) };
        const i32   bar{ this->scaled( MENU_SELECTION_BAR ) };

        for ( usize index{ }; index < count && index < STRIP_CAPACITY; ++index )
        {
            const i32 item_top{ items_top + static_cast< i32 >( index ) * item_height };
            const u8  hover_mix{ eased_mix( state.m_hover_mix[ index ] ) };
            const u8  active_mix{ eased_mix( state.m_active_mix[ index ] ) };

            color_t background{ mix_color( color_t{ 0, 0, 0, 0 }, SUBNAV_HOVER_BACKGROUND,
                                           hover_mix ) };
            background = mix_color( background, g_theme.m_selected_background, active_mix );

            if ( !background.a )
            {
                continue;
            }

            g_render.box( rect_t{ subnav.left, item_top, subnav.right - 1,
                                  item_top + item_height },
                          this->color( background ), true );
        }

        const float maximum_offset{ static_cast< float >(
            ( count - 1u ) * static_cast< usize >( MENU_SUBNAV_ITEM_HEIGHT ) ) };
        const float visual_offset{ math::clamp( state.m_indicator_offset, 0.0f,
                                                maximum_offset ) };
        const i32   bar_top{ items_top +
                             this->scaled( static_cast< i32 >( visual_offset + 0.5f ) ) };

        g_render.box( rect_t{ subnav.left, bar_top, subnav.left + bar,
                              bar_top + item_height },
                      this->color( g_theme.m_accent ), true );

        for ( usize index{ }; index < count && index < STRIP_CAPACITY; ++index )
        {
            const sub_tab *entry{ owner->subtab( index ) };

            if ( !entry )
            {
                continue;
            }

            const i32 item_top{ items_top + static_cast< i32 >( index ) * item_height };
            const u8  hover_mix{ eased_mix( state.m_hover_mix[ index ] ) };
            const u8  active_mix{ eased_mix( state.m_active_mix[ index ] ) };

            color_t label_color{ mix_color( SUBNAV_IDLE_COLOR, SUBNAV_HOVER_COLOR,
                                            hover_mix ) };
            label_color = mix_color( label_color, SUBNAV_ACTIVE_COLOR, active_mix );

            g_render.text( MENU_FONT( ),
                           point_t{ subnav.left + this->scaled( MENU_SUBNAV_INDENT ),
                                    this->text_center_y( item_top, item_height ) },
                           this->color( label_color ), entry->label( ) );
        }
    }

    rect_t window::panel_bounds( const sub_tab *page ) const
    {
        const i32 width{ this->scaled( page->panel_width( ) ) };
        const i32 gap{ this->scaled( PANEL_DOCK_GAP ) };

        if ( !g_theme.m_dock_panels && this->m_panel.placed( ) )
        {
            const point_t position{ this->m_panel.position( ) };
            return rect_t{ position.x, position.y, position.x + width, position.y + this->m_bounds.bottom - this->m_bounds.top };
        }

        i32 left{ this->m_bounds.right + gap };

        if ( left + width > this->m_screen_width )
        {
            left = this->m_bounds.left - gap - width;
        }

        return rect_t{ left, this->m_bounds.top, left + width, this->m_bounds.bottom };
    }

    rect_t window::panel_body( rect_t panel ) const
    {
        const i32 padding{ this->scaled( PANEL_DOCK_PADDING ) };

        return rect_t{ panel.left + padding,
                       panel.top + this->scaled( PANEL_HEADER_HEIGHT ) + padding,
                       panel.right - padding, panel.bottom - padding };
    }

    void window::draw_panel( const sub_tab *page ) const
    {
        const rect_t bounds{ this->panel_bounds( page ) };
        const i32    header{ this->scaled( PANEL_HEADER_HEIGHT ) };
        const i32    header_bottom{ bounds.top + header };

        g_render.blur( bounds, this->opacity( ) );

        g_render.box( bounds, this->color( MENU_BACKGROUND ), true );
        g_render.box( rect_t{ bounds.left, bounds.top, bounds.right, header_bottom },
                      this->color( MENU_HEADER_BACKGROUND ), true );

        i32 cursor{ bounds.left + this->scaled( MENU_HEADER_PADDING ) };

        draw_icon_glyph( page->panel_icon( ),
                         point_t{ cursor + this->scaled( MENU_ICON_SIZE ) / 2,
                                  bounds.top + header / 2 },
                         g_theme.m_accent );

        cursor += this->scaled( MENU_ICON_SIZE + MENU_HEADER_GAP );

        g_render.text( MENU_FONT_BOLD( ),
                       point_t{ cursor, this->text_center_y( bounds.top, header ) },
                       this->color( MENU_TEXT ), page->panel_title( ) );

        g_render.line( point_t{ bounds.left + 1, header_bottom - 1 },
                       point_t{ bounds.right - 2, header_bottom - 1 },
                       this->color( MENU_SECTION_SEPARATOR ) );
        g_render.box( rect_t{ bounds.left, bounds.top, bounds.right - 1, bounds.bottom - 1 },
                      this->color( MENU_OUTER_BORDER ) );

        page->panel( )->draw( );
    }

    void window::draw_footer( ) const
    {
        const rect_t bounds{ this->m_bounds };
        const i32    height{ this->scaled( MENU_FOOTER_HEIGHT ) };
        const i32    footer_top{ bounds.bottom - height };
        const i32    text_y{ this->text_center_y( footer_top, height ) };

        g_render.text( MENU_FONT( ),
                       point_t{ bounds.left + this->scaled( MENU_FOOTER_PADDING ), text_y },
                       this->color( MENU_FOOTER_TEXT ), this->m_build );

        const i32 dot{ this->scaled( MENU_FOOTER_DOT ) };
        const i32 dot_left{ bounds.right - this->scaled( MENU_FOOTER_PADDING_RIGHT ) - dot };
        const i32 dot_top{ footer_top + ( height - dot ) / 2 };

        g_render.box( rect_t{ dot_left, dot_top, dot_left + dot, dot_top + dot },
                      this->color( g_theme.m_accent ), true );

        if ( !this->m_user.empty( ) )
        {
            g_render.text( MENU_FONT( ),
                           point_t{ dot_left - this->scaled( MENU_HEADER_GAP ), text_y },
                           this->color( MENU_FOOTER_TEXT_DIM ), this->m_user, false,
                           render::text_align::right );
        }
    }
} // namespace gui
