#pragma once

#include "typedefs.hpp"

#include "runtime/vector.hpp"
#include "util/string.hpp"

namespace base64
{
    ATLAS_FORCE_INLINE char to_char( u32 value )
    {
        if ( value < 26 )
        {
            return static_cast< char >( 'A' + value );
        }

        if ( value < 52 )
        {
            return static_cast< char >( 'a' + value - 26 );
        }

        if ( value < 62 )
        {
            return static_cast< char >( '0' + value - 52 );
        }

        return value == 62 ? '+' : '/';
    }

    ATLAS_FORCE_INLINE i32 to_value( char value )
    {
        if ( value >= 'A' && value <= 'Z' )
        {
            return value - 'A';
        }

        if ( value >= 'a' && value <= 'z' )
        {
            return value - 'a' + 26;
        }

        if ( value >= '0' && value <= '9' )
        {
            return value - '0' + 52;
        }

        if ( value == '+' )
        {
            return 62;
        }

        return value == '/' ? 63 : -1;
    }

    ATLAS_FORCE_INLINE bool append( const u8 *input, usize size, atl::vector< char > &output )
    {
        usize at{ output.size( ) };

        if ( !output.resize( at + ( size + 2 ) / 3 * 4 ) )
        {
            return false;
        }

        for ( usize index{ }; index < size; index += 3 )
        {
            const usize left{ size - index };
            const u32   chunk{ static_cast< u32 >( input[ index ] ) << 16 | ( left > 1 ? static_cast< u32 >( input[ index + 1 ] ) << 8 : 0u ) |
                               ( left > 2 ? static_cast< u32 >( input[ index + 2 ] ) : 0u ) };

            output[ at++ ] = to_char( chunk >> 18 & 63 );
            output[ at++ ] = to_char( chunk >> 12 & 63 );
            output[ at++ ] = left > 1 ? to_char( chunk >> 6 & 63 ) : '=';
            output[ at++ ] = left > 2 ? to_char( chunk & 63 ) : '=';
        }

        return true;
    }

    ATLAS_FORCE_INLINE bool decode( string_view input, atl::vector< u8 > &output )
    {
        const usize size{ input.size( ) };

        if ( size % 4 )
        {
            return false;
        }

        usize padding{ };

        while ( padding < 2 && padding < size && input.data( )[ size - 1 - padding ] == '=' )
        {
            ++padding;
        }

        if ( !output.resize( size / 4 * 3 - padding ) )
        {
            return false;
        }

        usize at{ };

        for ( usize index{ }; index < size; index += 4 )
        {
            u32 chunk{ };

            for ( usize offset{ }; offset < 4; ++offset )
            {
                const usize position{ index + offset };
                const i32   value{ position < size - padding ? to_value( input.data( )[ position ] ) : 0 };

                if ( value < 0 )
                {
                    return false;
                }

                chunk = chunk << 6 | static_cast< u32 >( value );
            }

            for ( usize offset{ }; offset < 3 && at < output.size( ); ++offset )
            {
                output[ at++ ] = static_cast< u8 >( chunk >> ( 16 - 8 * offset ) );
            }
        }

        return true;
    }
}
