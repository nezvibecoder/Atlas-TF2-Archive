#include "ide.hpp"

#include "input/input.hpp"
#include "render/render.hpp"
#include "util/format.hpp"

namespace script
{
    inline constexpr i32 SECTION_GAP{ 5 };
    inline constexpr i32 CONSOLE_ROWS_OPEN{ 8 };
    inline constexpr i32 CONSOLE_ROWS_SHUT{ 3 };
    inline constexpr i32 CONSOLE_HEADER_HEIGHT{ 17 };
    inline constexpr i32 CONSOLE_BUTTON_INSET{ 2 };
    inline constexpr i32 CONSOLE_BADGE_PADDING{ 4 };
    inline constexpr i32 CONSOLE_BADGE_INSET{ 3 };

    ATLAS_FORCE_INLINE usize error_line( string_view text, string_view name )
    {
        if ( name.empty( ) )
        {
            return 0;
        }

        char needle[ NAME_LENGTH + 8 ]{ };
        str::copy( needle, sizeof( needle ), name.data( ), name.size( ) );
        str::append( needle, sizeof( needle ), ATLAS_XORSTR( ".lua:" ).get( ) );

        const usize needle_size{ str::length( needle ) };
        const usize found{ str::find( text.data( ), text.size( ), needle, needle_size ) };

        if ( found == str::npos )
        {
            return 0;
        }

        usize line{ };
        for ( usize index{ found + needle_size }; index < text.size( ) && str::is_digit( text[ index ] ); ++index )
        {
            line = line * 10 + static_cast< usize >( text[ index ] - '0' );
        }

        return line;
    }

    ide::ide( action_fn save, action_fn run, action_fn stop, action_fn clear ) :
        m_save{ ATLAS_XORSTR_STATIC( "Save" ), save },
        m_run{ ATLAS_XORSTR_STATIC( "Run" ), run },
        m_stop{ ATLAS_XORSTR_STATIC( "Stop" ), stop },
        m_clear{ ATLAS_XORSTR_STATIC( "Clear" ), clear }
    {
    }

    rect_t ide::toolbar_bounds( ) const
    {
        return rect_t{ this->m_bounds.left, this->m_bounds.top, this->m_bounds.right,
                       this->m_bounds.top + g_gui.scaled( WIDGET_BUTTON_HEIGHT ) };
    }

    usize ide::console_rows( ) const
    {
        return static_cast< usize >( this->m_console_open ? CONSOLE_ROWS_OPEN : CONSOLE_ROWS_SHUT );
    }

    rect_t ide::console_bounds( ) const
    {
        const i32 height{ CONSOLE_HEADER_HEIGHT + static_cast< i32 >( this->console_rows( ) ) * EDITOR_LINE_HEIGHT + EDITOR_PADDING };
        return rect_t{ this->m_bounds.left, this->m_bounds.bottom - height, this->m_bounds.right, this->m_bounds.bottom };
    }

    rect_t ide::console_header( ) const
    {
        const rect_t console{ this->console_bounds( ) };
        return rect_t{ console.left, console.top, console.right, console.top + CONSOLE_HEADER_HEIGHT };
    }

    rect_t ide::editor_bounds( ) const
    {
        return rect_t{ this->m_bounds.left, this->toolbar_bounds( ).bottom + SECTION_GAP, this->m_bounds.right,
                       this->console_bounds( ).top - SECTION_GAP };
    }

    void ide::arrange_buttons( )
    {
        const rect_t toolbar{ this->toolbar_bounds( ) };
        const i32    gap{ g_gui.scaled( WIDGET_BUTTON_GAP ) };
        i32          left{ toolbar.left };

        gui::button *const buttons[ ]{ &this->m_save, &this->m_run, &this->m_stop };

        for ( gui::button *button : buttons )
        {
            const i32 width{ button->measure_width( ) };
            button->set_bounds( rect_t{ left, toolbar.top, left + width, toolbar.bottom } );
            left += width + gap;
        }

        const rect_t header{ this->console_header( ) };
        const i32    right{ header.right - CONSOLE_BUTTON_INSET };

        this->m_clear.set_bounds( rect_t{ right - this->m_clear.measure_width( ), header.top + CONSOLE_BUTTON_INSET, right,
                                          header.bottom - CONSOLE_BUTTON_INSET } );
    }

    i32 ide::measure( ) const
    {
        return this->m_bounds.bottom - this->m_bounds.top;
    }

    void ide::open( string_view name, string_view source )
    {
        this->m_name.assign( name );
        this->m_editor.set_text( source );
        this->m_seen_serial = 0;
    }

    void ide::create( )
    {
        this->m_name.clear( );
        this->m_editor.set_text( ATLAS_XORSTR( "-- paint runs every frame, and draw only works inside it\n"
                                               "atlas.on(\"paint\", function()\n"
                                               "    draw.text(20, 20, color(255, 255, 255), \"hello from \" .. atlas.name())\n"
                                               "end)\n" )
                                     .get( ) );
    }

