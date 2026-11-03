#include "json.hpp"

#include "runtime/assert.hpp"

constexpr f64 POWERS_OF_TEN[ 23 ]{
    1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,
    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22
};

constexpr u64 INTEGER_POWERS_OF_TEN[ 20 ]{
    1ull, 10ull, 100ull, 1000ull, 10000ull, 100000ull, 1000000ull, 10000000ull, 100000000ull, 1000000000ull,
    10000000000ull, 100000000000ull, 1000000000000ull, 10000000000000ull, 100000000000000ull, 1000000000000000ull,
    10000000000000000ull, 100000000000000000ull, 1000000000000000000ull, 10000000000000000000ull
};

constexpr u64 MANTISSA_LIMIT{ ( 0xFFFFFFFFFFFFFFFFull - 9ull ) / 10ull }; // one more digit still fits
constexpr u64 EXACT_LIMIT{ 1ull << 53u };                                 // above this a double skips integers
constexpr u64 SIGNED_LIMIT{ 9223372036854775807ull };
constexpr f64 DOUBLE_LIMIT{ 1.7976931348623157e308 }; // strictly above this is infinity
constexpr f64 SIGNED_SCALE{ 9223372036854775808.0 };  // 2^63
constexpr f64 SCALE_CEILING{ 9.0e18 };                // largest product the digit scaling may reach
constexpr u32 MAX_DECIMALS{ 19 };

static bool make_real( u64 mantissa, i32 exponent, f64 &out )
{
    if ( !mantissa )
    {
        out = 0.0;
        return true;
    }

    f64 value{ static_cast< f64 >( mantissa ) };

    if ( mantissa <= EXACT_LIMIT && exponent >= -22 && exponent <= 22 )
    {
        out = exponent >= 0 ? value * POWERS_OF_TEN[ exponent ] : value / POWERS_OF_TEN[ -exponent ];
        return true;
    }

    for ( i32 remaining{ exponent }; remaining > 0; --remaining )
    {
        value *= 10.0;

        if ( value > DOUBLE_LIMIT )
        {
            return false;
        }
    }

    for ( i32 remaining{ exponent }; remaining < 0 && value != 0.0; ++remaining )
    {
        value /= 10.0;
    }

    out = value;
    return true;
}

namespace json
{
    namespace detail
    {
        struct parser
        {
            document   &m_document;
            const char *m_begin{ };
            const char *m_cursor{ };
            const char *m_end{ };
            usize       m_depth{ };

            bool fail( error_t code, const char *at )
            {
                this->m_document.m_error        = code;
                this->m_document.m_error_offset = static_cast< usize >( at - this->m_begin );
                return false;
            }

            bool left( usize count ) const
            {
                return static_cast< usize >( this->m_end - this->m_cursor ) >= count;
            }

            void skip_whitespace( )
            {
                while ( this->m_cursor < this->m_end )
                {
                    const char current{ *this->m_cursor };

                    if ( current != ' ' && current != '\t' && current != '\n' && current != '\r' )
                    {
                        return;
                    }

                    ++this->m_cursor;
                }
            }

            u32 add_node( )
            {
                if ( this->m_document.m_nodes.size( ) >= INVALID_NODE )
                {
                    return INVALID_NODE;
                }

                if ( !this->m_document.m_nodes.emplace_back( ) )
                {
                    return INVALID_NODE;
                }

                return static_cast< u32 >( this->m_document.m_nodes.size( ) - 1 );
            }

            bool push_byte( char value )
            {
                return this->m_document.m_pool.push_back( value ) ||
                       this->fail( error_t::allocation_failed, this->m_cursor );
            }

