#include "gui.hpp"

#include "input/input.hpp"
#include "render/render.hpp"
#include "runtime/hash.hpp"
#include "runtime/memory.hpp"

namespace gui
{
    constexpr hash_t word_hash( const char *text )
    {
        return hash::make_hash( text, hash::get_length( text ), false );
    }

    ATLAS_FORCE_INLINE color_t word_color( const char *word, usize length, bool member )
    {
        switch ( hash::make_hash( word, length, false ) )
        {
            case word_hash( "and" ):
            case word_hash( "break" ):
            case word_hash( "do" ):
            case word_hash( "else" ):
            case word_hash( "elseif" ):
            case word_hash( "end" ):
            case word_hash( "false" ):
            case word_hash( "for" ):
            case word_hash( "function" ):
            case word_hash( "global" ):
            case word_hash( "goto" ):
            case word_hash( "if" ):
            case word_hash( "in" ):
            case word_hash( "local" ):
            case word_hash( "nil" ):
            case word_hash( "not" ):
            case word_hash( "or" ):
            case word_hash( "repeat" ):
            case word_hash( "return" ):
            case word_hash( "then" ):
            case word_hash( "true" ):
            case word_hash( "until" ):
            case word_hash( "while" ): return EDITOR_KEYWORD;

            case word_hash( "atlas" ):
            case word_hash( "draw" ):
            case word_hash( "input" ):
            case word_hash( "engine" ):
            case word_hash( "entities" ):
            case word_hash( "cvar" ):
            case word_hash( "events" ):
            case word_hash( "buttons" ):
            case word_hash( "vec3" ):
            case word_hash( "color" ):
            case word_hash( "print" ):
            case word_hash( "pairs" ):
            case word_hash( "ipairs" ):
            case word_hash( "next" ):
            case word_hash( "type" ):
            case word_hash( "tostring" ):
            case word_hash( "tonumber" ):
            case word_hash( "pcall" ):
            case word_hash( "xpcall" ):
            case word_hash( "error" ):
            case word_hash( "assert" ):
            case word_hash( "select" ):
            case word_hash( "rawget" ):
            case word_hash( "rawset" ):
            case word_hash( "rawequal" ):
            case word_hash( "rawlen" ):
            case word_hash( "setmetatable" ):
            case word_hash( "getmetatable" ):
            case word_hash( "load" ):
            case word_hash( "string" ):
            case word_hash( "table" ):
            case word_hash( "math" ):
            case word_hash( "utf8" ):
            case word_hash( "coroutine" ):
            case word_hash( "self" ): return member ? EDITOR_TEXT : EDITOR_LIBRARY;

            default: return EDITOR_TEXT;
        }
    }

    ATLAS_FORCE_INLINE bool is_word( char value )
    {
        return str::is_alpha( value ) || str::is_digit( value ) || value == '_';
    }

    ATLAS_FORCE_INLINE bool opens_block( string_view line )
    {
        if ( line.empty( ) )
        {
            return false;
        }

        const char last{ line[ line.size( ) - 1 ] };

        if ( last == '{' || last == '(' )
        {
            return true;
        }

        hash_t last_word{ };
        bool   has_function{ };

        for ( usize index{ }; index < line.size( ); )
        {
            if ( !is_word( line[ index ] ) )
            {
                ++index;
                continue;
            }

            const usize start{ index };
            while ( index < line.size( ) && is_word( line[ index ] ) )
            {
                ++index;
            }

            last_word    = hash::make_hash( line.data( ) + start, index - start, false );
            has_function = has_function || last_word == word_hash( "function" );
        }

        if ( last == ')' )
        {
            return has_function;
        }

        return is_word( last ) && ( last_word == word_hash( "then" ) || last_word == word_hash( "do" ) ||
                                    last_word == word_hash( "else" ) || last_word == word_hash( "repeat" ) );
    }

    ATLAS_FORCE_INLINE bool starts_token( char value )
    {
        return value == ' ' || is_word( value ) || value == '"' || value == '\'' || value == '[' || value == '-' ||
               value == '.';
    }