    void ide::saved( string_view name )
    {
        this->m_name.assign( name );
        this->m_editor.clear_modified( );
    }

    void ide::run( string_view name )
    {
        this->m_name.assign( name );
        g_scripts.request_run( name, this->m_editor.text( ) );
    }

    void ide::stop( )
    {
        if ( !this->m_name.empty( ) )
        {
            g_scripts.request_unload( this->m_name.view( ) );
        }
    }

    void ide::follow_log( )
    {
        const u64 serial{ g_scripts.log_serial( ) };

        if ( serial == this->m_seen_serial )
        {
            return;
        }

        this->m_seen_serial = serial;

        char loaded[ NAME_LENGTH + 16 ]{ };
        str::copy( loaded, sizeof( loaded ), this->m_name.c_str( ) );
        str::append( loaded, sizeof( loaded ), ATLAS_XORSTR( " loaded" ).get( ) );

        usize       marked{ };
        usize       errors{ };
        string_view name{ this->m_name.view( ) };

        g_scripts.visit_log( [ &marked, &errors, name, &loaded ]( const log_line_t &line )
        {
            if ( line.m_kind == log_kind_t::error )
            {
                ++errors;
            }

            if ( line.m_kind == log_kind_t::info && line.m_text.view( ) == string_view{ loaded } )
            {
                marked = 0;
                return;
            }

            if ( line.m_kind != log_kind_t::error || marked )
            {
                return;
            }

            marked = error_line( line.m_text.view( ), name );
        } );

        this->m_editor.mark_line( marked );
        this->m_error_count = errors;
    }

    void ide::think_console( )
    {
        const rect_t console{ this->console_bounds( ) };

        if ( !g_gui.hit_test( console ) )
        {
            return;
        }

        const rect_t header{ this->console_header( ) };

        if ( g_input.pressed( VK_LBUTTON ) && g_gui.hit_test( header ) && !this->m_clear.hovered( ) )
        {
            this->m_console_open = !this->m_console_open;
            return;
        }

        const usize count{ g_scripts.log_count( ) };
        const usize rows{ this->console_rows( ) };

        if ( const i32 wheel{ g_input.wheel( ) } )
        {
            const usize limit{ count > rows ? count - rows : 0 };

            if ( wheel > 0 )
            {
                this->m_console_scroll += static_cast< usize >( wheel ) * EDITOR_WHEEL_LINES;
                this->m_console_scroll = this->m_console_scroll < limit ? this->m_console_scroll : limit;
            } else
            {
                const usize down{ static_cast< usize >( -wheel ) * EDITOR_WHEEL_LINES };
                this->m_console_scroll = this->m_console_scroll > down ? this->m_console_scroll - down : 0;
            }
        }

        if ( !g_input.pressed( VK_LBUTTON ) )
        {
            return;
        }

        const i32   row{ ( g_input.mouse( ).y - header.bottom - EDITOR_PADDING / 2 ) / EDITOR_LINE_HEIGHT };
        const usize shown{ count < rows ? count : rows };
        if ( row < 0 || static_cast< usize >( row ) >= shown )
        {
            return;
        }

        const usize scroll{ this->m_console_scroll < count - shown ? this->m_console_scroll : count - shown };
        const usize target{ count - shown - scroll + static_cast< usize >( row ) };
        usize       index{ };
        usize       line{ };
        string_view name{ this->m_name.view( ) };

        g_scripts.visit_log( [ &index, &line, target, name ]( const log_line_t &entry )
        {
            if ( index++ == target )
            {
                line = error_line( entry.m_text.view( ), name );
            }
        } );

        if ( line )
        {
            this->m_editor.go_to_line( line );
            this->m_editor.mark_line( line );
        }
    }

    void ide::think( )
    {
        this->arrange_buttons( );
        this->m_save.think( );
        this->m_run.think( );
        this->m_stop.think( );
        this->m_clear.think( );

        this->m_editor.set_bounds( this->editor_bounds( ) );
        this->m_editor.think( );

        if ( this->m_editor.take_save_request( ) && !this->m_name.empty( ) )
        {
            g_scripts.request_store( store_kind_t::save, this->m_name.view( ), this->m_editor.text( ) );
        }

        this->follow_log( );
        this->think_console( );
    }