            bool push_code_point( u32 code )
            {
                if ( code < 0x80u )
                {
                    return this->push_byte( static_cast< char >( code ) );
                }

                if ( code < 0x800u )
                {
                    return this->push_byte( static_cast< char >( 0xC0u | ( code >> 6u ) ) ) &&
                           this->push_byte( static_cast< char >( 0x80u | ( code & 0x3Fu ) ) );
                }

                if ( code < 0x10000u )
                {
                    return this->push_byte( static_cast< char >( 0xE0u | ( code >> 12u ) ) ) &&
                           this->push_byte( static_cast< char >( 0x80u | ( ( code >> 6u ) & 0x3Fu ) ) ) &&
                           this->push_byte( static_cast< char >( 0x80u | ( code & 0x3Fu ) ) );
                }

                return this->push_byte( static_cast< char >( 0xF0u | ( code >> 18u ) ) ) &&
                       this->push_byte( static_cast< char >( 0x80u | ( ( code >> 12u ) & 0x3Fu ) ) ) &&
                       this->push_byte( static_cast< char >( 0x80u | ( ( code >> 6u ) & 0x3Fu ) ) ) &&
                       this->push_byte( static_cast< char >( 0x80u | ( code & 0x3Fu ) ) );
            }

            bool hex_quad( u32 &out )
            {
                if ( !this->left( 4 ) )
                {
                    return false;
                }

                u32 code{ };

                for ( usize index{ }; index < 4; ++index )
                {
                    const char digit{ this->m_cursor[ index ] };
                    u32        nibble{ };

                    if ( digit >= '0' && digit <= '9' )
                    {
                        nibble = static_cast< u32 >( digit - '0' );
                    } else if ( digit >= 'a' && digit <= 'f' )
                    {
                        nibble = static_cast< u32 >( digit - 'a' ) + 10u;
                    } else if ( digit >= 'A' && digit <= 'F' )
                    {
                        nibble = static_cast< u32 >( digit - 'A' ) + 10u;
                    } else
                    {
                        return false;
                    }

                    code = ( code << 4u ) | nibble;
                }

                this->m_cursor += 4;
                out = code;
                return true;
            }

            bool parse_unicode( const char *at )
            {
                u32 code{ };

                if ( !this->hex_quad( code ) )
                {
                    return this->fail( error_t::bad_escape, at );
                }

                if ( code >= 0xDC00u && code <= 0xDFFFu )
                {
                    return this->fail( error_t::bad_escape, at ); // low surrogate with no lead
                }

                if ( code >= 0xD800u && code <= 0xDBFFu )
                {
                    if ( !this->left( 2 ) || this->m_cursor[ 0 ] != '\\' || this->m_cursor[ 1 ] != 'u' )
                    {
                        return this->fail( error_t::bad_escape, at );
                    }

                    this->m_cursor += 2;

                    u32 trail{ };

                    if ( !this->hex_quad( trail ) || trail < 0xDC00u || trail > 0xDFFFu )
                    {
                        return this->fail( error_t::bad_escape, at );
                    }

                    code = 0x10000u + ( ( code - 0xD800u ) << 10u ) + ( trail - 0xDC00u );
                }

                return this->push_code_point( code );
            }

            bool parse_escape( )
            {
                const char *at{ this->m_cursor };

                ++this->m_cursor; // backslash

                if ( this->m_cursor >= this->m_end )
                {
                    return this->fail( error_t::unterminated, at );
                }

                const char code{ *this->m_cursor };
                ++this->m_cursor;

                switch ( code )
                {
                    case '"': return this->push_byte( '"' );
                    case '\\': return this->push_byte( '\\' );
                    case '/': return this->push_byte( '/' );
                    case 'b': return this->push_byte( '\b' );
                    case 'f': return this->push_byte( '\f' );
                    case 'n': return this->push_byte( '\n' );
                    case 'r': return this->push_byte( '\r' );
                    case 't': return this->push_byte( '\t' );
                    case 'u': return this->parse_unicode( at );
                    default: return this->fail( error_t::bad_escape, at );
                }
            }

            bool parse_string( span_t &out )
            {
                const char *at{ this->m_cursor };

                ++this->m_cursor;

                const u32 offset{ static_cast< u32 >( this->m_document.m_pool.size( ) ) };

                while ( true )
                {
                    if ( this->m_cursor >= this->m_end )
                    {
                        return this->fail( error_t::unterminated, at );
                    }

                    const char current{ *this->m_cursor };

                    if ( current == '"' )
                    {
                        ++this->m_cursor;
                        break;
                    }

                    if ( current == '\\' )
                    {
                        if ( !this->parse_escape( ) )
                        {
                            return false;
                        }

                        continue;
                    }

                    if ( static_cast< u8 >( current ) < 0x20u )
                    {
                        return this->fail( error_t::bad_string, this->m_cursor ); // raw control byte
                    }

                    if ( !this->push_byte( current ) )
                    {
                        return false;
                    }

                    ++this->m_cursor;
                }

                out.m_offset = offset;
                out.m_count  = static_cast< u32 >( this->m_document.m_pool.size( ) ) - offset;
                return true;
            }

