#include "input/input.hpp"
#include "math/math.hpp"
#include "render/render.hpp"
#include "util/windefs.hpp"

#include "gui.hpp"
#include "theme.hpp"

namespace gui
{
    ATLAS_FORCE_INLINE void think_element( baseobject *element, u32 delta )
    {
        const bool was_hovered{ element->hovered( ) };

        element->think( );
        element->step_tooltip( delta );

        if ( element->hovered( ) && !was_hovered )
        {
            play_sound( SOUND_HOVER );
        }
    }

    void panel::drag( rect_t header, i32 screen_width, i32 screen_height, i32 width,
                      i32 height )
    {
        const i32 maximum_x{ screen_width - width };
        const i32 maximum_y{ screen_height - height };

        if ( maximum_x < 0 || maximum_y < 0 )
        {
            return;
        }

        const point_t mouse{ g_input.mouse( ) };

        if ( !g_gui.can_interact( ) )
        {
            this->m_dragging = false; // no grabbing things while the menu is shut
        } else if ( !this->m_dragging && g_input.pressed( VK_LBUTTON ) &&
                    g_input.in_area( header.left, header.top, header.right - header.left - 1,
                                     header.bottom - header.top - 1 ) )
        {
            this->m_drag_offset.x = mouse.x - this->m_position.x;
            this->m_drag_offset.y = mouse.y - this->m_position.y;
            this->m_dragging      = true;
        }

        if ( this->m_dragging )
        {
            if ( !g_input.down( VK_LBUTTON ) )
            {
                this->m_dragging = false;
            } else
            {
                this->m_position.x = mouse.x - this->m_drag_offset.x;
                this->m_position.y = mouse.y - this->m_drag_offset.y;
            }
        }

        this->m_position.x = math::clamp( this->m_position.x, 0, maximum_x );
        this->m_position.y = math::clamp( this->m_position.y, 0, maximum_y );
    }

    void panel::push_out( rect_t obstacle, i32 gap, i32 screen_width, i32 screen_height, i32 width,
                          i32 height )
    {
        const rect_t  taken{ obstacle.left - gap, obstacle.top - gap, obstacle.right + gap, obstacle.bottom + gap };
        const point_t at{ this->m_position };

        if ( at.x >= taken.right || at.x + width <= taken.left || at.y >= taken.bottom || at.y + height <= taken.top )
        {
            return;
        }

        const point_t candidates[ ]{ point_t{ taken.left - width, at.y }, point_t{ taken.right, at.y },
                                     point_t{ at.x, taken.top - height }, point_t{ at.x, taken.bottom } };

        i32 best_distance{ -1 };

        for ( const point_t candidate : candidates )
        {
            if ( candidate.x < 0 || candidate.y < 0 || candidate.x + width > screen_width ||
                 candidate.y + height > screen_height )
            {
                continue;
            }

            const i32 shift_x{ candidate.x - at.x };
            const i32 shift_y{ candidate.y - at.y };
            const i32 distance{ ( shift_x < 0 ? -shift_x : shift_x ) + ( shift_y < 0 ? -shift_y : shift_y ) };

            if ( best_distance < 0 || distance < best_distance )
            {
                best_distance    = distance;
                this->m_position = candidate;
            }
        }
    }

    void panel::draw_chrome( rect_t bounds, i32 header_height, u8 show ) const
    {
        const rect_t header{ bounds.left, bounds.top, bounds.right, bounds.top + header_height };

        g_render.blur( bounds, show );
        g_render.box( bounds, with_alpha( PANEL_BACKGROUND, show ), true );
        g_render.box( header, with_alpha( PANEL_HEADER_BACKGROUND, show ), true );

        if ( bounds.bottom > header.bottom )
        {
            g_render.line( point_t{ header.left, header.bottom },
                           point_t{ header.right - 1, header.bottom },
                           with_alpha( MENU_SECTION_SEPARATOR, show ) );
        }

        g_render.box( rect_t{ bounds.left + 1, bounds.top + 1, bounds.right - 2,
                              bounds.bottom - 2 },
                      with_alpha( PANEL_INNER_BORDER, show ) );
        g_render.box( rect_t{ bounds.left, bounds.top, bounds.right - 1, bounds.bottom - 1 },
                      with_alpha( g_theme.m_accent, show ) );
    }

