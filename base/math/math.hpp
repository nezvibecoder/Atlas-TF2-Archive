#pragma once

#include "util/typedefs.hpp"

#include <emmintrin.h>
#include <xmmintrin.h>

using matrix3x4 = f32[ 3 ][ 4 ];
using matrix4x4 = f32[ 4 ][ 4 ];
using VMatrix   = matrix4x4;

inline constexpr f32 PI{ 3.14159265358979 };

#define DEG2RAD( x ) ( ( f32 )( x ) * ( PI / 180.0f ) )
#define RAD2DEG( x ) ( ( f32 )( x ) * ( 180.0f / PI ) )

#define MATH_USE_BUILTINS

extern "C" double lua_crt_pow( double x, double y );

namespace math
{
    inline constexpr f64 PI_D{ 3.14159265358979323846264338327950288 };

    template< typename type >
    ATLAS_FORCE_INLINE constexpr type smaller( type left, type right )
    {
        return left < right ? left : right;
    }

    template< typename type >
    ATLAS_FORCE_INLINE constexpr type bigger( type left, type right )
    {
        return left > right ? left : right;
    }

    template< typename type >
    ATLAS_FORCE_INLINE constexpr type clamp( type value, type minimum, type maximum )
    {
        if ( value < minimum )
        {
            return minimum;
        }

        return value > maximum ? maximum : value;
    }

    template< typename type >
    ATLAS_FORCE_INLINE constexpr type approach( type value, type target, type step )
    {
        if ( value < target )
        {
            type distance{ static_cast< type >( target - value ) };
            return step >= distance ? target : static_cast< type >( value + step );
        }

        if ( value > target )
        {
            type distance{ static_cast< type >( value - target ) };
            return step >= distance ? target : static_cast< type >( value - step );
        }

        return value;
    }

    ATLAS_FORCE_INLINE constexpr f32 animate( f32 value, f32 target, u32 delta, f32 response )
    {
        f32 difference{ target - value };
        if ( difference > -0.002f && difference < 0.002f )
        {
            return target;
        }

        f32 factor{ static_cast< f32 >( delta ) / response };
        if ( factor > 1.0f )
        {
            factor = 1.0f;
        }

        return value + difference * factor;
    }

    // u / ( e^u - 1 )
    ATLAS_FORCE_INLINE constexpr f32 speed_over_distance( f32 u )
    {
        u = clamp( u, 0.0f, 4.0f );

        const f32 square{ u * u };

        return 1.0f - u / 2.0f +
               square * ( 1.0f / 12.0f + square * ( -1.0f / 720.0f + square * ( 1.0f / 30240.0f ) ) );
    }

    // RemapValClamped: linearly remaps value from [ a, b ] to [ c, d ], clamped to the input range
    ATLAS_FORCE_INLINE constexpr f32 remap_clamped( f32 value, f32 a, f32 b, f32 c, f32 d )
    {
        if ( a == b )
        {
            return value >= b ? d : c;
        }

        const f32 t{ clamp( ( value - a ) / ( b - a ), 0.0f, 1.0f ) };
        return c + ( d - c ) * t;
    }

    ATLAS_FORCE_INLINE constexpr f32 smooth_step( f32 value )
    {
        value = clamp( value, 0.0f, 1.0f );
        return value * value * ( 3.0f - 2.0f * value );
    }

    // quickselect; partially sorts values in place and leaves the nth smallest at that index
    ATLAS_FORCE_INLINE constexpr f32 select_nth( f32 *values, i32 count, i32 nth )
    {
        i32 lo{ 0 };
        i32 hi{ count - 1 };

        while ( lo < hi )
        {
            const i32 mid{ lo + ( hi - lo ) / 2 };
            const f32 a{ values[ lo ] };
            const f32 b{ values[ mid ] };
            const f32 c{ values[ hi ] };
            const f32 pivot{ a < b ? ( b < c ? b : ( a < c ? c : a ) )
                                   : ( a < c ? a : ( b < c ? c : b ) ) };

            i32 i{ lo };
            i32 j{ hi };

            while ( i <= j )
            {
                while ( values[ i ] < pivot )
                {
                    ++i;
                }

                while ( values[ j ] > pivot )
                {
                    --j;
                }

                if ( i <= j )
                {
                    const f32 swap{ values[ i ] };
                    values[ i ] = values[ j ];
                    values[ j ] = swap;
                    ++i;
                    --j;
                }
            }

            if ( nth <= j )
            {
                hi = j;
            } else if ( nth >= i )
            {
                lo = i;
            } else
            {
                return values[ nth ];
            }
        }

        return values[ nth ];
    }

