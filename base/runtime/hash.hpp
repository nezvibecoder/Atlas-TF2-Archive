#pragma once

#include "util/typedefs.hpp"

using hash_t = u32;

namespace hash
{
    constexpr u32 hash_prime{ 0x1000193u };
    constexpr u32 hash_basis{ 0x811C9DC5u };

    template< typename type >
    constexpr bool is_upper( const type character )
    {
        return character >= static_cast< const type >( 65 ) && character <= static_cast< const type >( 90 );
    }

    template< typename type >
    constexpr type to_lower( const type character )
    {
        if ( is_upper( character ) )
        {
            return character + static_cast< const type >( 32 );
        }

        return character;
    }

    template< typename type >
    constexpr bool is_terminator( const type character )
    {
        return character == static_cast< const type >( 0 );
    }

    template< typename type >
    constexpr size_t get_length( const type *const data )
    {
        size_t length{ 0 };

        while ( true )
        {
            if ( is_terminator( data[ length ] ) )
            {
                break;
            }

            length++;
        }

        return length;
    }

    template< typename type >
    constexpr hash_t compute_hash( const hash_t hash_basis, const type *data, const size_t size, const bool ignore_case )
    {
        if ( size == 0 )
        {
            return hash_basis;
        }

        const hash_t element{ static_cast< hash_t >( ignore_case ? to_lower( data[ 0 ] ) : data[ 0 ] ) };

        return compute_hash( ( hash_basis ^ element ) * hash_prime, data + 1, size - 1, ignore_case );
    }

    template< typename type >
    constexpr hash_t make_hash( const type *const data, const size_t size, const bool ignore_case )
    {
        return compute_hash( hash_basis, data, size, ignore_case );
    }

    constexpr hash_t make_hash( const char *const data, const bool ignore_case )
    {
        return make_hash( data, get_length( data ), ignore_case );
    }

    constexpr hash_t make_hash( const wchar_t *const data, const bool ignore_case )
    {
        return make_hash( data, get_length( data ), ignore_case );
    }
} // namespace hash

#define HASH_CT( data ) [ & ]( ) { constexpr hash_t hash{ hash::make_hash(data, true) }; return hash; }( )
#define HASH_RT( data ) hash::make_hash( data, true )