    bool label_matches( string_view label, string_view filter )
    {
        if ( filter.empty( ) )
        {
            return true;
        }

        if ( filter.size( ) > label.size( ) )
        {
            return false;
        }

        for ( usize start{ }; start + filter.size( ) <= label.size( ); ++start )
        {
            if ( str::equals_ignore_case( label.data( ) + start, filter.size( ), filter.data( ),
                                          filter.size( ) ) )
            {
                return true;
            }
        }

        return false;
    }

    bool group_box::evaluate( )
    {
        const string_view filter{ g_gui.search_filter( ) };
        const u32         delta{ animation_delta( this->m_reveal_tick ) };

        bool changed{ };
        bool any_visible{ };

        for ( baseobject *element : this->m_elements )
        {
            changed |= element->evaluate_visibility( );

            if ( element->visible( ) && !label_matches( element->name( ), filter ) &&
                 !label_matches( element->keywords( ), filter ) )
            {
                changed |= element->hide( );
            }

            any_visible |= element->visible( );

            for ( baseobject *module : element->modules( ) )
            {
                changed |= module->evaluate_visibility( );

                if ( module->visible( ) && !element->visible( ) )
                {
                    changed |= module->hide( );
                }

                module->step_reveal( delta );
            }

            changed |= element->step_reveal( delta );
        }

        bool still_showing{ this->m_elements.empty( ) };
        for ( const baseobject *element : this->m_elements )
        {
            still_showing |= element->laid_out( );
        }

        const bool visible{ any_visible || still_showing };

        if ( visible != this->m_visible )
        {
            this->m_visible = visible;
            changed         = true;
        }

        return changed;
    }

    i32 group_box::measure( ) const
    {
        const i32 body_top{ g_gui.scaled( CARD_TITLE_HEIGHT ) + g_gui.scaled( CARD_PADDING_TOP ) };
        const i32 gap{ g_gui.scaled( CARD_BLOCK_GAP ) };

        i32 height{ body_top };
        i32 counted{ };

        for ( const baseobject *element : this->m_elements )
        {
            if ( !element->laid_out( ) )
            {
                continue;
            }

            height += element->layout_height( ) + gap;
            ++counted;
        }

        if ( counted )
        {
            height -= gap; // the gap only sits between blocks, not after the last one
        }

        return height + g_gui.scaled( CARD_PADDING_BOTTOM );
    }

    rect_t group_box::body( ) const
    {
        const i32 body_top{ g_gui.scaled( CARD_TITLE_HEIGHT ) + g_gui.scaled( CARD_PADDING_TOP ) };

        return rect_t{ this->m_bounds.left, this->m_bounds.top + body_top, this->m_bounds.right,
                       this->m_bounds.bottom - g_gui.scaled( CARD_PADDING_BOTTOM ) };
    }

    // anything that pokes into the view gets drawn; the render clip and the hit clip
    // take care of the part that hangs outside, so scrolling never pops whole rows
    bool group_box::intersects_body( rect_t child ) const
    {
        const rect_t view{ this->body( ) };
        const rect_t page{ g_gui.content_room( ) };
        return child.bottom > math::bigger( view.top, page.top ) &&
               child.top < math::smaller( view.bottom, page.bottom );
    }

