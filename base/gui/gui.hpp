#pragma once

#include "runtime/array.hpp"
#include "runtime/atomic.hpp"
#include "runtime/vector.hpp"
#include "util/string.hpp"
#include "util/typedefs.hpp"

#include "arena.hpp"
#include "theme.hpp"

namespace gui
{
    enum sound_t : u32
    {
        SOUND_HOVER,
        SOUND_CLICK,
        SOUND_SLIDER,
        SOUND_TOGGLE_ON,
        SOUND_TOGGLE_OFF,
        SOUND_TAB,
        SOUND_POPUP_OPEN,
        SOUND_POPUP_CLOSE,
        SOUND_MENU_OPEN,
        SOUND_MENU_CLOSE,
        SOUND_NOTIFY,
        SOUND_SUCCESS,
        SOUND_WARN,

        SOUND_COUNT
    };

    bool load_sounds( );
    void play_sound( sound_t sound );

    bool    label_matches( string_view label, string_view filter );
    u64     clock( );
    u32     animation_delta( u64 &last_tick );
    u8      animation_step( u32 delta, u32 duration );
    void    step_mix( u8 &mix, bool on, u32 delta, u32 duration );
    u8      mix_channel( u8 from, u8 to, u8 amount );
    color_t mix_color( color_t from, color_t to, u8 amount );
    u8      eased_mix( u8 amount );
    u8      eased_out( u8 amount );
    u8      pulse( u64 now, u32 period );
    float   approach_float( float value, float target, u32 delta, float duration );
    color_t with_alpha( color_t value, u8 opacity );

    i32         estimate_text_width( string_view text );
    i32         estimate_bold_text_width( string_view text );
    i32         estimate_tiny_text_width( string_view text );
    i32         icon_ink_width( const char *glyph, i32 size = MENU_ICON_SIZE );
    void        draw_icon_at( const char *glyph, point_t origin, color_t value,
                              i32 size = MENU_ICON_SIZE );
    void        draw_logo( point_t origin, color_t value );
    void        draw_drop_shadow( rect_t bounds, i32 spread, i32 drop, color_t tint );
    void        draw_falloff_band( rect_t bounds, bool from_top, color_t tint );
    void        fill_horizontal_gradient( rect_t bounds, color_t left, color_t right );
    void        fill_rounded_box( rect_t bounds, i32 radius, color_t value );
    void        stroke_rounded_box( rect_t bounds, i32 radius, color_t value,
                                    i32 thickness = 1 );
    void        fill_ring( point_t center, i32 outer, i32 thickness, float progress,
                           color_t track, color_t fill, bool between_columns = false );
    void        stroke_rounded_wave( rect_t bounds, i32 radius, color_t low, color_t high,
                                     float phase, i32 thickness = 1 );
    void        draw_icon_glyph( const char *glyph, point_t center, color_t value );
    const char *key_name( u32 key );
    i32         measure_range( const char *text, usize first, usize last );
    i32         advance_of( char value );

    template< usize size_amt >
    inline void fit_text( char ( &buffer )[ size_amt ], string_view text, i32 room )
    {
        buffer[ 0 ] = 0;

        if ( text.empty( ) || room <= 0 )
        {
            return;
        }

        const i32 ellipsis{ advance_of( '.' ) * 2 };
        usize     length{ };
        i32       width{ };

        while ( length < text.size( ) && length + 1 < sizeof( buffer ) )
        {
            const i32 advance{ advance_of( text[ length ] ) };

            if ( width + advance > room )
            {
                break;
            }

            width += advance;
            ++length;
        }

        if ( length == text.size( ) )
        {
            str::copy( buffer, sizeof( buffer ), text.data( ), length );
            return;
        }

        // walk back off the end of what fit so the two dots have room of their own
        while ( length && width + ellipsis > room )
        {
            --length;
            width -= advance_of( text[ length ] );
        }

        str::copy( buffer, sizeof( buffer ), text.data( ), length );
        str::append( buffer, sizeof( buffer ), ATLAS_XORSTR( ".." ).get( ) );
    }

    struct row_state_t
    {
        u8   m_active_mix{ };
        u8   m_hover_mix{ };
        u64  m_last_tick{ };
        bool m_hovered{ };
    };

    struct text_buffer_t
    {
        char  m_data[ MENU_TEXT_LENGTH + 1 ]{ };
        usize m_limit{ MENU_TEXT_LENGTH };
        usize m_length{ };
        usize m_caret{ };
        usize m_scroll{ };
        float m_caret_x{ };
        u64   m_edit_tick{ };
        bool  m_caret_ready{ };

        void erase( usize index );
        bool insert( usize index, char value );
    };

    enum class edit_result_t : u8
    {
        idle,
        moved,
        released,
    };

    edit_result_t edit_text( text_buffer_t &buffer, char ( *filter )( char ) = nullptr );

    bool bind_active( bind_t &bind );

    inline constexpr usize STRIP_CAPACITY{ 8 };

    enum class column_t : u8
    {
        left,
        right,
    };

    struct strip_state_t
    {
        atl::array< u8, STRIP_CAPACITY > m_active_mix{ };
        atl::array< u8, STRIP_CAPACITY > m_hover_mix{ };
        u64                              m_last_tick{ };
        float                            m_indicator_offset{ };
        float                            m_indicator_velocity{ };
        bool                             m_initialized{ };
        usize                            m_hovered{ STRIP_CAPACITY };
    };