            bool parse_number( u32 index )
            {
                const char *at{ this->m_cursor };
                bool        negative{ };

                if ( *this->m_cursor == '-' )
                {
                    negative = true;
                    ++this->m_cursor;
                }

                if ( this->m_cursor >= this->m_end || !str::is_digit( *this->m_cursor ) )
                {
                    return this->fail( error_t::bad_number, at );
                }

                u64  mantissa{ };
                i32  exponent{ };
                bool exact{ true };
                bool integral{ true };

                if ( *this->m_cursor == '0' )
                {
                    ++this->m_cursor;

                    if ( this->m_cursor < this->m_end && str::is_digit( *this->m_cursor ) )
                    {
                        return this->fail( error_t::bad_number, this->m_cursor ); // 007
                    }
                } else
                {
                    while ( this->m_cursor < this->m_end && str::is_digit( *this->m_cursor ) )
                    {
                        if ( mantissa <= MANTISSA_LIMIT )
                        {
                            mantissa = mantissa * 10ull + static_cast< u64 >( *this->m_cursor - '0' );
                        } else
                        {
                            ++exponent; // the digit only moves the decimal point now
                            exact = false;
                        }

                        ++this->m_cursor;
                    }
                }

                if ( this->m_cursor < this->m_end && *this->m_cursor == '.' )
                {
                    integral = false;
                    ++this->m_cursor;

                    if ( this->m_cursor >= this->m_end || !str::is_digit( *this->m_cursor ) )
                    {
                        return this->fail( error_t::bad_number, this->m_cursor );
                    }

                    while ( this->m_cursor < this->m_end && str::is_digit( *this->m_cursor ) )
                    {
                        if ( mantissa <= MANTISSA_LIMIT )
                        {
                            mantissa = mantissa * 10ull + static_cast< u64 >( *this->m_cursor - '0' );
                            --exponent;
                        } else
                        {
                            exact = false;
                        }

                        ++this->m_cursor;
                    }
                }

                if ( this->m_cursor < this->m_end && ( *this->m_cursor == 'e' || *this->m_cursor == 'E' ) )
                {
                    integral = false;
                    ++this->m_cursor;

                    bool negative_exponent{ };

                    if ( this->m_cursor < this->m_end && ( *this->m_cursor == '+' || *this->m_cursor == '-' ) )
                    {
                        negative_exponent = *this->m_cursor == '-';
                        ++this->m_cursor;
                    }

                    if ( this->m_cursor >= this->m_end || !str::is_digit( *this->m_cursor ) )
                    {
                        return this->fail( error_t::bad_number, this->m_cursor );
                    }

                    i32 literal{ };

                    while ( this->m_cursor < this->m_end && str::is_digit( *this->m_cursor ) )
                    {
                        if ( literal < 100000 ) // anything past this leaves the double range either way..
                        {
                            literal = literal * 10 + static_cast< i32 >( *this->m_cursor - '0' );
                        }

                        ++this->m_cursor;
                    }

                    exponent += negative_exponent ? -literal : literal;
                }

                node_t &node{ this->m_document.m_nodes[ index ] };
                node.m_type = type_t::number;

                if ( integral && exact && mantissa <= SIGNED_LIMIT )
                {
                    const i64 whole{ static_cast< i64 >( mantissa ) };

                    node.m_integral          = true;
                    node.m_payload.m_integer = negative ? -whole : whole;
                    return true;
                }

                f64 real{ };

                if ( !make_real( mantissa, exponent, real ) )
                {
                    return this->fail( error_t::bad_number, at );
                }

                node.m_payload.m_real = negative ? -real : real;
                return true;
            }

