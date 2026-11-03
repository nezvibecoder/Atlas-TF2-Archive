#include "libs/lazy_importer.hpp"
#include "util/typedefs.hpp"

#pragma clang section rodata = ".lstr$a"
extern "C" __attribute__( ( used, retain ) ) volatile const u8 lstr_cookie[ 16 ]{
    'A', 'T', 'L', 'P', 'L', 'A', 'I', 'N',
    0, 0, 0, 0, 0, 0, 0, 0,
};
#pragma clang section rodata = ".lstr$z"
extern "C" __attribute__( ( used, retain ) ) volatile const u8 lstr_end[ 16 ]{
    'A', 'T', 'L', 'E', 'N', 'D', 'X', 'X',
    0, 0, 0, 0, 0, 0, 0, 0,
};
#pragma clang section rodata = ""

ATLAS_FORCE_INLINE bool marker_matches( const volatile u8 *data, const char *word )
{
    for ( usize index{ }; index < 8; ++index )
    {
        if ( data[ index ] != static_cast< u8 >( word[ index ] ) )
        {
            return false;
        }
    }

    return true;
}

extern "C" void decrypt_lstr( )
{
    volatile u8 *start{ const_cast< volatile u8 * >( lstr_cookie ) };
    volatile u8 *end{ const_cast< volatile u8 * >( lstr_end ) + sizeof( lstr_end ) };

    if ( end <= start + sizeof( lstr_cookie ) )
    {
        return;
    }

    if ( !marker_matches( start, "ATLXORED" ) )
    {
        return;
    }

    u8 key[ 8 ]{ };
    for ( usize i{ }; i < 8; ++i )
    {
        key[ i ] = start[ 8 + i ];
    }

    const usize size{ static_cast< usize >( end - start ) };
    for ( usize i{ sizeof( lstr_cookie ) }; i < size; ++i )
    {
        start[ i ] ^= key[ i & 7 ] ^ static_cast< u8 >( i * 73u + 0x9Du );
    }

    const char *plaintext{ "ATLPLAIN" };
    for ( usize i{ }; i < 8; ++i )
    {
        start[ i ] = static_cast< u8 >( plaintext[ i ] );
    }

    for ( usize i{ 8 }; i < 16; ++i )
    {
        start[ i ] = 0;
    }
}