    struct bound_value_t
    {
        const void *m_address{ };
        usize       m_size{ };
    };

    class group_box;
    class sub_tab;
    class tab;
    class text_input;

    class baseobject
    {
    public:
        baseobject( ) = default;

        explicit baseobject( string_view name )
        {
            this->m_name.assign( name );
        }

        virtual ~baseobject( ) = default;

        virtual void setup( )
        {
        }

        virtual i32  measure( ) const = 0;
        virtual void think( )         = 0;
        virtual void draw( ) const    = 0;

        virtual i32 control_height( ) const
        {
            return this->measure( );
        }

        virtual i32 measure_width( ) const
        {
            return 0;
        }

        virtual bool splits_modules( ) const
        {
            return false;
        }

        virtual rect_t occupied( ) const
        {
            return this->m_bounds;
        }

        virtual bound_value_t bound( ) const
        {
            return { };
        }

        bool step_reveal( u32 delta )
        {
            const u8 before{ this->m_reveal_mix };
            if ( !this->m_reveal_ready )
            {
                this->m_reveal_ready = true;
                this->m_reveal_mix   = this->m_visible ? 255 : 0;
                return this->m_reveal_mix != before;
            }

            if ( this->m_instant_reveal )
            {
                this->m_reveal_mix = this->m_visible ? 255 : 0;
                return this->m_reveal_mix != before;
            }

            step_mix( this->m_reveal_mix, this->m_visible, delta, WIDGET_REVEAL_DURATION );
            return this->m_reveal_mix != before;
        }

        bool laid_out( ) const
        {
            return this->m_visible || this->m_reveal_mix;
        }

        u8 reveal_alpha( ) const
        {
            if ( this->m_reveal_mix < WIDGET_REVEAL_SPLIT )
            {
                return 0;
            }

            const u32 travelled{ static_cast< u32 >( this->m_reveal_mix ) -
                                 WIDGET_REVEAL_SPLIT };
            const u32 span{ 255u - WIDGET_REVEAL_SPLIT };

            return eased_mix( static_cast< u8 >( travelled * 255u / span ) );
        }

        i32 layout_height( ) const
        {
            if ( this->m_reveal_mix >= WIDGET_REVEAL_SPLIT )
            {
                return this->measure( );
            }

            const u8 eased{ eased_mix( static_cast< u8 >(
                static_cast< u32 >( this->m_reveal_mix ) * 255u / WIDGET_REVEAL_SPLIT ) ) };

            return this->measure( ) * static_cast< i32 >( eased ) / 255;
        }

        i32 module_edge( ) const
        {
            i32 edge{ this->m_bounds.right };

            for ( const baseobject *module : this->m_modules )
            {
                if ( !module->visible( ) )
                {
                    continue;
                }

                const rect_t taken{ module->occupied( ) };

                if ( taken.right > taken.left && taken.left < edge )
                {
                    edge = taken.left;
                }
            }

            return edge;
        }

        virtual void draw_last( ) const
        {
        }

        template< typename self_t >
        self_t *set_tooltip( this self_t &self, string_view text )
        {
            self.m_tooltip.assign( text );
            return &self;
        }

        // @ extra search terms. the filter matches the label first, this second, so an
        // element labelled "Enable" can still answer to whatever the game files it under
        template< typename self_t >
        self_t *set_keywords( this self_t &self, string_view keywords )
        {
            self.m_keywords.assign( keywords );
            return &self;
        }

        void set_script_name( string_view name )
        {
            this->m_script_name.assign( name );
        }

        template< typename self_t >
        self_t *set_render_on( this self_t &self, const bool *source, bool expected = true )
        {
            self.m_render_source   = source;
            self.m_render_expected = expected;
            return &self;
        }

        template< typename self_t >
        self_t *set_render_on( this self_t &self, bool ( *predicate )( ) )
        {
            self.m_render_predicate = predicate;
            return &self;
        }

        template< typename self_t >
        self_t *set_instant_reveal( this self_t &self )
        {
            self.m_instant_reveal = true;
            return &self;
        }

        // @ a module rides on its host's row instead of claiming one of its own
        template< typename type_t, typename... args_t >
        type_t *add_module( args_t &&...arguments )
        {
            type_t *module{ make< type_t >( static_cast< args_t && >( arguments )... ) };

            if ( !module )
            {
                return nullptr;
            }

            if ( !this->m_modules.push_back( static_cast< baseobject * >( module ) ) )
            {
                return nullptr; // the node stays in the arena, it just never gets walked
            }

            module->m_parent = this->m_parent;
            module->m_module = true;
            module->setup( );
            return module;
        }

        bool is_module( ) const
        {
            return this->m_module;
        }

        bool evaluate_visibility( )
        {
            bool visible{ true };

            if ( this->m_render_source )
            {
                visible = *this->m_render_source == this->m_render_expected;
            }

            if ( visible && this->m_render_predicate )
            {
                visible = this->m_render_predicate( );
            }

            const bool changed{ visible != this->m_visible };
            this->m_visible = visible;
            return changed;
        }

        bool hide( )
        {
            if ( !this->m_visible )
            {
                return false;
            }

            this->m_visible = false;
            return true;
        }

        void set_parent( group_box *parent )
        {
            this->m_parent = parent;
        }

        void set_bounds( rect_t bounds )
        {
            this->m_bounds = bounds;
        }