            bool parse_literal( u32 index )
            {
                node_t &node{ this->m_document.m_nodes[ index ] };

                if ( this->left( 4 ) && str::equals( this->m_cursor, 4, "true", 4 ) )
                {
                    this->m_cursor += 4;
                    node.m_type              = type_t::boolean;
                    node.m_payload.m_boolean = true;
                    return true;
                }

                if ( this->left( 5 ) && str::equals( this->m_cursor, 5, "false", 5 ) )
                {
                    this->m_cursor += 5;
                    node.m_type              = type_t::boolean;
                    node.m_payload.m_boolean = false;
                    return true;
                }

                if ( this->left( 4 ) && str::equals( this->m_cursor, 4, "null", 4 ) )
                {
                    this->m_cursor += 4;
                    node.m_type = type_t::null;
                    return true;
                }

                return this->fail( error_t::bad_literal, this->m_cursor );
            }

            bool parse_container( u32 index, type_t type )
            {
                const char *at{ this->m_cursor };
                const char  closing{ type == type_t::object ? '}' : ']' };

                ++this->m_cursor;

                u32  first{ INVALID_NODE };
                u32  last{ INVALID_NODE };
                u32  count{ };
                bool seen_comma{ };

                while ( true )
                {
                    this->skip_whitespace( );

                    if ( this->m_cursor >= this->m_end )
                    {
                        return this->fail( error_t::unterminated, at );
                    }

                    if ( *this->m_cursor == closing && !seen_comma )
                    {
                        ++this->m_cursor;
                        break;
                    }

                    span_t key{ };

                    if ( type == type_t::object )
                    {
                        if ( *this->m_cursor != '"' )
                        {
                            return this->fail( error_t::unexpected_token, this->m_cursor );
                        }

                        if ( !this->parse_string( key ) )
                        {
                            return false;
                        }

                        this->skip_whitespace( );

                        if ( this->m_cursor >= this->m_end || *this->m_cursor != ':' )
                        {
                            return this->fail( error_t::unexpected_token, this->m_cursor );
                        }

                        ++this->m_cursor;
                    }

                    u32 child{ INVALID_NODE };

                    if ( !this->parse_value( child ) )
                    {
                        return false;
                    }

                    this->m_document.m_nodes[ child ].m_key = key;

                    if ( last == INVALID_NODE )
                    {
                        first = child;
                    } else
                    {
                        this->m_document.m_nodes[ last ].m_next = child;
                    }

                    last = child;
                    ++count;

                    this->skip_whitespace( );

                    if ( this->m_cursor >= this->m_end )
                    {
                        return this->fail( error_t::unterminated, at );
                    }

                    if ( *this->m_cursor == ',' )
                    {
                        ++this->m_cursor;
                        seen_comma = true;
                        continue;
                    }

                    if ( *this->m_cursor == closing )
                    {
                        ++this->m_cursor;
                        break;
                    }

                    return this->fail( error_t::unexpected_token, this->m_cursor );
                }

                node_t &node{ this->m_document.m_nodes[ index ] };
                node.m_type           = type;
                node.m_payload.m_span = span_t{ first, count };
                return true;
            }

            bool parse_value( u32 &out )
            {
                if ( this->m_depth >= MAX_DEPTH )
                {
                    return this->fail( error_t::depth_exceeded, this->m_cursor );
                }

                this->skip_whitespace( );

                if ( this->m_cursor >= this->m_end )
                {
                    return this->fail( error_t::unterminated, this->m_cursor );
                }

                const u32 index{ this->add_node( ) };

                if ( index == INVALID_NODE )
                {
                    return this->fail( error_t::allocation_failed, this->m_cursor );
                }

                out = index;

                ++this->m_depth;

                bool result{ };

                switch ( *this->m_cursor )
                {
                    case '{': result = this->parse_container( index, type_t::object ); break;
                    case '[': result = this->parse_container( index, type_t::array ); break;
                    case 't':
                    case 'f':
                    case 'n': result = this->parse_literal( index ); break;
                    case '"':
                    {
                        span_t span{ };
                        result = this->parse_string( span );

                        if ( result )
                        {
                            node_t &node{ this->m_document.m_nodes[ index ] };
                            node.m_type           = type_t::string;
                            node.m_payload.m_span = span;
                        }

                        break;
                    }
                    default:
                    {
                        if ( *this->m_cursor == '-' || str::is_digit( *this->m_cursor ) )
                        {
                            result = this->parse_number( index );
                        } else
                        {
                            result = this->fail( error_t::unexpected_token, this->m_cursor );
                        }

                        break;
                    }
                }

                --this->m_depth;
                return result;
            }
        };
    } // namespace detail