    // an even count averages the two middle values, so the lower half is scanned for its largest
    ATLAS_FORCE_INLINE constexpr f32 median_of( f32 *values, i32 count )
    {
        const f32 upper{ select_nth( values, count, count / 2 ) };

        if ( ( count & 1 ) != 0 )
        {
            return upper;
        }

        f32 lower{ values[ 0 ] };

        for ( i32 at{ 1 }; at < count / 2; ++at )
        {
            lower = bigger( lower, values[ at ] );
        }

        return ( lower + upper ) * 0.5f;
    }

    ATLAS_FORCE_INLINE constexpr f32 sqrt( f32 value )
    {
        if ( value <= 0.0f )
        {
            return 0.0f;
        }

        __m128 input{ _mm_set_ss( value ) };
        __m128 output{ _mm_sqrt_ss( input ) };
        return _mm_cvtss_f32( output );
    }

    ATLAS_FORCE_INLINE constexpr f64 sqrt( f64 value )
    {
        return _mm_cvtsd_f64( _mm_sqrt_sd( _mm_setzero_pd( ), _mm_set_sd( value ) ) );
    }

    ATLAS_FORCE_INLINE constexpr f32 abs( f32 value )
    {
#if defined( MATH_USE_BUILTINS )
        return __builtin_fabs( value );
#else
        return value < 0.0f ? -value : value;
#endif
    }

    ATLAS_FORCE_INLINE constexpr f64 abs( f64 value )
    {
        return value < 0.0 ? -value : value;
    }

#if defined( MATH_USE_BUILTINS )
    template< typename type >
    ATLAS_FORCE_INLINE constexpr bool signbit( type val )
    {
        return __builtin_signbit( val );
    }
#else
    template< typename type >
    ATLAS_FORCE_INLINE constexpr bool signbit( type val )
    {
        if constexpr ( sizeof( type ) == sizeof( f32 ) )
        {
            u32 bits{ };
            atl::memcpy( &bits, &val, sizeof( u32 ) );
            return ( bits & 0x80000000 ) != 0;
        } else if constexpr ( sizeof( type ) == sizeof( f64 ) )
        {
            u64 bits{ };
            atl::memcpy( &bits, &val, sizeof( u64 ) );
            return ( bits & 0x8000000000000000ULL ) != 0;
        }
    }
#endif

    ATLAS_FORCE_INLINE constexpr f32 fmod( f32 value, f32 divisor )
    {
        return value - __builtin_truncf( value / divisor ) * divisor;
    }

    ATLAS_FORCE_INLINE constexpr f32 ceil( f32 value )
    {
        return __builtin_ceilf( value );
    }

    ATLAS_FORCE_INLINE f32 pow( f32 base, f32 exponent )
    {
        return static_cast< f32 >( lua_crt_pow( base, exponent ) );
    }

    ATLAS_FORCE_INLINE constexpr f64 float32_edge_room( f64 value )
    {
        union
        {
            f32 f;
            i32 i;
        } at{ static_cast< f32 >( value ) }, below{ }, above{ };

        below.i = at.i - 1;
        above.i = at.i + 1;

        const f64 low{ ( static_cast< f64 >( below.f ) + static_cast< f64 >( at.f ) ) * 0.5 };
        const f64 high{ ( static_cast< f64 >( at.f ) + static_cast< f64 >( above.f ) ) * 0.5 };

        return smaller( value - low, high - value );
    }

