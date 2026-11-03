#ifndef _LIBM_H
#define _LIBM_H

#include <float.h>
#include <math.h>
#include <stdint.h>

#define hidden

typedef double double_t;

#define isnan( x ) __builtin_isnan( x )

#define WANT_ROUNDING           1
#define TOINT_INTRINSICS        0
#define issignaling_inline( x ) 0

#define predict_true( x )  __builtin_expect( !!( x ), 1 )
#define predict_false( x ) __builtin_expect( x, 0 )

static inline double eval_as_double( double x )
{
    double y = x;
    return y;
}

static inline double fp_barrier( double x )
{
    volatile double y = x;
    return y;
}

static inline void fp_force_eval( double x )
{
    volatile double y;
    y = x;
}

#define FORCE_EVAL( x ) fp_force_eval( x )

#define asuint64( f ) __builtin_bit_cast( uint64_t, static_cast< double >( f ) )
#define asdouble( i ) __builtin_bit_cast( double, static_cast< uint64_t >( i ) )

#define EXTRACT_WORDS( hi, lo, d )      \
    do                                  \
    {                                   \
        uint64_t __u = asuint64( d );   \
        ( hi )       = __u >> 32;       \
        ( lo )       = ( uint32_t )__u; \
    } while ( 0 )

#define GET_HIGH_WORD( hi, d )        \
    do                                \
    {                                 \
        ( hi ) = asuint64( d ) >> 32; \
    } while ( 0 )

#define GET_LOW_WORD( lo, d )               \
    do                                      \
    {                                       \
        ( lo ) = ( uint32_t )asuint64( d ); \
    } while ( 0 )

#define INSERT_WORDS( d, hi, lo )                                              \
    do                                                                         \
    {                                                                          \
        ( d ) = asdouble( ( ( uint64_t )( hi ) << 32 ) | ( uint32_t )( lo ) ); \
    } while ( 0 )

#define SET_HIGH_WORD( d, hi ) INSERT_WORDS( d, hi, ( uint32_t )asuint64( d ) )
#define SET_LOW_WORD( d, lo )  INSERT_WORDS( d, asuint64( d ) >> 32, lo )

#define scalbn lua_crt_scalbn
#define atan   lua_crt_atan

extern "C"
{
    double lua_crt_scalbn( double x, int n );
    double lua_crt_atan( double x );

    int    __rem_pio2_large( double *x, double *y, int e0, int nx, int prec );
    int    __rem_pio2( double x, double *y );
    double __sin( double x, double y, int iy );
    double __cos( double x, double y );
    double __tan( double x, double y, int odd );

    double __math_xflow( uint32_t sign, double y );
    double __math_uflow( uint32_t sign );
    double __math_oflow( uint32_t sign );
    double __math_divzero( uint32_t sign );
    double __math_invalid( double x );
}

#endif