    value::value( const document *owner, u32 index ) :
        m_document{ owner },
        m_index{ index }
    {
    }

    const detail::node_t *value::node( ) const
    {
        return this->m_document ? this->m_document->node( this->m_index ) : nullptr;
    }

    type_t value::type( ) const
    {
        const detail::node_t *node{ this->node( ) };
        return node ? node->m_type : type_t::null;
    }

    bool value::valid( ) const
    {
        return this->node( ) != nullptr;
    }

    usize value::size( ) const
    {
        const detail::node_t *node{ this->node( ) };

        if ( !node || ( node->m_type != type_t::object && node->m_type != type_t::array ) )
        {
            return 0;
        }

        return node->m_payload.m_span.m_count;
    }

    value value::operator[]( string_view name ) const
    {
        const detail::node_t *node{ this->node( ) };

        if ( !node || node->m_type != type_t::object )
        {
            return value{ };
        }

        u32 current{ node->m_payload.m_span.m_offset };

        for ( u32 step{ }; step < node->m_payload.m_span.m_count; ++step )
        {
            const detail::node_t *member{ this->m_document->node( current ) };

            if ( this->m_document->pooled( member->m_key ) == name )
            {
                return value{ this->m_document, current };
            }

            current = member->m_next;
        }

        return value{ };
    }

    value value::operator[]( usize index ) const
    {
        const detail::node_t *node{ this->node( ) };

        if ( !node || ( node->m_type != type_t::object && node->m_type != type_t::array ) )
        {
            return value{ };
        }

        if ( index >= node->m_payload.m_span.m_count )
        {
            return value{ };
        }

        u32 current{ node->m_payload.m_span.m_offset };

        for ( usize step{ }; step < index; ++step )
        {
            current = this->m_document->node( current )->m_next;
        }

        return value{ this->m_document, current };
    }

    bool value::boolean( bool &out ) const
    {
        const detail::node_t *node{ this->node( ) };

        if ( !node || node->m_type != type_t::boolean )
        {
            return false;
        }

        out = node->m_payload.m_boolean;
        return true;
    }

    bool value::number( i64 &out ) const
    {
        const detail::node_t *node{ this->node( ) };

        if ( !node || node->m_type != type_t::number )
        {
            return false;
        }

        if ( node->m_integral )
        {
            out = node->m_payload.m_integer;
            return true;
        }

        const f64 real{ node->m_payload.m_real };
        if ( real >= SIGNED_SCALE )
        {
            out = static_cast< i64 >( SIGNED_LIMIT );
        } else if ( real < -SIGNED_SCALE )
        {
            out = -static_cast< i64 >( SIGNED_LIMIT ) - 1;
        } else
        {
            out = static_cast< i64 >( real );
        }

        return true;
    }

    bool value::number( f64 &out ) const
    {
        const detail::node_t *node{ this->node( ) };

        if ( !node || node->m_type != type_t::number )
        {
            return false;
        }

        out = node->m_integral ? static_cast< f64 >( node->m_payload.m_integer ) : node->m_payload.m_real;
        return true;
    }

    bool value::text( string_view &out ) const
    {
        const detail::node_t *node{ this->node( ) };

        if ( !node || node->m_type != type_t::string )
        {
            return false;
        }

        out = this->m_document->pooled( node->m_payload.m_span );
        return true;
    }

    document::~document( )
    {
        if ( this->m_sensitive )
        {
            atl::memset( this->m_pool.data( ), 0, this->m_pool.capacity( ) );
        }
    }

