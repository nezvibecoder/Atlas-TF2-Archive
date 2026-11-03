#pragma once

typedef long long time_t;
typedef long      clock_t;

static inline time_t time( time_t *out )
{
    const unsigned long long ticks{ *reinterpret_cast< volatile unsigned long long * >( 0x7FFE0014 ) };
    const time_t             now{ static_cast< time_t >( ( ticks - 116444736000000000ull ) / 10000000ull ) };

    if ( out )
    {
        *out = now;
    }

    return now;
}

static inline clock_t clock( )
{
    return static_cast< clock_t >( __builtin_ia32_rdtsc( ) );
}
