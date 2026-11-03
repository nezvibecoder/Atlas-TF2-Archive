#pragma once

#include "string.hpp"
#include "typedefs.hpp"

namespace str
{
    namespace detail
    {
        inline bool scan_value( const char *&cursor, double &out )
        {
            bool negative{ };

            if ( *cursor == '-' || *cursor == '+' )
            {
                negative = *cursor == '-';
                ++cursor;
            }

            if ( !is_digit( *cursor ) )
            {
                return false;
            }

            double value{ };

            while ( is_digit( *cursor ) )
            {
                value = value * 10.0 + static_cast< double >( *cursor - '0' );
                ++cursor;
            }

            if ( *cursor == '.' )
            {
                ++cursor;

                double scale{ 0.1 };

                while ( is_digit( *cursor ) )
                {
                    value += static_cast< double >( *cursor - '0' ) * scale;
                    scale *= 0.1;
                    ++cursor;
                }
            }

            out = negative ? -value : value;
            return true;
        }

        inline bool scan_value( const char *&cursor, float &out )
        {
            double value{ };

            if ( !scan_value( cursor, value ) )
            {
                return false;
            }

            out = static_cast< float >( value );
            return true;
        }

        inline bool scan_value( const char *&cursor, i32 &out )
        {
            double value{ };

            if ( !scan_value( cursor, value ) )
            {
                return false;
            }

            out = static_cast< i32 >( value );
            return true;
        }

        inline bool scan_value( const char *&cursor, u32 &out )
        {
            double value{ };

            if ( !scan_value( cursor, value ) || value < 0.0 )
            {
                return false;
            }

            out = static_cast< u32 >( value );
            return true;
        }

        inline void skip_spaces( const char *&cursor )
        {
            while ( is_space( *cursor ) )
            {
                ++cursor;
            }
        }

        inline bool scan_literal( const char *&cursor, const char *&pattern )
        {
            while ( *pattern && !( pattern[ 0 ] == '{' && pattern[ 1 ] == '}' ) )
            {
                if ( is_space( *pattern ) )
                {
                    skip_spaces( cursor );

                    while ( is_space( *pattern ) )
                    {
                        ++pattern;
                    }

                    continue;
                }

                if ( *cursor != *pattern )
                {
                    return false;
                }

                ++cursor;
                ++pattern;
            }

            return true;
        }

        inline bool scan_next( const char *&cursor, const char *&pattern )
        {
            if ( !scan_literal( cursor, pattern ) )
            {
                return false;
            }

            // ran out of pattern with arguments still to fill
            return *pattern != '\0';
        }

        inline bool scan_tail( const char *cursor, const char *pattern )
        {
            if ( !scan_literal( cursor, pattern ) )
            {
                return false;
            }

            skip_spaces( cursor );

            return *pattern == '\0' && *cursor == '\0';
        }
    } // namespace detail

    inline bool scan( const char *input, const char *pattern )
    {
        detail::skip_spaces( input );
        return detail::scan_tail( input, pattern );
    }

    template< typename type, typename... rest >
    bool scan( const char *input, const char *pattern, type &first, rest &...others )
    {
        detail::skip_spaces( input );

        if ( !detail::scan_next( input, pattern ) )
        {
            return false;
        }

        pattern += 2; // consume the placeholder
        detail::skip_spaces( input );

        if ( !detail::scan_value( input, first ) )
        {
            return false;
        }

        if constexpr ( sizeof...( others ) == 0 )
        {
            return detail::scan_tail( input, pattern );
        } else
        {
            return scan( input, pattern, others... );
        }
    }
} // namespace str