        virtual void set_cursor( ) const;

        rect_t bounds( ) const
        {
            return this->m_bounds;
        }

        string_view name( ) const
        {
            return this->m_name.view( );
        }

        string_view tooltip( ) const
        {
            return this->m_tooltip.view( );
        }

        string_view keywords( ) const
        {
            return this->m_keywords.view( );
        }

        bool visible( ) const
        {
            return this->m_visible;
        }

        bool hovered( ) const
        {
            return this->m_hovered;
        }

        const atl::vector< baseobject * > &modules( ) const
        {
            return this->m_modules;
        }

        void draw_tooltip( ) const;
        void step_tooltip( u32 delta );

        rect_t hover_strip( i32 height ) const;

    protected:
        group_box                          *m_parent{ };
        rect_t                              m_bounds{ };
        // ATLAS_XORSTR decrypts into a temporary that dies at the end of the
        // full expression, so a view over it dangles the moment the constructor returns; add_tab
        // with an obfuscated label read freed stack for as long as nothing else touched that stack
        fixed_string< MENU_LABEL_LENGTH >   m_name{ };
        fixed_string< MENU_TOOLTIP_LENGTH > m_tooltip{ };
        fixed_string< MENU_LABEL_LENGTH >   m_keywords{ };
        fixed_string< MENU_LABEL_LENGTH >   m_script_name{ };
        atl::vector< baseobject * >         m_modules{ };

        const bool *m_render_source{ };
        bool ( *m_render_predicate )( ){ };
        bool m_render_expected{ true };

        u8      m_hover_mix{ };
        u8      m_active_mix{ };
        u8      m_reveal_mix{ 255 }; // settled, so nothing animates in on the first frame
        u8      m_tooltip_mix{ };
        point_t m_tooltip_anchor{ };
        u64     m_last_tick{ };
        u64     m_reveal_tick{ };
        bool    m_hovered{ };
        bool    m_visible{ true };
        bool    m_instant_reveal{ };
        bool    m_module{ };
        bool    m_reveal_ready{ };
    };

    enum : i32
    {
        CURSOR_STATE_ARROW       = 0,
        CURSOR_STATE_CLICK_HAND  = 1,
        CURSOR_STATE_TEXT_BEAM   = 2,
        CURSOR_STATE_RESIZE_NWSE = 3,
        CURSOR_STATE_RESIZE_NESW = 4,

        CURSOR_STATE_COUNT,
    };

    class panel
    {
    public:
        bool placed( ) const
        {
            return this->m_placed;
        }

        void place( point_t position )
        {
            this->m_position = position;
            this->m_placed   = true;
        }

        point_t position( ) const
        {
            return this->m_position;
        }

        void drag( rect_t header, i32 screen_width, i32 screen_height, i32 width, i32 height );
        void push_out( rect_t obstacle, i32 gap, i32 screen_width, i32 screen_height, i32 width, i32 height );
        void draw_chrome( rect_t bounds, i32 header_height, u8 show ) const;

    private:
        point_t m_position{ };
        point_t m_drag_offset{ };
        bool    m_placed{ };
        bool    m_dragging{ };
    };

    class window
    {
    public:
        bool init( string_view title, string_view version = { } );
        tab *add_tab( string_view label, const char *icon, bool rail_foot = false );
        void shutdown( );

        void set_status( string_view build, string_view user )
        {
            this->m_build.assign( build );
            this->m_user.assign( user );
        }

        string_view user( ) const
        {
            return this->m_user.view( );
        }

        const atl::vector< tab * > &tabs( ) const
        {
            return this->m_tabs;
        }

        void think( i32 screen_width, i32 screen_height );
        void draw_background( );
        void draw( );

        bool open( ) const
        {
            return this->m_open;
        }

        // anything that publishes a screen rect for something outside the overlay to draw into
        // has to sit still while the window is being moved, or the two disagree for a frame
        bool dragging( ) const
        {
            return this->m_dragging;
        }

        void invalidate_layout( )
        {
            this->m_layout_dirty = true;
        }

        string_view search_filter( ) const;
        i32         scaled( i32 value ) const;
        u8          opacity( ) const;
        color_t     color( color_t value ) const;
        bool        can_interact( ) const;
        bool        hit_test( rect_t bounds ) const;
        i32         text_center_y( i32 top, i32 height ) const;
        i32         tiny_center_y( i32 top, i32 height ) const;

        u64 frame( ) const
        {
            return this->m_frame;
        }

        void set_metrics( i32 menu_width, float scale, bool open )
        {
            this->m_menu_width = menu_width;
            this->m_scale      = scale;
            this->m_open       = open;
        }

        u8 fade( ) const
        {
            return this->m_fade;
        }

        void set_fade( u8 fade )
        {
            this->m_fade = fade;
        }

        rect_t content_room( ) const
        {
            return this->m_content_room;
        }

        rect_t screen_room( ) const
        {
            return rect_t{ 0, 0, this->m_screen_width, this->m_screen_height };
        }

        void set_content_room( rect_t bounds )
        {
            this->m_content_room = bounds;
        }

        void block_hits( rect_t bounds )
        {
            this->m_block         = bounds;
            this->m_block_active  = true;
            this->m_block_renewed = true;
        }

        void clear_hit_block( )
        {
            this->m_block_active  = false;
            this->m_block_renewed = false;
        }