    template< typename emit_t >
    code_editor::line_state_t code_editor::lex_line( const char *line, usize length, line_state_t state, emit_t &&emit )
    {
        const auto close_long = [ line, length ]( usize from, u8 level ) -> usize
        {
            for ( usize index{ from }; index < length; ++index )
            {
                if ( line[ index ] != ']' )
                {
                    continue;
                }

                usize probe{ index + 1 };
                u8    count{ };

                while ( probe < length && line[ probe ] == '=' )
                {
                    ++probe;
                    ++count;
                }

                if ( count == level && probe < length && line[ probe ] == ']' )
                {
                    return probe + 1;
                }
            }

            return str::npos;
        };

        const auto open_long = [ line, length ]( usize at, usize &after ) -> i32
        {
            if ( at >= length || line[ at ] != '[' )
            {
                return -1;
            }

            usize probe{ at + 1 };
            i32   level{ };

            while ( probe < length && line[ probe ] == '=' )
            {
                ++probe;
                ++level;
            }

            if ( probe >= length || line[ probe ] != '[' )
            {
                return -1;
            }

            after = probe + 1;
            return level;
        };

        usize cursor{ };

        if ( state.m_state != lex_state_t::code )
        {
            const color_t value{ state.m_state == lex_state_t::long_comment ? EDITOR_COMMENT : EDITOR_STRING };
            const usize   end{ close_long( 0, state.m_level ) };

            if ( end == str::npos )
            {
                emit( 0, length, value );
                return state;
            }

            emit( 0, end, value );
            cursor = end;
            state  = line_state_t{ };
        }

        while ( cursor < length )
        {
            const usize start{ cursor };
            const char  value{ line[ cursor ] };

            if ( value == ' ' )
            {
                ++cursor;
                continue;
            }

            if ( value == '-' && cursor + 1 < length && line[ cursor + 1 ] == '-' )
            {
                usize     after{ };
                const i32 level{ open_long( cursor + 2, after ) };

                if ( level < 0 )
                {
                    emit( start, length - start, EDITOR_COMMENT );
                    return state;
                }

                const usize end{ close_long( after, static_cast< u8 >( level ) ) };

                if ( end == str::npos )
                {
                    emit( start, length - start, EDITOR_COMMENT );
                    return line_state_t{ lex_state_t::long_comment, static_cast< u8 >( level ) };
                }

                emit( start, end - start, EDITOR_COMMENT );
                cursor = end;
                continue;
            }

            if ( value == '[' )
            {
                usize     after{ };
                const i32 level{ open_long( cursor, after ) };

                if ( level >= 0 )
                {
                    const usize end{ close_long( after, static_cast< u8 >( level ) ) };

                    if ( end == str::npos )
                    {
                        emit( start, length - start, EDITOR_STRING );
                        return line_state_t{ lex_state_t::long_string, static_cast< u8 >( level ) };
                    }

                    emit( start, end - start, EDITOR_STRING );
                    cursor = end;
                    continue;
                }
            }

            if ( value == '"' || value == '\'' )
            {
                ++cursor;

                while ( cursor < length && line[ cursor ] != value )
                {
                    cursor += line[ cursor ] == '\\' ? 2 : 1;
                }

                cursor = cursor < length ? cursor + 1 : length;
                emit( start, cursor - start, EDITOR_STRING );
                continue;
            }

            if ( str::is_digit( value ) || ( value == '.' && cursor + 1 < length && str::is_digit( line[ cursor + 1 ] ) ) )
            {
                ++cursor;

                while ( cursor < length )
                {
                    const char next{ line[ cursor ] };
                    const char previous{ line[ cursor - 1 ] };
                    const bool exponent_sign{ ( next == '+' || next == '-' ) &&
                                              ( previous == 'e' || previous == 'E' || previous == 'p' || previous == 'P' ) };

                    if ( !is_word( next ) && next != '.' && !exponent_sign )
                    {
                        break;
                    }

                    ++cursor;
                }

                emit( start, cursor - start, EDITOR_NUMBER );
                continue;
            }

            if ( is_word( value ) )
            {
                while ( cursor < length && is_word( line[ cursor ] ) )
                {
                    ++cursor;
                }

                const bool member{ start && ( line[ start - 1 ] == '.' || line[ start - 1 ] == ':' ) };
                emit( start, cursor - start, word_color( line + start, cursor - start, member ) );
                continue;
            }

            ++cursor;
            while ( cursor < length && !starts_token( line[ cursor ] ) )
            {
                ++cursor;
            }

            emit( start, cursor - start, EDITOR_TEXT );
        }

        return state;
    }

    usize code_editor::line_count( ) const
    {
        return this->m_lines.size( );
    }

    usize code_editor::line_start( usize line ) const
    {
        return this->m_lines[ line ];
    }

    usize code_editor::line_end( usize line ) const
    {
        return line + 1 < this->m_lines.size( ) ? this->m_lines[ line + 1 ] - 1 : this->m_text.size( );
    }

    usize code_editor::line_of( usize offset ) const
    {
        usize low{ };
        usize high{ this->m_lines.size( ) };

        while ( high - low > 1 )
        {
            const usize middle{ ( low + high ) / 2 };

            if ( this->m_lines[ middle ] <= offset )
            {
                low = middle;
            } else
            {
                high = middle;
            }
        }

        return low;
    }

    i32 code_editor::gutter_width( ) const
    {
        i32 digits{ 2 };

        for ( usize count{ this->line_count( ) }; count >= 100; count /= 10 )
        {
            ++digits;
        }

        return digits * this->m_char_width + EDITOR_PADDING * 2;
    }