    void group_box::arrange( rect_t bounds )
    {
        this->m_bounds = bounds;

        const i32 body_top{ g_gui.scaled( CARD_TITLE_HEIGHT ) + g_gui.scaled( CARD_PADDING_TOP ) };
        const i32 gap{ g_gui.scaled( CARD_BLOCK_GAP ) };
        const i32 left{ bounds.left + g_gui.scaled( CARD_PADDING_X ) };
        const i32 right{ bounds.right - g_gui.scaled( CARD_PADDING_X ) };

        const rect_t view{ this->body( ) };

        this->m_view_height    = view.bottom > view.top ? view.bottom - view.top : 0;
        this->m_content_height = this->measure( ) - body_top -
                                 g_gui.scaled( CARD_PADDING_BOTTOM );
        this->m_scrollable = this->m_content_height > this->m_view_height;

        const i32 runoff{ this->m_scrollable ? this->m_content_height - this->m_view_height : 0 };

        this->m_scroll_target = math::clamp( this->m_scroll_target, 0, runoff );
        this->m_scroll_glide  = math::clamp( this->m_scroll_glide, 0.0f,
                                             static_cast< float >( runoff ) );
        this->m_scroll        = math::clamp( this->m_scroll, 0, runoff );

        i32 cursor{ bounds.top + body_top - this->m_scroll };

        for ( baseobject *element : this->m_elements )
        {
            if ( !element->laid_out( ) )
            {
                continue;
            }

            const i32 height{ element->layout_height( ) };
            rect_t    block{ left, cursor, right, cursor + height };

            const i32 strip_bottom{ cursor + element->control_height( ) };
            const i32 module_gap{ g_gui.scaled( WIDGET_BUTTON_GAP ) };

            if ( element->splits_modules( ) && !element->modules( ).empty( ) )
            {
                const i32 count{ static_cast< i32 >( element->modules( ).size( ) ) };
                const i32 share{ ( right - left - ( count - 1 ) * module_gap ) / count };

                i32 module_left{ left };

                for ( baseobject *module : element->modules( ) )
                {
                    const i32 module_right{ module == element->modules( ).back( ) ? right : module_left + share };

                    module->set_bounds( rect_t{ module_left, cursor, module_right, strip_bottom } );
                    module_left = module_right + module_gap;
                }

                element->set_bounds( block );
                cursor = block.bottom + gap;
                continue;
            }

            i32   sized_width{ };
            usize sized_count{ };

            for ( const baseobject *module : element->modules( ) )
            {
                if ( const i32 width{ module->measure_width( ) }; width > 0 )
                {
                    sized_width += width;
                    ++sized_count;
                }
            }

            if ( sized_count > 1 )
            {
                sized_width += static_cast< i32 >( sized_count - 1 ) * module_gap;
            }

            const i32 module_edge{ sized_count ? right - sized_width : right };

            i32 module_left{ module_edge };

            for ( baseobject *module : element->modules( ) )
            {
                const i32 width{ module->measure_width( ) };

                if ( width <= 0 )
                {
                    module->set_bounds( rect_t{ left, cursor, module_edge, strip_bottom } );
                    continue;
                }

                module->set_bounds( rect_t{ module_left, cursor, module_left + width,
                                            strip_bottom } );
                module_left += width + module_gap;
            }

            // the host stops where the sized modules begin, so nothing paints over anything
            if ( sized_count )
            {
                block.right = module_edge - module_gap;
            }

            element->set_bounds( block );

            cursor = block.bottom + gap;
        }
    }

    bool group_box::scrolls_at( i32 wheel ) const
    {
        return this->m_scrollable && g_gui.hit_test( this->body( ) ) &&
               ( ( wheel > 0 && this->m_scroll_target > 0 ) ||
                 ( wheel < 0 && this->m_scroll_target < this->m_content_height - this->m_view_height ) );
    }