        // the interactive twin of the render clip: while active, hit tests only pass
        // inside this rect, so a control half-hidden by a scrolled body cannot be
        // clicked through the chrome that covers it
        void set_hit_clip( rect_t bounds )
        {
            this->m_hit_clip        = bounds;
            this->m_hit_clip_active = true;
        }

        void clear_hit_clip( )
        {
            this->m_hit_clip_active = false;
        }

        baseobject *active_slider( ) const
        {
            return this->m_active_slider;
        }

        void set_active_slider( baseobject *element )
        {
            this->m_active_slider = element;
        }

        baseobject *listening( ) const
        {
            return this->m_listening;
        }

        void set_listening( baseobject *element )
        {
            this->m_listening      = element;
            this->m_listen_started = this->m_frame;
        }

        // a keybind must not eat the very click that armed it
        bool listen_armed( ) const
        {
            return this->m_frame > this->m_listen_started;
        }

        baseobject *focused_input( ) const
        {
            return this->m_focused_input;
        }

        void set_focused_input( baseobject *element )
        {
            this->m_focused_input = element;
        }

        baseobject *open_popup( ) const
        {
            return this->m_open_popup;
        }

        void set_open_popup( baseobject *element )
        {
            if ( element != this->m_open_popup )
            {
                play_sound( element ? SOUND_POPUP_OPEN : SOUND_POPUP_CLOSE );
            }

            this->m_open_popup = element;
        }

        i32 get_cursor_state( )
        {
            return m_cursor_state;
        }

        void set_cursor_state( i32 cursor_state )
        {
            m_cursor_state = cursor_state;
        }

    private:
        void   update_visibility( );
        void   update_position( i32 screen_width, i32 screen_height, i32 menu_height );
        void   layout( rect_t content );
        void   drive_rail( );
        void   drive_subnav( );
        rect_t rail_bounds( ) const;
        rect_t rail_foot_bounds( ) const;
        rect_t subnav_bounds( ) const;
        rect_t search_bounds( ) const;
        void   draw_search( ) const;
        void   draw_empty_page( ) const;
        void   draw_shell( ) const;
        void   draw_frame( ) const;
        void   draw_header( ) const;
        void   draw_rail( ) const;
        void   draw_subnav( ) const;
        void   draw_footer( ) const;
        rect_t panel_bounds( const sub_tab *page ) const;
        rect_t panel_body( rect_t panel ) const;
        void   draw_panel( const sub_tab *page ) const;

        sub_tab *current_subtab( ) const;

        bool evaluate_all( );
        bool find_match( usize &tab_index, usize &subnav_index ) const;

    protected:
        i32                 m_menu_width{ MENU_BASE_WIDTH };
        i32                 m_screen_width{ };
        i32                 m_screen_height{ };
        u8                  m_background_mix{ };
        u64                 m_background_tick{ };
        float               m_scale{ 1.0f };
        atl::atomic< bool > m_open{ };
        u8                  m_fade{ 255 };
        u64                 m_frame{ };

        rect_t m_content_room{ };
        rect_t m_block{ };
        bool   m_block_active{ };
        bool   m_block_renewed{ };
        rect_t m_hit_clip{ };
        bool   m_hit_clip_active{ };

        baseobject *m_active_slider{ };
        baseobject *m_listening{ };
        baseobject *m_focused_input{ };
        baseobject *m_open_popup{ };
        u64         m_listen_started{ };

        fixed_string< MENU_LABEL_LENGTH > m_title{ };
        fixed_string< MENU_LABEL_LENGTH > m_version{ };
        fixed_string< MENU_LABEL_LENGTH > m_build{ };
        fixed_string< MENU_LABEL_LENGTH > m_user{ };
        text_input                       *m_search{ };
        usize                             m_search_length{ };
        usize                             m_search_return_tab{ };
        usize                             m_search_return_subnav{ };
        atl::vector< tab * >              m_tabs{ };
        usize                             m_selected_tab{ };
        usize                             m_selected_subnav{ };
        usize                             m_foot_tab{ STRIP_CAPACITY }; // STRIP_CAPACITY: no foot tab
        strip_state_t                     m_tab_state{ };
        strip_state_t                     m_subnav_state{ };

        rect_t  m_bounds{ };
        point_t m_position{ };
        point_t m_drag_offset{ };
        bool    m_position_initialized{ };
        bool    m_dragging{ };
        u8      m_scale_mix{ };
        u64     m_scale_tick{ };

        panel m_panel{ };

        rect_t m_layout_content{ };
        usize  m_layout_tab{ };
        usize  m_layout_subnav{ };
        bool   m_layout_dirty{ true };

        u8  m_content_mix{ 255 };
        u64 m_content_tick{ };

        atl::atomic< i32 > m_cursor_state{ };
    };

    // POO at it's finest
    class checkbox : public baseobject
    {
    public:
        checkbox( string_view label, bool *value ) :
            baseobject{ label },
            m_value{ value }
        {
        }

        void setup( ) override;
        i32  measure( ) const override;
        i32  control_height( ) const override;
        void think( ) override;
        void draw( ) const override;

        bound_value_t bound( ) const override
        {
            return { this->m_value, sizeof( *this->m_value ) };
        }

    private:
        rect_t hit_row( ) const;

        bool *m_value{ };
    };

    class keybind : public baseobject
    {
    public:
        keybind( string_view label, bind_t *bind ) :
            baseobject{ label },
            m_bind{ bind }
        {
        }