    rect_t code_editor::text_area( ) const
    {
        return rect_t{ this->m_bounds.left + this->gutter_width( ) + EDITOR_PADDING, this->m_bounds.top + EDITOR_PADDING / 2,
                       this->m_bounds.right - EDITOR_PADDING, this->m_bounds.bottom - EDITOR_PADDING / 2 };
    }

    usize code_editor::visible_lines( ) const
    {
        const rect_t area{ this->text_area( ) };
        const i32    rows{ ( area.bottom - area.top ) / EDITOR_LINE_HEIGHT };
        return rows > 0 ? static_cast< usize >( rows ) : 1;
    }

    usize code_editor::visible_columns( ) const
    {
        const rect_t area{ this->text_area( ) };
        const i32    columns{ this->m_char_width ? ( area.right - area.left ) / this->m_char_width : 0 };
        return columns > 0 ? static_cast< usize >( columns ) : 1;
    }

    usize code_editor::offset_at( point_t point ) const
    {
        const rect_t area{ this->text_area( ) };
        const i32    row{ point.y < area.top ? -1 : ( point.y - area.top ) / EDITOR_LINE_HEIGHT };

        usize line{ this->m_scroll_line };
        if ( row < 0 )
        {
            line = line ? line - 1 : 0;
        } else
        {
            line += static_cast< usize >( row );
        }

        if ( line >= this->line_count( ) )
        {
            return this->m_text.size( );
        }

        const i32 x{ point.x - area.left + this->m_char_width / 2 };
        usize     column{ this->m_scroll_column + ( x > 0 && this->m_char_width ? static_cast< usize >( x / this->m_char_width ) : 0 ) };

        const usize length{ this->line_end( line ) - this->line_start( line ) };
        return this->line_start( line ) + ( column < length ? column : length );
    }

    void code_editor::selection( usize &first, usize &last ) const
    {
        first = this->m_caret < this->m_anchor ? this->m_caret : this->m_anchor;
        last  = this->m_caret < this->m_anchor ? this->m_anchor : this->m_caret;
    }

    void code_editor::apply( usize position, usize removed, const char *inserted, usize inserted_length )
    {
        const usize old_size{ this->m_text.size( ) };
        const usize tail{ old_size - position - removed };
        const usize new_size{ old_size - removed + inserted_length };

        if ( new_size > old_size && !this->m_text.resize( new_size ) )
        {
            return;
        }

        char *data{ this->m_text.data( ) };
        memmove( data + position + inserted_length, data + position + removed, tail );
        memcpy( data + position, inserted, inserted_length );

        if ( new_size < old_size )
        {
            this->m_text.resize( new_size );
        }

        this->m_modified = true;
        this->relex( );
    }

    void code_editor::relex( )
    {
        this->m_lines.clear( );
        this->m_lines.push_back( 0 );

        for ( usize index{ }; index < this->m_text.size( ); ++index )
        {
            if ( this->m_text[ index ] == '\n' )
            {
                this->m_lines.push_back( static_cast< u32 >( index + 1 ) );
            }
        }

        this->m_states.resize( this->m_lines.size( ) );

        line_state_t state{ };
        for ( usize line{ }; line < this->m_lines.size( ); ++line )
        {
            this->m_states[ line ] = state;

            const usize start{ this->line_start( line ) };
            state = lex_line( this->m_text.data( ) + start, this->line_end( line ) - start, state, []( usize, usize, color_t ) { } );
        }
    }

    void code_editor::replace( usize position, usize removed, string_view inserted, bool typing )
    {
        constexpr usize HISTORY_LIMIT{ 8u * 1024u * 1024u };

        if ( this->m_history_cursor < this->m_history.size( ) )
        {
            this->m_history_text.resize( this->m_history[ this->m_history_cursor ].m_offset );
            this->m_history.resize( this->m_history_cursor );
        }

        if ( this->m_history_text.size( ) > HISTORY_LIMIT )
        {
            this->m_history.clear( );
            this->m_history_text.clear( );
            this->m_history_cursor = 0;
        }

        edit_record_t *last{ this->m_history.empty( ) ? nullptr : &this->m_history[ this->m_history.size( ) - 1 ] };

        const bool extends{ typing && last && last->m_typing && !removed &&
                            last->m_position + last->m_inserted == position };

        if ( extends )
        {
            last->m_inserted += static_cast< u32 >( inserted.size( ) );
        } else
        {
            edit_record_t record{ };
            record.m_position      = static_cast< u32 >( position );
            record.m_removed       = static_cast< u32 >( removed );
            record.m_inserted      = static_cast< u32 >( inserted.size( ) );
            record.m_offset        = static_cast< u32 >( this->m_history_text.size( ) );
            record.m_caret_before  = static_cast< u32 >( this->m_caret );
            record.m_anchor_before = static_cast< u32 >( this->m_anchor );
            record.m_typing        = typing;

            this->m_history.push_back( record );

            const usize kept{ this->m_history_text.size( ) };
            this->m_history_text.resize( kept + removed );
            memcpy( this->m_history_text.data( ) + kept, this->m_text.data( ) + position, removed );
        }

        const usize kept{ this->m_history_text.size( ) };
        this->m_history_text.resize( kept + inserted.size( ) );
        memcpy( this->m_history_text.data( ) + kept, inserted.data( ), inserted.size( ) );
        this->m_history_cursor = this->m_history.size( );

        this->apply( position, removed, inserted.data( ), inserted.size( ) );
    }