    ATLAS_FORCE_INLINE constexpr f32 sin( f32 value )
    {
        // wrap into [-pi, pi]
        const i32 turns{ static_cast< i32 >( value * ( 1.0f / ( PI * 2 ) ) +
                                             ( value >= 0.0f ? 0.5f : -0.5f ) ) };
        value -= ( PI * 2 ) * static_cast< f32 >( turns );

        if ( value > ( PI / 2 ) )
        {
            value = PI - value;
        } else if ( value < -( PI / 2 ) )
        {
            value = -PI - value;
        }

        const f32 square{ value * value };
        return value * ( 1.0f + square * ( -0.16666667f +
                                           square * ( 0.00833333f + square * -0.000198409f ) ) );
    }

    ATLAS_FORCE_INLINE constexpr f32 cos( f32 value )
    {
        return sin( value + 1.57079632679490f );
    }

    ATLAS_FORCE_INLINE constexpr f32 tan( f32 value )
    {
        return sin( value ) / cos( value );
    }

    ATLAS_FORCE_INLINE constexpr f32 atan( f32 value )
    {
        if ( value > 1.0f || value < -1.0f )
        {
            const f32 folded{ atan( 1.0f / value ) };
            return value > 0.0f ? ( PI / 2 ) - folded : -( PI / 2 ) - folded;
        }

        const f32 square{ value * value };
        // clang-format off
        return value *
            ( 0.99997726f +square *
            ( -0.33262347f +square *
            ( 0.19354346f +square *
            ( -0.11643287f +square *
            ( 0.05265332f + square * -0.01172120f ) ) ) ) );
        // clang-format on
    }

    namespace trig_detail
    {
        inline constexpr f64 PI_D{ math::PI_D };
        inline constexpr f64 HALF_PI{ PI_D * 0.5 };
        inline constexpr f64 QUARTER_PI{ PI_D * 0.25 };
        inline constexpr f64 TAN_PI_OVER_8{ 0.41421356237309504880168872420969808 };

        ATLAS_FORCE_INLINE constexpr f64 atan_reduced( f64 value )
        {
            f64 x{ value };
            f64 base{ 0.0 };
            if ( x > TAN_PI_OVER_8 )
            {
                x    = ( x - 1.0 ) / ( x + 1.0 );
                base = QUARTER_PI;
            }

            const f64 x2{ x * x };
            f64       series{ 1.0 / 21.0 };
            series = -1.0 / 19.0 + x2 * series;
            series = 1.0 / 17.0 + x2 * series;
            series = -1.0 / 15.0 + x2 * series;
            series = 1.0 / 13.0 + x2 * series;
            series = -1.0 / 11.0 + x2 * series;
            series = 1.0 / 9.0 + x2 * series;
            series = -1.0 / 7.0 + x2 * series;
            series = 1.0 / 5.0 + x2 * series;
            series = -1.0 / 3.0 + x2 * series;
            return base + x * ( 1.0 + x2 * series );
        }

        // keep this separate from the f32 atan2 path so existing gameplay math stays bit-stable..
        ATLAS_FORCE_INLINE constexpr f64 atan_reduced_f64( f64 value )
        {
            f64 x{ value };
            f64 base{ 0.0 };
            if ( x > TAN_PI_OVER_8 )
            {
                x    = ( x - 1.0 ) / ( x + 1.0 );
                base = QUARTER_PI;
            }

            const f64 x2{ x * x };
            f64       series{ -1.0 / 35.0 };
            series = 1.0 / 33.0 + x2 * series;
            series = -1.0 / 31.0 + x2 * series;
            series = 1.0 / 29.0 + x2 * series;
            series = -1.0 / 27.0 + x2 * series;
            series = 1.0 / 25.0 + x2 * series;
            series = -1.0 / 23.0 + x2 * series;
            series = 1.0 / 21.0 + x2 * series;
            series = -1.0 / 19.0 + x2 * series;
            series = 1.0 / 17.0 + x2 * series;
            series = -1.0 / 15.0 + x2 * series;
            series = 1.0 / 13.0 + x2 * series;
            series = -1.0 / 11.0 + x2 * series;
            series = 1.0 / 9.0 + x2 * series;
            series = -1.0 / 7.0 + x2 * series;
            series = 1.0 / 5.0 + x2 * series;
            series = -1.0 / 3.0 + x2 * series;
            return base + x * ( 1.0 + x2 * series );
        }