        i32    measure( ) const override;
        rect_t occupied( ) const override;
        void   think( ) override;
        void   draw( ) const override;
        void   draw_last( ) const override;

        bound_value_t bound( ) const override
        {
            return { this->m_bind, sizeof( *this->m_bind ) };
        }

        void collapse( );

    private:
        rect_t      chip_bounds( ) const;
        i32         popup_height( ) const;
        bool        mode_marked( ) const;
        const char *chip_label( ) const;

        bind_t *m_bind{ };
        float   m_chip_tween{ };
        i32     m_chip_width{ };
        i32     m_revealed{ };
        rect_t  m_popup{ };

        atl::array< row_state_t, 3 > m_rows{ };

        u8   m_listen_pulse{ };
        u8   m_expand_mix{ };
        u8   m_progress{ };
        bool m_listening{ };
        bool m_expanded{ };
        bool m_upward{ };
        bool m_tween_ready{ };
    };

    class slider_int : public baseobject
    {
    public:
        slider_int( string_view label, i32 *value, i32 minimum, i32 maximum,
                    string_view suffix = { } ) :
            baseobject{ label },
            m_value{ value },
            m_minimum{ minimum },
            m_maximum{ maximum }
        {
            this->m_suffix.assign( suffix );
        }

        i32  measure( ) const override;
        void think( ) override;
        void draw( ) const override;

        bound_value_t bound( ) const override
        {
            return { this->m_value, sizeof( *this->m_value ) };
        }

    private:
        i32                              *m_value{ };
        i32                               m_minimum{ };
        i32                               m_maximum{ };
        fixed_string< MENU_LABEL_LENGTH > m_suffix{ };
        bool                              m_dragging{ };
    };

    class slider_float : public baseobject
    {
    public:
        slider_float( string_view label, float *value, float minimum, float maximum,
                      u8 precision = 1, string_view suffix = { } ) :
            baseobject{ label },
            m_value{ value },
            m_minimum{ minimum },
            m_maximum{ maximum },
            m_precision{ precision }
        {
            this->m_suffix.assign( suffix );
        }

        i32  measure( ) const override;
        void think( ) override;
        void draw( ) const override;

        bound_value_t bound( ) const override
        {
            return { this->m_value, sizeof( *this->m_value ) };
        }

    private:
        float                            *m_value{ };
        float                             m_minimum{ };
        float                             m_maximum{ };
        u8                                m_precision{ 1 };
        fixed_string< MENU_LABEL_LENGTH > m_suffix{ };
        bool                              m_dragging{ };
    };

    class button : public baseobject
    {
    public:
        button( string_view label, void ( *callback )( ) ) :
            baseobject{ label },
            m_callback{ callback }
        {
        }

        i32  measure( ) const override;
        i32  measure_width( ) const override;
        void think( ) override;
        void draw( ) const override;

    private:
        void ( *m_callback )( ){ };
        bool m_pressed{ };
    };

    class label : public baseobject
    {
    public:
        label( string_view name, const char *( *text )( ), usize lines = 1 ) :
            baseobject{ name },
            m_text{ text },
            m_lines{ lines }
        {
        }

        i32  measure( ) const override;
        void think( ) override;
        void draw( ) const override;

        bool splits_modules( ) const override
        {
            return !this->m_text;
        }

        virtual void set_cursor( ) const override
        {
        }

        label *set_pair( )
        {
            this->m_pair = true;
            return this;
        }

    private:
        const char *( *m_text )( ){ };
        usize m_lines{ 1 };
        bool  m_pair{ };
    };

    class combo_box : public baseobject
    {
    public:
        combo_box( string_view label, const char *const *items, usize item_count,
                   usize *selected ) :
            baseobject{ label },
            m_items{ items },
            m_item_count{ item_count },
            m_selected{ selected }
        {
        }

        combo_box( string_view label, const char *const *items, usize item_count,
                   bool *selections ) :
            baseobject{ label },
            m_items{ items },
            m_item_count{ item_count },
            m_selections{ selections }
        {
        }

        void setup( ) override;
        i32  measure( ) const override;
        void think( ) override;
        void draw( ) const override;
        void draw_last( ) const override;

        bound_value_t bound( ) const override
        {
            if ( this->m_selections )
            {
                return { this->m_selections, this->m_item_count * sizeof( *this->m_selections ) };
            }

            return { this->m_selected, sizeof( *this->m_selected ) };
        }

        void collapse( );

    private:
        rect_t well_bounds( ) const;
        usize  visible_rows( ) const;
        i32    full_popup_height( ) const;
        i32    popup_inset( ) const;
        void   build_summary( char ( &buffer )[ 96 ], char ( &count_text )[ 32 ],
                              const char *&text, usize &selected_count ) const;

        const char *const *m_items{ };
        usize              m_item_count{ };
        usize             *m_selected{ };
        bool              *m_selections{ };

        atl::vector< row_state_t > m_rows{ };
        rect_t                     m_popup{ };
        u8                         m_expand_mix{ };
        u8                         m_progress{ };
        i32                        m_revealed{ };
        bool                       m_expanded{ };
        bool                       m_upward{ };
    };

    class text_input : public baseobject
    {
    public:
        text_input( string_view placeholder, const char *icon = nullptr,
                    color_t background = CONTROL_BACKGROUND, color_t border = CONTROL_BORDER ) :
            baseobject{ placeholder },
            m_icon{ icon },
            m_background{ background },
            m_border{ border }
        {
        }