    void code_editor::insert( string_view text, bool typing )
    {
        usize first{ };
        usize last{ };
        this->selection( first, last );

        this->replace( first, last - first, text, typing && first == last );
        this->m_caret       = first + text.size( );
        this->m_anchor      = this->m_caret;
        this->m_goal_column = this->m_caret - this->line_start( this->line_of( this->m_caret ) );
        this->keep_caret_visible( );
    }

    void code_editor::undo( )
    {
        if ( !this->m_history_cursor )
        {
            return;
        }

        const edit_record_t record{ this->m_history[ --this->m_history_cursor ] };
        this->apply( record.m_position, record.m_inserted, this->m_history_text.data( ) + record.m_offset, record.m_removed );

        this->m_caret  = record.m_caret_before;
        this->m_anchor = record.m_anchor_before;
        this->keep_caret_visible( );
    }

    void code_editor::redo( )
    {
        if ( this->m_history_cursor >= this->m_history.size( ) )
        {
            return;
        }

        const edit_record_t record{ this->m_history[ this->m_history_cursor++ ] };
        this->apply( record.m_position, record.m_removed, this->m_history_text.data( ) + record.m_offset + record.m_removed,
                     record.m_inserted );

        this->m_caret  = record.m_position + record.m_inserted;
        this->m_anchor = this->m_caret;
        this->keep_caret_visible( );
    }

    void code_editor::set_text( string_view text )
    {
        this->m_text.clear( );
        this->m_text.reserve( text.size( ) );

        for ( const char value : text )
        {
            if ( value == '\t' )
            {
                for ( i32 space{ }; space < EDITOR_TAB_WIDTH; ++space )
                {
                    this->m_text.push_back( ' ' );
                }
            } else if ( value != '\r' )
            {
                this->m_text.push_back( value );
            }
        }

        this->m_history.clear( );
        this->m_history_text.clear( );
        this->m_history_cursor = 0;
        this->m_caret          = 0;
        this->m_anchor         = 0;
        this->m_goal_column    = 0;
        this->m_scroll_line    = 0;
        this->m_scroll_column  = 0;
        this->m_marked_line    = 0;
        this->m_modified       = false;
        this->relex( );
    }

    void code_editor::go_to_line( usize line )
    {
        const usize target{ line && line <= this->line_count( ) ? line - 1 : 0 };
        this->move_caret( this->line_start( target ), false );
    }

    void code_editor::move_caret( usize offset, bool extend )
    {
        this->m_caret = offset < this->m_text.size( ) ? offset : this->m_text.size( );

        if ( !extend )
        {
            this->m_anchor = this->m_caret;
        }

        this->m_goal_column = this->m_caret - this->line_start( this->line_of( this->m_caret ) );
        this->m_caret_tick  = clock( );
        this->keep_caret_visible( );
    }

    void code_editor::keep_caret_visible( )
    {
        const usize line{ this->line_of( this->m_caret ) };
        const usize column{ this->m_caret - this->line_start( line ) };
        const usize rows{ this->visible_lines( ) };
        const usize columns{ this->visible_columns( ) };

        if ( line < this->m_scroll_line )
        {
            this->m_scroll_line = line;
        } else if ( line >= this->m_scroll_line + rows )
        {
            this->m_scroll_line = line - rows + 1;
        }

        if ( column < this->m_scroll_column )
        {
            this->m_scroll_column = column;
        } else if ( column >= this->m_scroll_column + columns )
        {
            this->m_scroll_column = column - columns + 1;
        }
    }

    void code_editor::indent_lines( bool outdent )
    {
        usize first{ };
        usize last{ };
        this->selection( first, last );

        const usize first_line{ this->line_of( first ) };
        usize       last_line{ this->line_of( last ) };

        if ( last_line > first_line && last == this->line_start( last_line ) )
        {
            --last_line;
        }

        for ( usize line{ first_line }; line <= last_line; ++line )
        {
            const usize start{ this->line_start( line ) };

            if ( !outdent )
            {
                this->replace( start, 0, string_view{ "    ", static_cast< usize >( EDITOR_TAB_WIDTH ) }, false );
                continue;
            }

            usize spaces{ };
            while ( spaces < static_cast< usize >( EDITOR_TAB_WIDTH ) && start + spaces < this->line_end( line ) &&
                    this->m_text[ start + spaces ] == ' ' )
            {
                ++spaces;
            }

            if ( spaces )
            {
                this->replace( start, spaces, { }, false );
            }
        }

        this->m_anchor = this->line_start( first_line );
        this->m_caret  = this->line_end( last_line );
        this->keep_caret_visible( );
    }