    void group_box::think( bool scroll )
    {
        const u32 think_delta{ animation_delta( this->m_think_tick ) };

        if ( this->m_scrollable )
        {
            if ( scroll )
            {
                const i32 runoff{ this->m_content_height - this->m_view_height };

                this->m_scroll_target = math::clamp(
                    this->m_scroll_target - g_input.wheel( ) * g_gui.scaled( SCROLL_WHEEL_STEP ),
                    0, runoff );
            }

            const float target{ static_cast< float >( this->m_scroll_target ) };

            this->m_scroll_glide = approach_float( this->m_scroll_glide, target, think_delta,
                                                   SCROLL_GLIDE_DURATION );

            const float left_over{ target - this->m_scroll_glide };

            if ( left_over > -0.5f && left_over < 0.5f )
            {
                this->m_scroll_glide = target; // close enough that no frame moves a pixel again
            }

            const i32 moved{ static_cast< i32 >( this->m_scroll_glide + 0.5f ) };

            if ( moved != this->m_scroll )
            {
                this->m_scroll = moved;
                this->arrange( this->m_bounds ); // cheap, and keeps bounds honest this frame
            }
        }

        const rect_t body{ this->body( ) };
        const rect_t page{ g_gui.content_room( ) };
        g_gui.set_hit_clip( rect_t{ body.left, math::bigger( body.top, page.top ),
                                    body.right, math::smaller( body.bottom, page.bottom ) } );

        for ( baseobject *element : this->m_elements )
        {
            if ( !element->visible( ) || !this->intersects_body( element->bounds( ) ) )
            {
                continue;
            }

            think_element( element, think_delta );

            for ( baseobject *module : element->modules( ) )
            {
                think_element( module, think_delta );
            }
        }

        g_gui.clear_hit_clip( );
    }

    void group_box::draw( ) const
    {
        const rect_t bounds{ this->m_bounds };
        const i32    title_bottom{ bounds.top + g_gui.scaled( CARD_TITLE_HEIGHT ) };

        g_render.box( bounds, g_gui.color( CARD_BACKGROUND ), true );
        g_render.box( rect_t{ bounds.left + 1, bounds.top + 1, bounds.right - 1, title_bottom },
                      g_gui.color( CARD_TITLE_BACKGROUND ), true );
        g_render.box( rect_t{ bounds.left, bounds.top, bounds.right - 1, bounds.bottom - 1 },
                      g_gui.color( CARD_BORDER ) );

        g_render.line( point_t{ bounds.left + 1, title_bottom },
                       point_t{ bounds.right - 2, title_bottom }, g_gui.color( CARD_RULE ) );

        g_render.text( MENU_FONT( ),
                       point_t{ bounds.left + g_gui.scaled( CARD_PADDING_X ),
                                g_gui.text_center_y( bounds.top + 1,
                                                     g_gui.scaled( CARD_TITLE_HEIGHT ) - 1 ) },
                       g_gui.color( CARD_TITLE_TEXT ), this->m_title );

        const u8 card_fade{ g_gui.fade( ) };

        if ( this->m_scrollable )
        {
            const rect_t body{ this->body( ) };
            const rect_t page{ g_gui.content_room( ) };
            const rect_t clip{ body.left, math::bigger( body.top, page.top ),
                               body.right, math::smaller( body.bottom, page.bottom ) };
            g_render.set_clip( rect_t{ bounds.left + 1, clip.top, bounds.right - 1,
                                       clip.bottom } );
        }

        for ( const baseobject *element : this->m_elements )
        {
            if ( !element->laid_out( ) || !this->intersects_body( element->bounds( ) ) )
            {
                continue;
            }

            const u8 reveal{ element->reveal_alpha( ) };

            if ( !reveal )
            {
                continue; // faded out already; only its height is still closing
            }

            if ( reveal != 255 )
            {
                g_gui.set_fade( static_cast< u8 >( ( card_fade * reveal + 127 ) / 255 ) );
            }

            element->draw( );

            for ( const baseobject *module : element->modules( ) )
            {
                if ( module->visible( ) )
                {
                    module->draw( );
                }
            }

            g_gui.set_fade( card_fade );
        }

        g_render.set_clip( g_gui.content_room( ) );

        if ( !this->m_scrollable || this->m_view_height <= 0 )
        {
            return;
        }

        const rect_t view{ this->body( ) };
        const i32    hidden_below{ this->m_content_height - this->m_view_height -
                                this->m_scroll };
        const i32    depth{ math::smaller( g_gui.scaled( SCROLL_SHADOW_DEPTH ),
                                           this->m_view_height / 2 ) };

        if ( depth > 0 )
        {
            // the shadow ramps in over the first shadow-depth of travel, so it fades
            // with the glide instead of popping when an edge is reached
            const u8 top_strength{ static_cast< u8 >(
                math::smaller( this->m_scroll, depth ) * 255 / depth ) };
            const u8 bottom_strength{ static_cast< u8 >(
                math::smaller( hidden_below, depth ) * 255 / depth ) };

            // the band starts a little into the body so it does not sit on the title rule
            const i32 drop{ g_gui.scaled( SCROLL_SHADOW_DROP ) };

            draw_falloff_band( rect_t{ bounds.left + 1, view.top, bounds.right - 1,
                                       view.top + depth },
                               true, g_gui.color( with_alpha( SCROLL_SHADOW, top_strength ) ) );
            draw_falloff_band( rect_t{ bounds.left + 1, view.bottom - depth + drop,
                                       bounds.right - 1, view.bottom + drop },
                               false, g_gui.color( with_alpha( SCROLL_SHADOW, bottom_strength ) ) );
        }

        const i32 width{ g_gui.scaled( SCROLLBAR_WIDTH ) };
        const i32 rail_right{ bounds.right - g_gui.scaled( SCROLLBAR_INSET ) };
        const i32 rail_left{ rail_right - width };

        const i32 proportional{ this->m_view_height * this->m_view_height /
                                this->m_content_height };
        const i32 thumb_height{ proportional < width * 2 ? width * 2 : proportional };
        const i32 runoff{ this->m_content_height - this->m_view_height };
        const i32 travel{ this->m_view_height - thumb_height };
        const i32 thumb_top{ view.top + ( runoff > 0 ? this->m_scroll * travel / runoff : 0 ) };

        g_render.box( rect_t{ rail_left, view.top, rail_right, view.bottom },
                      g_gui.color( SCROLLBAR_TRACK ), true );
        g_render.box( rect_t{ rail_left, thumb_top, rail_right, thumb_top + thumb_height },
                      g_gui.color( SCROLLBAR_THUMB ), true );
    }