        void setup( ) override;
        i32  measure( ) const override;
        void think( ) override;
        void draw( ) const override;

        string_view value( ) const
        {
            return string_view{ this->m_buffer.m_data, this->m_buffer.m_length };
        }

        text_input *set_value( string_view value )
        {
            const usize length{ value.size( ) < this->m_buffer.m_limit
                                    ? value.size( )
                                    : this->m_buffer.m_limit };
            str::copy( this->m_buffer.m_data, sizeof( this->m_buffer.m_data ),
                       value.data( ), length );
            this->m_buffer.m_length      = length;
            this->m_buffer.m_caret       = length;
            this->m_buffer.m_scroll      = 0;
            this->m_buffer.m_caret_ready = false;
            this->m_empty_mix            = length ? 0 : 255;
            return this;
        }

        text_input *set_filter( char ( *filter )( char ) )
        {
            this->m_filter = filter;
            return this;
        }

        text_input *set_limit( usize limit )
        {
            this->m_buffer.m_limit = limit < MENU_TEXT_LENGTH ? limit : MENU_TEXT_LENGTH;
            return this;
        }

        virtual void set_cursor( ) const override;

    private:
        i32 text_left( ) const;
        i32 text_room( ) const;

        const char *m_icon{ };
        color_t     m_background{ };
        color_t     m_border{ };
        char ( *m_filter )( char ){ };

        text_buffer_t m_buffer{ };
        u8            m_focus_mix{ };
        u8            m_empty_mix{ 255 }; // starts empty, so the placeholder starts shown
        bool          m_focused{ };
    };

    class searchable_list : public baseobject
    {
    protected:
        searchable_list( );

        i32  search_height( ) const;
        bool think_search( );

        text_input                       m_search;
        fixed_string< MENU_TEXT_LENGTH > m_filter{ };
    };

    class list_box : public searchable_list
    {
    public:
        list_box( const char *const *items, usize item_count, usize *selected ) :
            m_items{ items },
            m_item_count{ item_count },
            m_selected{ selected }
        {
        }

        void set_items( const char *const *items, usize count );

        void setup( ) override;
        i32  measure( ) const override;
        void think( ) override;
        void draw( ) const override;

        bound_value_t bound( ) const override
        {
            return { this->m_selected, sizeof( *this->m_selected ) };
        }

    private:
        i32   box_top( ) const;
        bool  matches( usize index ) const;
        usize visible_count( ) const;

        const char *const         *m_items{ };
        usize                      m_item_count{ };
        usize                     *m_selected{ };
        atl::vector< row_state_t > m_rows{ };
    };

    struct dynamic_list_box_element_t
    {
        fixed_string< 64 > m_name{ };
        row_state_t        m_row_state{ };
    };

    // Basically a listbox but you can dynamically add elements to the end of the listbox if you wish.
    // TODO: Maybe figure out a way to remove elements cleanly from this...
    class dynamic_list_box : public searchable_list
    {
    public:
        dynamic_list_box( usize *var ) :
            m_selected{ var },
            m_item_count{ 0 },
            m_elements{ }
        {
        }

        i32  measure( ) const override;
        void think( ) override;
        void draw( ) const override;

        bool insert_element( const char *element_name );

    private:
        i32   box_top( ) const;
        bool  matches( usize index ) const;
        usize visible_count( ) const;

        usize                                    *m_selected{ nullptr };
        usize                                     m_item_count{ 0 };
        atl::vector< dynamic_list_box_element_t > m_elements{ };
    };

    class segment_bar : public baseobject
    {
    public:
        segment_bar( const char *const *items, usize count, usize *value ) :
            m_items{ items },
            m_count{ count },
            m_value{ value }
        {
        }

        void setup( ) override;
        i32  measure( ) const override;
        void think( ) override;
        void draw( ) const override;

        bound_value_t bound( ) const override
        {
            return { this->m_value, sizeof( *this->m_value ) };
        }

    private:
        rect_t strip_bounds( ) const;
        rect_t segment_bounds( usize index ) const;

        const char *const *m_items{ };
        usize              m_count{ };
        usize             *m_value{ };

        atl::vector< row_state_t > m_segments{ };
    };

    class target_picker : public baseobject
    {
    public:
        struct group_t
        {
            const char        *m_label{ };
            const char *const *m_items{ };
            usize              m_item_count{ };
        };

        target_picker( const group_t *groups, usize group_count, usize *group, usize *item ) :
            m_groups{ groups },
            m_group_count{ group_count },
            m_group{ group },
            m_item{ item }
        {
        }

        target_picker *set_marks( bool ( *predicate )( usize group, usize item ) )
        {
            this->m_marked = predicate;
            return this;
        }

        void setup( ) override;
        i32  measure( ) const override;
        void think( ) override;
        void draw( ) const override;

    private:
        const group_t *current( ) const;
        rect_t         strip_bounds( ) const;
        rect_t         list_bounds( ) const;

        const group_t *m_groups{ };
        usize          m_group_count{ };
        usize         *m_group{ };
        usize         *m_item{ };
        bool ( *m_marked )( usize group, usize item ){ };

        atl::vector< row_state_t > m_segments{ };
        atl::vector< row_state_t > m_rows{ };
    };

    class color_picker : public baseobject
    {
    public:
        color_picker( string_view label, color_t *value, bool alpha = true ) :
            baseobject{ label },
            m_value{ value },
            m_show_alpha{ alpha }
        {
        }

