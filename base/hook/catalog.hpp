#pragma once

#include "util/typedefs.hpp"

namespace atlashook
{
    template< typename module_type, typename resolve_type >
    struct signature_definition
    {
        const char  *key{ };
        module_type  module{ };
        const char  *pattern{ };
        resolve_type resolve{ };
        u8           dereference_count{ };
        i32          addend{ };
    };

    template< typename type_type >
    struct hook_definition
    {
        const char *key{ };
        const char *signature_key{ };
        type_type   type{ };
        u32         target{ };
    };

    constexpr i16 hex_value( char value )
    {
        if ( value >= '0' && value <= '9' )
        {
            return static_cast< i16 >( value - '0' );
        }

        if ( value >= 'A' && value <= 'F' )
        {
            return static_cast< i16 >( value - 'A' + 10 );
        }

        if ( value >= 'a' && value <= 'f' )
        {
            return static_cast< i16 >( value - 'a' + 10 );
        }

        return -1;
    }

    constexpr u16 pattern_size( const char *pattern )
    {
        if ( !pattern || !*pattern )
        {
            return 0;
        }

        u16         size{ };
        const char *cursor{ pattern };

        while ( *cursor )
        {
            if ( *cursor == '?' )
            {
                ++cursor;

                if ( *cursor == '?' )
                {
                    ++cursor;
                }
            }
            else
            {
                if ( hex_value( cursor[ 0 ] ) < 0 || hex_value( cursor[ 1 ] ) < 0 )
                {
                    return 0;
                }

                cursor += 2;
            }

            ++size;

            if ( !*cursor )
            {
                break;
            }

            if ( *cursor != ' ' || !cursor[ 1 ] )
            {
                return 0;
            }

            ++cursor;
        }

        return size;
    }

    constexpr bool parse_pattern( const char *pattern, u8 *bytes, u8 *mask, u16 capacity )
    {
        const u16 size{ pattern_size( pattern ) };

        if ( !size || size != capacity || !bytes || !mask )
        {
            return false;
        }

        const char *cursor{ pattern };

        for ( u16 index{ }; index < size; ++index )
        {
            if ( *cursor == '?' )
            {
                bytes[ index ] = 0;
                mask[ index ]  = 0;
                ++cursor;

                if ( *cursor == '?' )
                {
                    ++cursor;
                }
            }
            else
            {
                const i16 high{ hex_value( cursor[ 0 ] ) };
                const i16 low{ hex_value( cursor[ 1 ] ) };
                bytes[ index ] = static_cast< u8 >( ( high << 4 ) | low );
                mask[ index ]  = 0xFF;
                cursor += 2;
            }

            if ( *cursor == ' ' )
            {
                ++cursor;
            }
        }

        return !*cursor;
    }
} // namespace atlashook