    void code_editor::copy_selection( bool cut )
    {
        usize first{ };
        usize last{ };
        this->selection( first, last );

        if ( first == last )
        {
            return;
        }

        usize newlines{ };
        for ( usize index{ first }; index < last; ++index )
        {
            newlines += this->m_text[ index ] == '\n';
        }

        HGLOBAL memory{ I( GlobalAlloc )( GMEM_MOVEABLE, last - first + newlines + 1 ) };
        char   *clipboard{ memory ? static_cast< char * >( I( GlobalLock )( memory ) ) : nullptr };

        if ( !clipboard )
        {
            if ( memory )
            {
                I( GlobalFree )( memory );
            }

            return;
        }

        usize written{ };
        for ( usize index{ first }; index < last; ++index )
        {
            if ( this->m_text[ index ] == '\n' )
            {
                clipboard[ written++ ] = '\r';
            }

            clipboard[ written++ ] = this->m_text[ index ];
        }

        clipboard[ written ] = '\0';
        I( GlobalUnlock )( memory );

        bool copied{ };
        if ( I( OpenClipboard )( static_cast< HWND >( g_input.window( ) ) ) )
        {
            copied = I( EmptyClipboard )( ) && I( SetClipboardData )( CF_TEXT, memory ) != nullptr;
            I( CloseClipboard )( );
        }

        if ( !copied )
        {
            I( GlobalFree )( memory );
            return;
        }

        if ( cut )
        {
            this->insert( { }, false );
        }
    }

    void code_editor::paste( )
    {
        if ( !I( OpenClipboard )( static_cast< HWND >( g_input.window( ) ) ) )
        {
            return;
        }

        atl::vector< char > text{ };
        HANDLE              memory{ I( GetClipboardData )( CF_TEXT ) };
        const char         *clipboard{ memory ? static_cast< const char * >( I( GlobalLock )( memory ) ) : nullptr };

        if ( clipboard )
        {
            for ( const char *cursor{ clipboard }; *cursor; ++cursor )
            {
                if ( *cursor == '\t' )
                {
                    for ( i32 space{ }; space < EDITOR_TAB_WIDTH; ++space )
                    {
                        text.push_back( ' ' );
                    }
                } else if ( *cursor == '\n' || static_cast< u8 >( *cursor ) >= 0x20 )
                {
                    text.push_back( *cursor );
                }
            }

            I( GlobalUnlock )( memory );
        }

        I( CloseClipboard )( );

        if ( !text.empty( ) )
        {
            this->insert( string_view{ text.data( ), text.size( ) }, false );
        }
    }

    void code_editor::think_mouse( )
    {
        const bool    hovered{ g_gui.hit_test( this->m_bounds ) };
        const point_t mouse{ g_input.mouse( ) };

        this->m_hovered = hovered;
        if ( hovered )
        {
            this->set_cursor( );
        }

        if ( hovered && g_input.wheel( ) )
        {
            const i32   lines{ g_input.wheel( ) * EDITOR_WHEEL_LINES };
            const usize last{ this->line_count( ) ? this->line_count( ) - 1 : 0 };

            if ( lines > 0 )
            {
                this->m_scroll_line = this->m_scroll_line > static_cast< usize >( lines ) ? this->m_scroll_line - lines : 0;
            } else
            {
                this->m_scroll_line += static_cast< usize >( -lines );
                this->m_scroll_line = this->m_scroll_line < last ? this->m_scroll_line : last;
            }
        }

        if ( g_input.pressed( VK_LBUTTON ) )
        {
            if ( !hovered )
            {
                if ( this->m_focused )
                {
                    this->m_focused = false;
                    g_gui.set_focused_input( nullptr );
                }

                return;
            }

            if ( !this->m_focused )
            {
                this->m_focused = true;
                g_gui.set_focused_input( this );
                g_gui.set_listening( nullptr );
            }

            const usize offset{ this->offset_at( mouse ) };
            const u64   now{ clock( ) };

            if ( offset == this->m_click_offset && now - this->m_click_tick < EDITOR_DOUBLE_CLICK )
            {
                usize first{ offset };
                usize last{ offset };

                while ( first && is_word( this->m_text[ first - 1 ] ) )
                {
                    --first;
                }

                while ( last < this->m_text.size( ) && is_word( this->m_text[ last ] ) )
                {
                    ++last;
                }

                this->m_anchor = first;
                this->move_caret( last, true );
                this->m_click_tick = 0;
                return;
            }

            this->move_caret( offset, g_input.down( VK_SHIFT ) );
            this->m_click_tick   = now;
            this->m_click_offset = offset;
            this->m_dragging     = true;
            return;
        }

        if ( this->m_dragging )
        {
            if ( !g_input.down( VK_LBUTTON ) )
            {
                this->m_dragging = false;
                return;
            }

            this->move_caret( this->offset_at( mouse ), true );
        }
    }