    bool document::parse( string_view text, bool sensitive )
    {
        this->reset( );
        this->m_sensitive = sensitive;

        if ( text.empty( ) )
        {
            this->m_error = error_t::no_input;
            return false;
        }

        if ( !this->m_pool.reserve( text.size( ) ) )
        {
            this->m_error = error_t::allocation_failed;
            return false;
        }

        detail::parser parser{ *this, text.data( ), text.data( ), text.data( ) + text.size( ) };

        u32 root{ INVALID_NODE };

        if ( !parser.parse_value( root ) )
        {
            if ( this->m_sensitive )
            {
                atl::memset( this->m_pool.data( ), 0, this->m_pool.capacity( ) );
            }
            this->m_nodes.reset( );
            this->m_pool.reset( );
            return false;
        }

        parser.skip_whitespace( );

        if ( parser.m_cursor != parser.m_end )
        {
            parser.fail( error_t::trailing_data, parser.m_cursor );
            if ( this->m_sensitive )
            {
                atl::memset( this->m_pool.data( ), 0, this->m_pool.capacity( ) );
            }
            this->m_nodes.reset( );
            this->m_pool.reset( );
            return false;
        }

        return true;
    }

    void document::reset( )
    {
        if ( this->m_sensitive )
        {
            atl::memset( this->m_pool.data( ), 0, this->m_pool.capacity( ) );
        }
        this->m_nodes.reset( );
        this->m_pool.reset( );
        this->m_error        = error_t::none;
        this->m_error_offset = 0;
    }

    value document::root( ) const
    {
        return this->m_nodes.empty( ) ? value{ } : value{ this, 0 };
    }

    error_t document::error( ) const
    {
        return this->m_error;
    }

    usize document::error_offset( ) const
    {
        return this->m_error_offset;
    }

    const detail::node_t *document::node( u32 index ) const
    {
        return this->m_nodes.at( index );
    }

    string_view document::pooled( detail::span_t span ) const
    {
        if ( !span.m_count )
        {
            return string_view{ };
        }

        return string_view{ this->m_pool.data( ) + span.m_offset, span.m_count };
    }

    void writer::push( char value )
    {
        if ( this->m_failed )
        {
            return;
        }

        if ( !this->m_buffer.push_back( value ) )
        {
            this->m_failed = true;
        }
    }

    void writer::push( string_view text )
    {
        for ( char value : text )
        {
            this->push( value );
        }
    }

    void writer::indent( usize depth )
    {
        for ( usize level{ }; level < depth; ++level )
        {
            this->push( string_view{ "    " } );
        }
    }

    void writer::prefix( )
    {
        if ( this->m_after_key )
        {
            this->m_after_key = false; // the value belongs to the key already on the line
            return;
        }

        if ( !this->m_depth )
        {
            return; // the root value stands on its own
        }

        const u64 bit{ 1ull << ( this->m_depth - 1 ) };

        if ( this->m_populated & bit )
        {
            this->push( ',' );
        }

        this->m_populated |= bit;
        this->push( '\n' );
        this->indent( this->m_depth );
    }

    void writer::quoted( string_view text )
    {
        this->push( '"' );

        for ( char value : text )
        {
            switch ( value )
            {
                case '"': this->push( string_view{ "\\\"" } ); continue;
                case '\\': this->push( string_view{ "\\\\" } ); continue;
                case '\b': this->push( string_view{ "\\b" } ); continue;
                case '\f': this->push( string_view{ "\\f" } ); continue;
                case '\n': this->push( string_view{ "\\n" } ); continue;
                case '\r': this->push( string_view{ "\\r" } ); continue;
                case '\t': this->push( string_view{ "\\t" } ); continue;
                default: break;
            }

            if ( static_cast< u8 >( value ) < 0x20u )
            {
                constexpr char digits[ 17 ]{ "0123456789abcdef" };
                const u8       byte{ static_cast< u8 >( value ) };

                this->push( string_view{ "\\u00" } );
                this->push( digits[ byte >> 4u ] );
                this->push( digits[ byte & 0x0Fu ] );
                continue;
            }

            this->push( value );
        }

        this->push( '"' );
    }

    void writer::open( char bracket )
    {
        Assert( this->m_depth < MAX_DEPTH );

        this->prefix( );
        this->push( bracket );

        ++this->m_depth;
        this->m_populated &= ~( 1ull << ( this->m_depth - 1 ) );
    }