        void   setup( ) override;
        i32    measure( ) const override;
        i32    measure_width( ) const override;
        rect_t occupied( ) const override;
        void   think( ) override;
        void   draw( ) const override;
        void   draw_last( ) const override;

        bound_value_t bound( ) const override
        {
            return { this->m_value, sizeof( *this->m_value ) };
        }

        color_picker *set_on_change( void ( *callback )( ) )
        {
            this->m_callback = callback;
            return this;
        }

        void collapse( );

    private:
        enum class grab_t : u8
        {
            none,
            field,
            hue,
            alpha,
        };

        rect_t swatch_bounds( ) const;
        rect_t risen_popup( ) const;
        rect_t place_popup( rect_t swatch );
        rect_t field_bounds( rect_t popup ) const;
        rect_t hue_bounds( rect_t popup ) const;
        rect_t alpha_bounds( rect_t popup ) const;
        rect_t preview_bounds( rect_t popup ) const;
        rect_t hex_bounds( rect_t popup ) const;

        void pull( );
        void push( u8 alpha );
        void adopt( color_t value );

        void begin_hex( );
        void release_hex( );
        void trim_hex( );
        void apply_hex( );
        void think_hex( u32 delta );
        void think_grab( );

        void draw_popup( rect_t popup ) const;
        void draw_field( rect_t field ) const;
        void draw_hue( rect_t bar ) const;
        void draw_alpha( rect_t bar ) const;
        void draw_footer( rect_t popup ) const;

        color_t *m_value{ };
        void ( *m_callback )( ){ };
        bool m_show_alpha{ true };

        float   m_hue{ };
        float   m_saturation{ };
        float   m_brightness{ };
        color_t m_mirror{ };
        bool    m_synced{ };

        rect_t m_popup{ };
        grab_t m_grab{ };
        u8     m_expand_mix{ };
        u8     m_progress{ };
        bool   m_expanded{ };
        bool   m_upward{ };

        text_buffer_t m_hex{ };
        u8            m_hex_hover_mix{ };
        u8            m_hex_focus_mix{ };
        bool          m_hex_focused{ };
    };

    class code_editor : public baseobject
    {
    public:
        i32  measure( ) const override;
        void think( ) override;
        void draw( ) const override;
        void set_cursor( ) const override;

        string_view text( ) const
        {
            return string_view{ this->m_text.data( ), this->m_text.size( ) };
        }

        void set_text( string_view text );
        void go_to_line( usize line );

        void mark_line( usize line )
        {
            this->m_marked_line = line;
        }

        usize marked_line( ) const
        {
            return this->m_marked_line;
        }

        usize lines( ) const
        {
            return this->line_count( );
        }

        usize caret_line( ) const
        {
            return this->line_of( this->m_caret ) + 1;
        }

        usize caret_column( ) const
        {
            return this->m_caret - this->line_start( this->line_of( this->m_caret ) ) + 1;
        }

        bool modified( ) const
        {
            return this->m_modified;
        }

        void clear_modified( )
        {
            this->m_modified = false;
        }

        bool take_save_request( )
        {
            const bool requested{ this->m_save_requested };
            this->m_save_requested = false;
            return requested;
        }

    private:
        enum class lex_state_t : u8
        {
            code,
            long_comment,
            long_string,
        };

        struct line_state_t
        {
            lex_state_t m_state{ };
            u8          m_level{ };
        };

        struct edit_record_t
        {
            u32  m_position{ };
            u32  m_removed{ };
            u32  m_inserted{ };
            u32  m_offset{ };
            u32  m_caret_before{ };
            u32  m_anchor_before{ };
            bool m_typing{ };
        };

        template< typename emit_t >
        static line_state_t lex_line( const char *line, usize length, line_state_t state, emit_t &&emit );

        usize  line_count( ) const;
        usize  line_start( usize line ) const;
        usize  line_end( usize line ) const;
        usize  line_of( usize offset ) const;
        usize  visible_lines( ) const;
        usize  visible_columns( ) const;
        rect_t text_area( ) const;
        i32    gutter_width( ) const;
        usize  offset_at( point_t point ) const;
        void   selection( usize &first, usize &last ) const;

        void apply( usize position, usize removed, const char *inserted, usize inserted_length );
        void replace( usize position, usize removed, string_view inserted, bool typing );
        void insert( string_view text, bool typing );
        void undo( );
        void redo( );
        void relex( );

        void move_caret( usize offset, bool extend );
        void keep_caret_visible( );
        void indent_lines( bool outdent );
        void copy_selection( bool cut );
        void paste( );
        void think_mouse( );
        void think_keys( );

        void draw_line( usize line, i32 top, i32 text_left, usize first_column, usize columns ) const;

        atl::vector< char >          m_text{ };
        atl::vector< u32 >           m_lines{ };
        atl::vector< line_state_t >  m_states{ };
        atl::vector< edit_record_t > m_history{ };
        atl::vector< char >          m_history_text{ };
        usize                        m_history_cursor{ };

        usize m_caret{ };
        usize m_anchor{ };
        usize m_goal_column{ };
        usize m_scroll_line{ };
        usize m_scroll_column{ };
        usize m_marked_line{ };
        i32   m_char_width{ };
        u64   m_caret_tick{ };
        u64   m_click_tick{ };
        usize m_click_offset{ };
        bool  m_focused{ };
        bool  m_dragging{ };
        bool  m_modified{ };
        bool  m_save_requested{ };
    };