    void code_editor::think_keys( )
    {
        const bool shift{ g_input.down( VK_SHIFT ) };
        const bool control{ g_input.down( VK_CONTROL ) };

        usize first{ };
        usize last{ };

        for ( usize index{ }; index < g_input.typed_count( ); ++index )
        {
            const char value{ g_input.typed( index ) };
            this->selection( first, last );

            switch ( value )
            {
                case 0x01:
                {
                    this->m_anchor = 0;
                    this->move_caret( this->m_text.size( ), true );
                    break;
                }

                case 0x03: this->copy_selection( false ); break;
                case 0x18: this->copy_selection( true ); break;
                case 0x16: this->paste( ); break;
                case 0x13: this->m_save_requested = true; break;
                case 0x19: this->redo( ); break;

                case 0x1A:
                {
                    if ( shift )
                    {
                        this->redo( );
                    } else
                    {
                        this->undo( );
                    }
                    break;
                }

                case 0x1B:
                {
                    this->m_focused = false;
                    g_gui.set_focused_input( nullptr );
                    return;
                }

                case '\r':
                {
                    const usize line{ this->line_of( first ) };
                    const usize start{ this->line_start( line ) };
                    usize       indent{ };

                    while ( start + indent < first && this->m_text[ start + indent ] == ' ' )
                    {
                        ++indent;
                    }

                    usize tail{ first };
                    while ( tail > start && this->m_text[ tail - 1 ] == ' ' )
                    {
                        --tail;
                    }

                    if ( opens_block( string_view{ this->m_text.data( ) + start, tail - start } ) )
                    {
                        indent += static_cast< usize >( EDITOR_TAB_WIDTH );
                    }

                    char buffer[ 256 ]{ '\n' };
                    indent = indent < sizeof( buffer ) - 1 ? indent : sizeof( buffer ) - 1;
                    memset( buffer + 1, ' ', indent );
                    this->insert( string_view{ buffer, indent + 1 }, false );
                    break;
                }

                case '\b':
                {
                    if ( first != last )
                    {
                        this->insert( { }, false );
                        break;
                    }

                    if ( !first )
                    {
                        break;
                    }

                    const usize start{ this->line_start( this->line_of( first ) ) };
                    const usize column{ first - start };
                    usize       count{ 1 };

                    bool only_spaces{ column > 0 };
                    for ( usize index{ start }; index < first && only_spaces; ++index )
                    {
                        only_spaces = this->m_text[ index ] == ' ';
                    }

                    if ( only_spaces )
                    {
                        count = ( column - 1 ) % static_cast< usize >( EDITOR_TAB_WIDTH ) + 1;
                    }

                    this->m_anchor = first - count;
                    this->insert( { }, false );
                    break;
                }

                case 0x7F:
                {
                    if ( first != last )
                    {
                        this->insert( { }, false );
                        break;
                    }

                    usize start{ first };
                    while ( start && this->m_text[ start - 1 ] == ' ' )
                    {
                        --start;
                    }

                    while ( start && is_word( this->m_text[ start - 1 ] ) )
                    {
                        --start;
                    }

                    if ( start == first && start )
                    {
                        --start;
                    }

                    this->m_anchor = start;
                    this->insert( { }, false );
                    break;
                }

                case '\t':
                {
                    if ( shift || this->line_of( first ) != this->line_of( last ) )
                    {
                        this->indent_lines( shift );
                        break;
                    }

                    const usize column{ first - this->line_start( this->line_of( first ) ) };
                    const usize spaces{ static_cast< usize >( EDITOR_TAB_WIDTH ) - column % static_cast< usize >( EDITOR_TAB_WIDTH ) };
                    this->insert( string_view{ "    ", spaces }, false );
                    break;
                }

                default:
                {
                    if ( static_cast< u8 >( value ) >= 0x20 && static_cast< u8 >( value ) < 0x7F )
                    {
                        this->insert( string_view{ &value, 1 }, value != ' ' );
                    }
                    break;
                }
            }
        }

        this->selection( first, last );

        const usize line{ this->line_of( this->m_caret ) };
        const usize start{ this->line_start( line ) };
        const usize end{ this->line_end( line ) };

        if ( g_input.repeated( VK_LEFT ) )
        {
            usize target{ first != last && !shift ? first : ( this->m_caret ? this->m_caret - 1 : 0 ) };

            if ( control )
            {
                target = this->m_caret;
                while ( target && this->m_text[ target - 1 ] == ' ' )
                {
                    --target;
                }

                while ( target && is_word( this->m_text[ target - 1 ] ) )
                {
                    --target;
                }

                if ( target == this->m_caret && target )
                {
                    --target;
                }
            }

            this->move_caret( target, shift );
        }

        if ( g_input.repeated( VK_RIGHT ) )
        {
            usize target{ first != last && !shift ? last : this->m_caret + 1 };

            if ( control )
            {
                target = this->m_caret;
                while ( target < this->m_text.size( ) && is_word( this->m_text[ target ] ) )
                {
                    ++target;
                }

                while ( target < this->m_text.size( ) && this->m_text[ target ] == ' ' )
                {
                    ++target;
                }

                if ( target == this->m_caret )
                {
                    ++target;
                }
            }

            this->move_caret( target, shift );
        }

        const auto vertical = [ this, shift, line ]( i64 rows )
        {
            const i64   target_line{ static_cast< i64 >( line ) + rows };
            const usize clamped{ target_line < 0 ? 0 : ( target_line >= static_cast< i64 >( this->line_count( ) ) ? this->line_count( ) - 1 : static_cast< usize >( target_line ) ) };
            const usize length{ this->line_end( clamped ) - this->line_start( clamped ) };
            const usize goal{ this->m_goal_column };

            this->move_caret( this->line_start( clamped ) + ( goal < length ? goal : length ), shift );
            this->m_goal_column = goal;
        };

        const i64 page{ static_cast< i64 >( this->visible_lines( ) ) };

        if ( g_input.repeated( VK_UP ) )
        {
            vertical( -1 );
        }

        if ( g_input.repeated( VK_DOWN ) )
        {
            vertical( 1 );
        }

        if ( g_input.repeated( VK_PRIOR ) )
        {
            vertical( -page );
        }

        if ( g_input.repeated( VK_NEXT ) )
        {
            vertical( page );
        }

        if ( g_input.repeated( VK_HOME ) )
        {
            usize indent{ start };
            while ( indent < end && this->m_text[ indent ] == ' ' )
            {
                ++indent;
            }

            this->move_caret( control ? 0 : ( this->m_caret == indent ? start : indent ), shift );
        }

        if ( g_input.repeated( VK_END ) )
        {
            this->move_caret( control ? this->m_text.size( ) : end, shift );
        }

        if ( g_input.repeated( VK_DELETE ) )
        {
            if ( first != last )
            {
                this->insert( { }, false );
            } else if ( this->m_caret < this->m_text.size( ) )
            {
                this->m_anchor = this->m_caret + 1;
                this->insert( { }, false );
            }
        }
    }