        ATLAS_FORCE_INLINE constexpr void sincos( f64 radians, f64 &sine, f64 &cosine )
        {
            const f64 scaled{ radians / HALF_PI };
            const i32 quadrant{ static_cast< i32 >( scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5 ) };
            const f64 x{ radians - static_cast< f64 >( quadrant ) * HALF_PI };
            const f64 x2{ x * x };

            f64 sin_series{ 1.0 / 355687428096000.0 };
            sin_series = -1.0 / 1307674368000.0 + x2 * sin_series;
            sin_series = 1.0 / 6227020800.0 + x2 * sin_series;
            sin_series = -1.0 / 39916800.0 + x2 * sin_series;
            sin_series = 1.0 / 362880.0 + x2 * sin_series;
            sin_series = -1.0 / 5040.0 + x2 * sin_series;
            sin_series = 1.0 / 120.0 + x2 * sin_series;
            sin_series = -1.0 / 6.0 + x2 * sin_series;
            const f64 sin_poly{ x * ( 1.0 + x2 * sin_series ) };

            f64 cos_series{ 1.0 / 20922789888000.0 };
            cos_series = -1.0 / 87178291200.0 + x2 * cos_series;
            cos_series = 1.0 / 479001600.0 + x2 * cos_series;
            cos_series = -1.0 / 3628800.0 + x2 * cos_series;
            cos_series = 1.0 / 40320.0 + x2 * cos_series;
            cos_series = -1.0 / 720.0 + x2 * cos_series;
            cos_series = 1.0 / 24.0 + x2 * cos_series;
            cos_series = -1.0 / 2.0 + x2 * cos_series;
            const f64 cos_poly{ 1.0 + x2 * cos_series };

            switch ( quadrant & 3 )
            {
                case 0:
                {
                    sine   = sin_poly;
                    cosine = cos_poly;
                    break;
                }

                case 1:
                {
                    sine   = cos_poly;
                    cosine = -sin_poly;
                    break;
                }

                case 2:
                {
                    sine   = -sin_poly;
                    cosine = -cos_poly;
                    break;
                }

                default:
                {
                    sine   = -cos_poly;
                    cosine = sin_poly;
                    break;
                }
            }
        }
    } // namespace trig_detail

    ATLAS_FORCE_INLINE constexpr void sincos( f64 radians, f64 &sine, f64 &cosine )
    {
        trig_detail::sincos( radians, sine, cosine );
    }

    ATLAS_FORCE_INLINE constexpr f32 crt_sinf( f32 x )
    {
#pragma clang fp contract( off )
        const f32 magnitude{ abs( x ) };
        const f32 shifted{ magnitude * 0x1.45f306p-2f + 0x1.8p+23f };
        const f32 turns{ shifted - 0x1.8p+23f };
        f32       reduced{ magnitude - turns * 0x1.92p+1f };
        reduced = reduced - turns * 0x1.fb4p-11f;
        reduced = reduced - turns * 0x1.444p-23f;
        reduced = reduced - turns * 0x1.68c234p-38f;
        const f32 square{ reduced * reduced };
        if ( __builtin_bit_cast( u32, shifted ) & 1u )
        {
            reduced = -reduced;
        }
        const f32 series{ ( ( 0x1.5dbdfp-19f * square + -0x1.9f6ffep-13f ) * square + 0x1.110eccp-7f ) * square + -0x1.55554cp-3f };
        const f32 sine{ reduced + square * series * reduced };
        return __builtin_bit_cast( f32, __builtin_bit_cast( u32, sine ) ^ ( __builtin_bit_cast( u32, x ) & 0x80000000u ) );
    }