    class group_box
    {
    public:
        group_box( const char *icon, string_view title, column_t column ) :
            m_icon{ icon },
            m_column{ column }
        {
            this->m_title.assign( title );
        }

        template< typename type_t, typename... args_t >
        type_t *add( args_t &&...arguments )
        {
            type_t *element{ make< type_t >( static_cast< args_t && >( arguments )... ) };

            if ( !element )
            {
                return nullptr;
            }

            if ( !this->m_elements.push_back( static_cast< baseobject * >( element ) ) )
            {
                return nullptr; // the node stays in the arena, it just never gets walked
            }

            element->set_parent( this );
            element->setup( );
            return element;
        }

        void reset_scroll( )
        {
            this->m_scroll        = 0;
            this->m_scroll_target = 0;
            this->m_scroll_glide  = 0.0f;
        }

        bool scrolls_at( i32 wheel ) const;
        bool evaluate( );
        i32  measure( ) const;
        void arrange( rect_t bounds );
        void think( bool scroll );
        void draw( ) const;
        void draw_last( ) const;
        void destroy( );

        column_t column( ) const
        {
            return this->m_column;
        }

        const char *icon( ) const
        {
            return this->m_icon;
        }

        rect_t bounds( ) const
        {
            return this->m_bounds;
        }

        bool visible( ) const
        {
            return this->m_visible;
        }

        const atl::vector< baseobject * > &elements( ) const
        {
            return this->m_elements;
        }

    private:
        rect_t body( ) const;
        bool   intersects_body( rect_t child ) const;

        const char                       *m_icon{ };
        fixed_string< MENU_LABEL_LENGTH > m_title{ };
        column_t                          m_column{ };
        rect_t                            m_bounds{ };
        bool                              m_visible{ true };
        atl::vector< baseobject * >       m_elements{ };

        i32   m_scroll{ };
        i32   m_scroll_target{ };
        float m_scroll_glide{ };
        i32   m_content_height{ };
        i32   m_view_height{ };
        bool  m_scrollable{ };
        u64   m_reveal_tick{ };
        u64   m_think_tick{ };
    };

    class sub_tab
    {
    public:
        explicit sub_tab( string_view label )
        {
            this->m_label.assign( label );
        }

        group_box *add_group( const char *icon, string_view title, column_t column )
        {
            group_box *group{ make< group_box >( icon, title, column ) };

            if ( !group || !this->m_groups.push_back( group ) )
            {
                return nullptr;
            }

            return group;
        }

        template< typename type_t, typename... args_t >
        type_t *set_panel( const char *icon, string_view title, i32 width,
                           args_t &&...arguments )
        {
            type_t *content{ make< type_t >( static_cast< args_t && >( arguments )... ) };

            if ( !content )
            {
                return nullptr;
            }

            this->m_panel       = content;
            this->m_panel_icon  = icon;
            this->m_panel_width = width;
            this->m_panel_title.assign( title );
            content->setup( );
            return content;
        }

        bool evaluate( );
        void layout( rect_t content );
        void think( );
        void draw( ) const;
        void draw_last( ) const;
        void destroy( );

        // whether anything in here survived the conditionals and the search filter
        bool has_visible( ) const;

        string_view label( ) const
        {
            return this->m_label.view( );
        }

        const atl::vector< group_box * > &groups( ) const
        {
            return this->m_groups;
        }

        baseobject *panel( ) const
        {
            return this->m_panel;
        }

        const char *panel_icon( ) const
        {
            return this->m_panel_icon;
        }

        string_view panel_title( ) const
        {
            return this->m_panel_title.view( );
        }

        i32 panel_width( ) const
        {
            return this->m_panel_width;
        }

    private:
        fixed_string< MENU_LABEL_LENGTH > m_label{ };
        atl::vector< group_box * >        m_groups{ };

        rect_t m_content{ };
        i32    m_scroll{ };
        i32    m_scroll_target{ };
        i32    m_runoff{ };
        float  m_scroll_glide{ };
        u64    m_think_tick{ };

        baseobject                       *m_panel{ };
        const char                       *m_panel_icon{ };
        fixed_string< MENU_LABEL_LENGTH > m_panel_title{ };
        i32                               m_panel_width{ };
    };

    class tab
    {
    public:
        tab( string_view label, const char *icon ) :
            m_icon{ icon }
        {
            this->m_label.assign( label );
        }

        sub_tab *add_subtab( string_view label )
        {
            sub_tab *child{ make< sub_tab >( label ) };

            if ( !child || !this->m_subtabs.push_back( child ) )
            {
                return nullptr;
            }

            return child;
        }

        void destroy( );

        string_view label( ) const
        {
            return this->m_label.view( );
        }

        const char *icon( ) const
        {
            return this->m_icon;
        }

        usize subtab_count( ) const
        {
            return this->m_subtabs.size( );
        }

        sub_tab *subtab( usize index ) const
        {
            return index < this->m_subtabs.size( ) ? this->m_subtabs[ index ] : nullptr;
        }

    private:
        fixed_string< MENU_LABEL_LENGTH > m_label{ };
        const char                       *m_icon{ };
        atl::vector< sub_tab * >          m_subtabs{ };
    };
} // namespace gui

extern gui::window g_gui;