    i32 code_editor::measure( ) const
    {
        return this->m_bounds.bottom - this->m_bounds.top;
    }

    void code_editor::set_cursor( ) const
    {
        g_gui.set_cursor_state( CURSOR_STATE_TEXT_BEAM );
    }

    void code_editor::think( )
    {
        if ( this->m_lines.empty( ) )
        {
            this->relex( );
        }

        if ( !this->m_char_width )
        {
            this->m_char_width = g_render.get_text_width( MENU_FONT_MONO( ), ATLAS_XORSTR( "0000000000" ).get( ) ) / 10;
        }

        if ( !this->m_char_width )
        {
            return;
        }

        this->m_focused = g_gui.focused_input( ) == this;

        if ( this->m_focused && !g_gui.can_interact( ) )
        {
            this->m_focused = false;
            g_gui.set_focused_input( nullptr );
        }

        this->think_mouse( );

        if ( this->m_focused )
        {
            this->think_keys( );
        }
    }

    void code_editor::draw_line( usize line, i32 top, i32 text_left, usize first_column, usize columns ) const
    {
        const usize begin{ this->line_start( line ) };
        const usize length{ this->line_end( line ) - begin };
        const char *data{ this->m_text.data( ) + begin };
        const usize last_column{ first_column + columns };
        const i32   width{ this->m_char_width };

        lex_line( data, length, this->m_states[ line ],
                  [ data, top, text_left, first_column, last_column, width ]( usize start, usize count, color_t value )
        {
            const usize from{ start > first_column ? start : first_column };
            const usize to{ start + count < last_column ? start + count : last_column };

            if ( from >= to )
            {
                return;
            }

            g_render.text( MENU_FONT_MONO( ),
                           point_t{ text_left + static_cast< i32 >( from - first_column ) * width, top },
                           g_gui.color( value ), string_view{ data + from, to - from } );
        } );
    }