    // __libm_sse2_sincosf_ (0x180849b90): below 2^-13 it returns x and 1 - x^2 / 2, else the sines of x and of |x| + pi/2
    ATLAS_FORCE_INLINE constexpr void crt_sincosf( f32 x, f32 &sine, f32 &cosine )
    {
#pragma clang fp contract( off )
        const f32 magnitude{ abs( x ) };
        if ( magnitude < 0x1p-13f )
        {
            sine   = x;
            cosine = 1.0f - magnitude * magnitude * 0.5f;
            return;
        }
        sine   = crt_sinf( x );
        cosine = crt_sinf( magnitude + PI / 2 );
    }

    ATLAS_FORCE_INLINE constexpr f32 atan2( f32 y, f32 x )
    {
        if ( x == 0.0f )
        {
            if ( y > 0.0f )
            {
                return static_cast< f32 >( trig_detail::HALF_PI );
            }
            if ( y < 0.0f )
            {
                return static_cast< f32 >( -trig_detail::HALF_PI );
            }
            return 0.0f;
        }

        const f64 dx{ static_cast< f64 >( x ) };
        const f64 dy{ static_cast< f64 >( y ) };
        const f64 ax{ dx < 0.0 ? -dx : dx };
        const f64 ay{ dy < 0.0 ? -dy : dy };

        f64 angle{ };
        if ( ax >= ay )
        {
            angle = trig_detail::atan_reduced( ay / ax );
        } else
        {
            angle = trig_detail::HALF_PI - trig_detail::atan_reduced( ax / ay );
        }

        if ( dx < 0.0 )
        {
            angle = trig_detail::PI_D - angle;
        }
        if ( dy < 0.0 )
        {
            angle = -angle;
        }
        return static_cast< f32 >( angle );
    }

    ATLAS_FORCE_INLINE constexpr f64 atan2( f64 y, f64 x )
    {
        if ( x == 0.0 )
        {
            return y > 0.0 ? trig_detail::HALF_PI : y < 0.0 ? -trig_detail::HALF_PI
                                                            : 0.0;
        }

        const f64 ax{ abs( x ) };
        const f64 ay{ abs( y ) };
        f64       angle{ ax >= ay ? trig_detail::atan_reduced_f64( ay / ax )
                                  : trig_detail::HALF_PI - trig_detail::atan_reduced_f64( ax / ay ) };
        if ( x < 0.0 )
        {
            angle = trig_detail::PI_D - angle;
        }
        return y < 0.0 ? -angle : angle;
    }

    ATLAS_FORCE_INLINE constexpr f32 acos( f32 value )
    {
        value = clamp( value, -1.0f, 1.0f );
        return atan2( sqrt( 1.0f - value * value ), value );
    }

    ATLAS_FORCE_INLINE constexpr f32 asin( f32 value )
    {
        value = clamp( value, -1.0f, 1.0f );
        return atan2( value, sqrt( 1.0f - value * value ) );
    }

    ATLAS_FORCE_INLINE f64 asin( f64 value )
    {
        value = clamp( value, -1.0, 1.0 );
        return atan2( value, sqrt( bigger( 0.0, 1.0 - value * value ) ) );
    }

    template< typename T >
    ATLAS_FORCE_INLINE T square( T val )
    {
        return val * val;
    }

    ATLAS_FORCE_INLINE constexpr f32 wrap_turn( f32 radians )
    {
        while ( radians > PI )
        {
            radians -= PI * 2.0f;
        }

        while ( radians < -PI )
        {
            radians += PI * 2.0f;
        }

        return radians;
    }

    ATLAS_FORCE_INLINE constexpr i32 cell_axis( f32 value )
    {
        const f32 scaled{ value / 256.0f };
        const i32 truncated{ static_cast< i32 >( scaled ) };
        return truncated - ( scaled < static_cast< f32 >( truncated ) );
    }
} // namespace math
