#pragma once

#define HUGE_VAL __builtin_huge_val( )

static inline double floor( double value )
{
    return __builtin_floor( value );
}

static inline double ceil( double value )
{
    return __builtin_ceil( value );
}

static inline double fabs( double value )
{
    return __builtin_fabs( value );
}

static inline double sqrt( double value )
{
    return __builtin_elementwise_sqrt( value );
}

extern "C"
{
    double lua_crt_fmod( double x, double y );
    double lua_crt_pow( double x, double y );
    double lua_crt_exp( double x );
    double lua_crt_log( double x );
    double lua_crt_log10( double x );
    double lua_crt_sin( double x );
    double lua_crt_cos( double x );
    double lua_crt_tan( double x );
    double lua_crt_asin( double x );
    double lua_crt_acos( double x );
    double lua_crt_atan2( double y, double x );
    double lua_crt_frexp( double x, int *exponent );
    double lua_crt_ldexp( double x, int exponent );
}

#define fmod  lua_crt_fmod
#define pow   lua_crt_pow
#define exp   lua_crt_exp
#define log   lua_crt_log
#define log10 lua_crt_log10
#define sin   lua_crt_sin
#define cos   lua_crt_cos
#define tan   lua_crt_tan
#define asin  lua_crt_asin
#define acos  lua_crt_acos
#define atan2 lua_crt_atan2
#define frexp lua_crt_frexp
#define ldexp lua_crt_ldexp