    void writer::close( char bracket )
    {
        Assert( this->m_depth != 0 );

        const u64 bit{ 1ull << ( this->m_depth - 1 ) };

        --this->m_depth;

        if ( this->m_populated & bit )
        {
            this->push( '\n' );
            this->indent( this->m_depth );
        }

        this->push( bracket );
    }

    void writer::begin_object( )
    {
        this->open( '{' );
    }

    void writer::end_object( )
    {
        this->close( '}' );
    }

    void writer::begin_array( )
    {
        this->open( '[' );
    }

    void writer::end_array( )
    {
        this->close( ']' );
    }

    void writer::key( string_view name )
    {
        this->prefix( );
        this->quoted( name );
        this->push( string_view{ ": " } );
        this->m_after_key = true;
    }

    void writer::write( string_view text )
    {
        this->prefix( );
        this->quoted( text );
    }

    void writer::write_boolean( bool value )
    {
        this->prefix( );
        this->push( value ? string_view{ "true" } : string_view{ "false" } );
    }

    void writer::write_integer( i64 value )
    {
        this->prefix( );
        this->emit_integer( value );
    }

    void writer::write_unsigned( u64 value )
    {
        this->prefix( );
        this->emit_unsigned( value );
    }

    void writer::emit_integer( i64 value )
    {
        if ( value < 0 )
        {
            this->push( '-' );
            this->emit_unsigned( 0ull - static_cast< u64 >( value ) );
            return;
        }

        this->emit_unsigned( static_cast< u64 >( value ) );
    }

    void writer::emit_unsigned( u64 value )
    {
        char  buffer[ 20 ]{ };
        usize index{ sizeof( buffer ) };

        do
        {
            --index;
            buffer[ index ] = static_cast< char >( '0' + value % 10ull );
            value /= 10ull;
        } while ( value );

        this->push( string_view{ buffer + index, sizeof( buffer ) - index } );
    }

    void writer::write_real( f64 value, bool single )
    {
        this->prefix( );
        if ( value != value || value >= SIGNED_SCALE || value < -SIGNED_SCALE )
        {
            this->push( string_view{ "null" } );
            return;
        }

        const i64 truncated{ static_cast< i64 >( value ) };

        if ( static_cast< f64 >( truncated ) == value )
        {
            this->emit_integer( truncated );
            return;
        }

        f64 magnitude{ value };

        if ( value < 0.0 )
        {
            this->push( '-' );
            magnitude = -value;
        }

        u32 limit{ MAX_DECIMALS };
        while ( limit && magnitude > SCALE_CEILING / POWERS_OF_TEN[ limit ] )
        {
            --limit;
        }

        u32 decimals{ 1 };
        while ( decimals < limit )
        {
            const f64 scale{ POWERS_OF_TEN[ decimals ] };
            const u64 scaled{ static_cast< u64 >( magnitude * scale + 0.5 ) };
            const f64 rounded{ static_cast< f64 >( scaled ) / scale };

            if ( scaled <= EXACT_LIMIT &&
                 ( single ? static_cast< f32 >( rounded ) == static_cast< f32 >( magnitude ) : rounded == magnitude ) )
            {
                break;
            }

            ++decimals;
        }

        const u64 scale{ INTEGER_POWERS_OF_TEN[ decimals ] };
        const u64 scaled{ static_cast< u64 >( magnitude * static_cast< f64 >( scale ) + 0.5 ) };

        this->emit_unsigned( scaled / scale );
        this->push( '.' );

        char buffer[ MAX_DECIMALS ]{ };
        u64  fraction{ scaled % scale };

        for ( usize index{ decimals }; index; )
        {
            --index;
            buffer[ index ] = static_cast< char >( '0' + fraction % 10ull );
            fraction /= 10ull;
        }

        this->push( string_view{ buffer, decimals } );
    }

    bool writer::ok( ) const
    {
        return !this->m_failed;
    }

    string_view writer::text( ) const
    {
        Assert( this->m_depth == 0 ); // an unbalanced writer never produced a document

        if ( this->m_failed || this->m_buffer.empty( ) )
        {
            return string_view{ };
        }

        return string_view{ this->m_buffer.data( ), this->m_buffer.size( ) };
    }
} // namespace json