    void code_editor::draw( ) const
    {
        g_render.box( this->m_bounds, g_gui.color( EDITOR_BACKGROUND ), true );

        if ( !this->m_char_width || this->m_lines.empty( ) )
        {
            return;
        }

        const rect_t area{ this->text_area( ) };
        const i32    gutter_right{ this->m_bounds.left + this->gutter_width( ) };
        const usize  rows{ this->visible_lines( ) };
        const usize  columns{ this->visible_columns( ) };
        const usize  caret_line{ this->line_of( this->m_caret ) };

        g_render.box( rect_t{ this->m_bounds.left, this->m_bounds.top, gutter_right, this->m_bounds.bottom },
                      g_gui.color( EDITOR_GUTTER ), true );

        usize first{ };
        usize last{ };
        this->selection( first, last );

        for ( usize row{ }; row < rows && this->m_scroll_line + row < this->line_count( ); ++row )
        {
            const usize  line{ this->m_scroll_line + row };
            const i32    top{ area.top + static_cast< i32 >( row ) * EDITOR_LINE_HEIGHT };
            const rect_t band{ gutter_right, top, this->m_bounds.right, top + EDITOR_LINE_HEIGHT };

            if ( line + 1 == this->m_marked_line )
            {
                g_render.box( band, g_gui.color( EDITOR_ERROR_LINE ), true );
            } else if ( this->m_focused && line == caret_line && first == last )
            {
                g_render.box( band, g_gui.color( EDITOR_CURRENT_LINE ), true );
            }

            const usize start{ this->line_start( line ) };
            const usize end{ this->line_end( line ) };

            if ( first < last && first <= end && last > start )
            {
                const usize from{ ( first > start ? first : start ) - start };
                const usize to{ ( last < end ? last : end ) - start + ( last > end ? 1 : 0 ) };
                const i32   left{ area.left + ( static_cast< i32 >( from ) - static_cast< i32 >( this->m_scroll_column ) ) * this->m_char_width };
                const i32   right{ area.left + ( static_cast< i32 >( to ) - static_cast< i32 >( this->m_scroll_column ) ) * this->m_char_width };

                if ( right > area.left && left < area.right )
                {
                    g_render.box( rect_t{ left > area.left ? left : area.left, top, right < area.right ? right : area.right, top + EDITOR_LINE_HEIGHT },
                                  g_gui.color( EDITOR_SELECTION ), true );
                }
            }

            char  number[ 12 ]{ };
            usize digits{ };
            for ( usize value{ line + 1 }; value; value /= 10 )
            {
                number[ digits++ ] = static_cast< char >( '0' + value % 10 );
            }

            for ( usize index{ }; index < digits / 2; ++index )
            {
                const char swapped{ number[ index ] };
                number[ index ]              = number[ digits - 1 - index ];
                number[ digits - 1 - index ] = swapped;
            }

            g_render.text( MENU_FONT_MONO( ), point_t{ gutter_right - EDITOR_PADDING, top + 1 },
                           g_gui.color( line == caret_line ? CONTROL_VALUE : EDITOR_GUTTER_TEXT ), string_view{ number, digits },
                           false, render::text_align::right );
        }

        g_render.set_clip( area );

        for ( usize row{ }; row < rows && this->m_scroll_line + row < this->line_count( ); ++row )
        {
            this->draw_line( this->m_scroll_line + row, area.top + static_cast< i32 >( row ) * EDITOR_LINE_HEIGHT + 1, area.left,
                             this->m_scroll_column, columns );
        }

        const u64 since{ clock( ) - this->m_caret_tick };
        if ( this->m_focused && ( since < INPUT_CARET_SOLID || since % INPUT_CARET_PERIOD < INPUT_CARET_PERIOD / 2 ) &&
             caret_line >= this->m_scroll_line && caret_line < this->m_scroll_line + rows )
        {
            const usize column{ this->m_caret - this->line_start( caret_line ) };
            const i32   x{ area.left + ( static_cast< i32 >( column ) - static_cast< i32 >( this->m_scroll_column ) ) * this->m_char_width };
            const i32   top{ area.top + static_cast< i32 >( caret_line - this->m_scroll_line ) * EDITOR_LINE_HEIGHT };

            g_render.box( rect_t{ x, top + 1, x + 1, top + EDITOR_LINE_HEIGHT - 1 }, g_gui.color( EDITOR_CARET ), true );
        }

        g_render.reset_clip( );

        if ( this->line_count( ) > rows )
        {
            const i32 track{ this->m_bounds.bottom - this->m_bounds.top };
            const i32 thumb{ track * static_cast< i32 >( rows ) / static_cast< i32 >( this->line_count( ) ) };
            const i32 offset{ track * static_cast< i32 >( this->m_scroll_line ) / static_cast< i32 >( this->line_count( ) ) };
            const i32 top{ this->m_bounds.top + offset };

            g_render.box( rect_t{ this->m_bounds.right - SCROLLBAR_WIDTH - SCROLLBAR_INSET, top,
                                  this->m_bounds.right - SCROLLBAR_INSET, top + ( thumb > 8 ? thumb : 8 ) },
                          g_gui.color( SCROLLBAR_THUMB ), true );
        }

        g_render.box( this->m_bounds, g_gui.color( EDITOR_BORDER ) );
    }
} // namespace gui
