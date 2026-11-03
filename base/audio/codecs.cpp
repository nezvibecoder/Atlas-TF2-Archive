#include "runtime/assert.hpp"
#include "util/typedefs.hpp"

#include <string.h>

inline constexpr double VORBIS_LN2{ 0.693147180559945309417232121458 };
inline constexpr double VORBIS_PI{ 3.14159265358979323846264338328 };

static double vorbis_ldexp( double value, int exponent )
{
    return value * __builtin_bit_cast( double, static_cast< u64 >( exponent + 1023 ) << 52 );
}

static double vorbis_floor( double value )
{
    const double whole{ static_cast< double >( static_cast< i32 >( value ) ) };
    return whole > value ? whole - 1.0 : whole;
}

static double vorbis_log( double value )
{
    const u64    bits{ __builtin_bit_cast( u64, value ) };
    const i32    exponent{ static_cast< i32 >( ( bits >> 52 ) & 0x7FF ) - 1023 };
    const double mantissa{ __builtin_bit_cast( double, ( bits & 0xFFFFFFFFFFFFFull ) | 0x3FF0000000000000ull ) };
    const double z{ ( mantissa - 1.0 ) / ( mantissa + 1.0 ) };

    double term{ z };
    double sum{ };
    for ( i32 odd{ 1 }; odd < 40; odd += 2 )
    {
        sum += term / odd;
        term *= z * z;
    }

    return 2.0 * sum + exponent * VORBIS_LN2;
}

static double vorbis_exp( double value )
{
    const double whole{ vorbis_floor( value / VORBIS_LN2 + 0.5 ) };
    const double rest{ value - whole * VORBIS_LN2 };

    double term{ 1.0 };
    double sum{ 1.0 };
    for ( i32 index{ 1 }; index < 20; ++index )
    {
        term *= rest / index;
        sum += term;
    }

    return vorbis_ldexp( sum, static_cast< int >( whole ) );
}

static double vorbis_pow( double base, double exponent )
{
    Assert( exponent == vorbis_floor( exponent ) && exponent >= 0.0 );

    double result{ 1.0 };
    for ( i32 index{ }; index < static_cast< i32 >( exponent ); ++index )
    {
        result *= base;
    }

    return result;
}

static double vorbis_sin( double value )
{
    value -= vorbis_floor( value / ( 2.0 * VORBIS_PI ) + 0.5 ) * ( 2.0 * VORBIS_PI );

    double term{ value };
    double sum{ value };
    for ( i32 index{ 2 }; index < 30; index += 2 )
    {
        term *= -value * value / ( index * ( index + 1 ) );
        sum += term;
    }

    return sum;
}

static double vorbis_cos( double value )
{
    return vorbis_sin( value + VORBIS_PI * 0.5 );
}

static int vorbis_abs( int value )
{
    return value < 0 ? -value : value;
}

static void vorbis_sort( void *base, usize count, usize size, int ( *compare )( const void *, const void * ) )
{
    auto *bytes{ static_cast< u8 * >( base ) };

    for ( usize gap{ count / 2 }; gap; gap /= 2 )
    {
        for ( usize index{ gap }; index < count; ++index )
        {
            for ( usize at{ index }; at >= gap && compare( bytes + ( at - gap ) * size, bytes + at * size ) > 0; at -= gap )
            {
                u8 *left{ bytes + ( at - gap ) * size };
                u8 *right{ bytes + at * size };

                for ( usize offset{ }; offset < size; ++offset )
                {
                    const u8 swap{ left[ offset ] };
                    left[ offset ]  = right[ offset ];
                    right[ offset ] = swap;
                }
            }
        }
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#define MINIMP3_IMPLEMENTATION
#include "audio/codecs.hpp"

#define ldexp( value, exponent ) vorbis_ldexp( value, exponent )
#define floor( value )           vorbis_floor( value )
#define log( value )             vorbis_log( value )
#define exp( value )             vorbis_exp( value )
#define pow( base, exponent )    vorbis_pow( base, exponent )
#define sin( value )             vorbis_sin( value )
#define cos( value )             vorbis_cos( value )
#define abs( value )             vorbis_abs( value )
#define qsort                    vorbis_sort
#define assert( expression )     Assert( expression )
#define alloca                   __builtin_alloca

#include "libs/stb_vorbis.c"

#pragma clang diagnostic pop