    void group_box::draw_last( ) const
    {
        for ( const baseobject *element : this->m_elements )
        {
            if ( !element->visible( ) || !this->intersects_body( element->bounds( ) ) )
            {
                continue;
            }

            element->draw_last( );
            element->draw_tooltip( );

            for ( const baseobject *module : element->modules( ) )
            {
                if ( !module->visible( ) )
                {
                    continue;
                }

                module->draw_last( );
                module->draw_tooltip( );
            }
        }
    }

    void group_box::destroy( )
    {
        // the arena never runs a destructor, so anything holding heap
        // has to be told to let go before the block is released
        for ( baseobject *element : this->m_elements )
        {
            for ( baseobject *module : element->modules( ) )
            {
                module->~baseobject( );
            }

            element->~baseobject( );
        }

        this->m_elements.clear( );
    }

    bool sub_tab::evaluate( )
    {
        bool changed{ };

        for ( group_box *group : this->m_groups )
        {
            changed |= group->evaluate( );
        }

        return changed;
    }

    void sub_tab::layout( rect_t content )
    {
        this->m_content = content;
        const i32 gap{ g_gui.scaled( MENU_COLUMN_GAP ) };
        const i32 view_height{ content.bottom - content.top };
        i32       height[ 2 ]{ };

        for ( const group_box *group : this->m_groups )
        {
            if ( group->visible( ) )
            {
                height[ static_cast< usize >( group->column( ) ) ] +=
                    math::smaller( group->measure( ), view_height ) + gap;
            }
        }

        this->m_runoff        = math::bigger( 0, math::bigger( height[ 0 ], height[ 1 ] ) - gap - view_height );
        this->m_scroll_target = math::clamp( this->m_scroll_target, 0, this->m_runoff );
        this->m_scroll_glide  = math::clamp( this->m_scroll_glide, 0.0f, static_cast< float >( this->m_runoff ) );
        this->m_scroll        = math::clamp( this->m_scroll, 0, this->m_runoff );

        i32 cursor[ 2 ]{ content.top - this->m_scroll, content.top - this->m_scroll };
        for ( group_box *group : this->m_groups )
        {
            if ( !group->visible( ) )
            {
                continue;
            }

            const usize column{ static_cast< usize >( group->column( ) ) };
            const i32   left{ content.left + static_cast< i32 >( column ) *
                                              ( g_gui.scaled( MENU_COLUMN_WIDTH ) + gap ) };
            const i32   right{ column ? content.right : left + g_gui.scaled( MENU_COLUMN_WIDTH ) };
            const i32   size{ math::smaller( group->measure( ), view_height ) };
            group->arrange( rect_t{ left, cursor[ column ], right, cursor[ column ] + size } );
            cursor[ column ] += size + gap;
        }
    }