    void ide::draw_toolbar( ) const
    {
        this->m_save.draw( );
        this->m_run.draw( );
        this->m_stop.draw( );

        const rect_t toolbar{ this->toolbar_bounds( ) };
        const i32    text_y{ g_gui.text_center_y( toolbar.top, toolbar.bottom - toolbar.top ) };
        const i32    gap{ g_gui.scaled( MENU_HEADER_GAP ) };

        if ( !this->m_name.empty( ) && g_scripts.is_loaded( this->m_name.view( ) ) )
        {
            g_render.text( MENU_FONT( ), point_t{ this->m_stop.bounds( ).right + gap, text_y }, g_gui.color( g_theme.m_accent ),
                           ATLAS_XORSTR( "running" ).get( ) );
        }

        char position[ 32 ]{ };
        str::format_to( position, sizeof( position ), ATLAS_XORSTR( "Ln {}, Col {}" ).get( ), this->m_editor.caret_line( ),
                        this->m_editor.caret_column( ) );

        g_render.text( MENU_FONT( ), point_t{ toolbar.right, text_y }, g_gui.color( MENU_TEXT_DIM ), position, false,
                       render::text_align::right );

        char label[ NAME_LENGTH + 32 ]{ };
        if ( this->m_name.empty( ) )
        {
            str::copy( label, sizeof( label ), ATLAS_XORSTR( "unsaved script" ).get( ) );
        } else
        {
            str::copy( label, sizeof( label ), this->m_name.c_str( ) );
            str::append( label, sizeof( label ), ATLAS_XORSTR( ".lua" ).get( ) );
        }

        if ( this->m_editor.modified( ) )
        {
            str::append( label, sizeof( label ), ATLAS_XORSTR( " *" ).get( ) );
        }

        const i32 label_right{ toolbar.right - g_render.get_text_width( MENU_FONT( ), position ) - gap * 2 };
        g_render.text( MENU_FONT( ), point_t{ label_right, text_y }, g_gui.color( CONTROL_LABEL ), label, false,
                       render::text_align::right );
    }

    void ide::draw_console( ) const
    {
        const rect_t console{ this->console_bounds( ) };
        const rect_t header{ this->console_header( ) };

        g_render.box( console, g_gui.color( EDITOR_GUTTER ), true );
        g_render.box( console, g_gui.color( EDITOR_BORDER ) );
        g_render.line( point_t{ console.left + 1, header.bottom - 1 }, point_t{ console.right - 2, header.bottom - 1 },
                       g_gui.color( EDITOR_BORDER ) );

        const i32 text_y{ g_gui.text_center_y( header.top, CONSOLE_HEADER_HEIGHT ) };
        const i32 icon_size{ g_gui.scaled( MENU_ICON_SIZE ) };
        i32       cursor{ header.left + EDITOR_PADDING };

        gui::draw_icon_glyph( this->m_console_open ? ICON_CHEVRON_DOWN : ICON_CHEVRON_UP,
                         point_t{ cursor + icon_size / 2, header.top + CONSOLE_HEADER_HEIGHT / 2 }, CONTROL_LABEL );
        cursor += icon_size + g_gui.scaled( INPUT_ICON_GAP );

        const char *title{ ATLAS_XORSTR_STATIC( "Console" ) };
        g_render.text( MENU_FONT( ), point_t{ cursor, text_y }, g_gui.color( CONTROL_VALUE ), title );
        cursor += g_render.get_text_width( MENU_FONT( ), title ) + g_gui.scaled( MENU_HEADER_GAP );

        if ( this->m_error_count )
        {
            char errors[ 32 ]{ };
            str::format_to( errors, sizeof( errors ), this->m_error_count == 1 ? ATLAS_XORSTR( "{} error" ).get( ) : ATLAS_XORSTR( "{} errors" ).get( ),
                            this->m_error_count );

            const i32 width{ g_render.get_text_width( MENU_FONT( ), errors ) + CONSOLE_BADGE_PADDING * 2 };
            g_render.box( rect_t{ cursor, header.top + CONSOLE_BADGE_INSET, cursor + width, header.bottom - CONSOLE_BADGE_INSET },
                          g_gui.color( EDITOR_ERROR_LINE ), true );
            g_render.text( MENU_FONT( ), point_t{ cursor + CONSOLE_BADGE_PADDING, text_y }, g_gui.color( CONSOLE_ERROR ), errors );
        }

        this->m_clear.draw( );

        const rect_t rows_area{ console.left + 1, header.bottom, console.right - 1, console.bottom - 1 };
        g_render.set_clip( rows_area );

        const usize count{ g_scripts.log_count( ) };
        const usize rows{ this->console_rows( ) };
        const usize shown{ count < rows ? count : rows };
        const usize first{ count - shown - ( this->m_console_scroll < count - shown ? this->m_console_scroll : count - shown ) };
        usize       index{ };

        g_scripts.visit_log( [ &index, first, shown, console, rows_area ]( const log_line_t &line )
        {
            const usize slot{ index++ };

            if ( slot < first || slot >= first + shown )
            {
                return;
            }

            color_t value{ EDITOR_TEXT };
            if ( line.m_kind == log_kind_t::error )
            {
                value = CONSOLE_ERROR;
            } else if ( line.m_kind == log_kind_t::info )
            {
                value = CONSOLE_INFO;
            }

            const i32 top{ rows_area.top + EDITOR_PADDING / 2 + static_cast< i32 >( slot - first ) * EDITOR_LINE_HEIGHT };
            g_render.text( MENU_FONT_MONO( ), point_t{ console.left + EDITOR_PADDING, top + 1 }, g_gui.color( value ),
                           line.m_text.view( ) );
        } );

        g_render.reset_clip( );
    }

    void ide::draw( ) const
    {
        this->draw_toolbar( );
        this->m_editor.draw( );
        this->draw_console( );
    }
} // namespace script