    bool sub_tab::has_visible( ) const
    {
        for ( const group_box *group : this->m_groups )
        {
            if ( group->visible( ) )
            {
                return true;
            }
        }

        return false;
    }

    void sub_tab::think( )
    {
        const u32 delta{ animation_delta( this->m_think_tick ) };
        const i32 wheel{ g_input.wheel( ) };
        group_box *scroll_group{ };
        g_gui.set_hit_clip( g_gui.content_room( ) );
        for ( group_box *group : this->m_groups )
        {
            if ( group->visible( ) && group->scrolls_at( wheel ) )
            {
                scroll_group = group;
                break;
            }
        }

        if ( wheel && !scroll_group && g_gui.hit_test( this->m_content ) )
        {
            this->m_scroll_target = math::clamp(
                this->m_scroll_target - wheel * g_gui.scaled( SCROLL_WHEEL_STEP ), 0, this->m_runoff );
        }
        g_gui.clear_hit_clip( );

        this->m_scroll_glide = approach_float( this->m_scroll_glide,
            static_cast< float >( this->m_scroll_target ), delta, SCROLL_GLIDE_DURATION );
        const i32 moved{ static_cast< i32 >( this->m_scroll_glide + 0.5f ) };

        if ( moved != this->m_scroll )
        {
            this->m_scroll = moved;
            this->layout( this->m_content );
        }

        for ( group_box *group : this->m_groups )
        {
            if ( group->visible( ) )
            {
                g_gui.set_hit_clip( g_gui.content_room( ) );
                group->think( group == scroll_group );
            }
        }
    }

    void sub_tab::draw( ) const
    {
        for ( const group_box *group : this->m_groups )
        {
            if ( group->visible( ) )
            {
                const rect_t bounds{ group->bounds( ) };
                const rect_t page{ g_gui.content_room( ) };
                if ( bounds.bottom > page.top && bounds.top < page.bottom )
                {
                    g_render.set_clip( page );
                    group->draw( );
                    g_render.reset_clip( );
                }
            }
        }

        if ( this->m_runoff > 0 )
        {
            const i32 height{ this->m_content.bottom - this->m_content.top };
            const i32 width{ g_gui.scaled( SCROLLBAR_WIDTH ) };
            const i32 thumb{ math::bigger( width * 2, height * height / ( height + this->m_runoff ) ) };
            const i32 top{ this->m_content.top + this->m_scroll * ( height - thumb ) / this->m_runoff };
            const i32 left{ this->m_content.right + g_gui.scaled( SCROLLBAR_INSET ) };
            g_render.box( rect_t{ left, this->m_content.top, left + width, this->m_content.bottom },
                          g_gui.color( SCROLLBAR_TRACK ), true );
            g_render.box( rect_t{ left, top, left + width, top + thumb },
                          g_gui.color( SCROLLBAR_THUMB ), true );
        }
    }

    void sub_tab::draw_last( ) const
    {
        for ( const group_box *group : this->m_groups )
        {
            if ( group->visible( ) )
            {
                group->draw_last( );
            }
        }
    }

    void sub_tab::destroy( )
    {
        for ( group_box *group : this->m_groups )
        {
            group->destroy( );
            group->~group_box( );
        }

        this->m_groups.clear( );

        if ( this->m_panel )
        {
            this->m_panel->~baseobject( );
            this->m_panel = nullptr;
        }
    }

    void tab::destroy( )
    {
        for ( sub_tab *child : this->m_subtabs )
        {
            child->destroy( );
            child->~sub_tab( );
        }

        this->m_subtabs.clear( );
    }
} // namespace gui
