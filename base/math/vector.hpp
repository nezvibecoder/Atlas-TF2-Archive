#pragma once

#include "math.hpp"

inline constexpr f32 epsilon{ 0.00001f };

struct vec2
{
    f32 x{ };
    f32 y{ };

    constexpr vec2( ) = default;

    constexpr vec2( f32 x_value, f32 y_value ) :
        x{ x_value },
        y{ y_value }
    {
    }

    ATLAS_FORCE_INLINE f32 &operator[]( usize index )
    {
        return ( &this->x )[ index ];
    }

    ATLAS_FORCE_INLINE const f32 &operator[]( usize index ) const
    {
        return ( &this->x )[ index ];
    }

    constexpr bool operator==( const vec2 &other ) const
    {
        return this->x == other.x && this->y == other.y;
    }

    constexpr bool operator!=( const vec2 &other ) const
    {
        return !( *this == other );
    }

    constexpr vec2 operator+( const vec2 &other ) const
    {
        return vec2{ this->x + other.x, this->y + other.y };
    }

    constexpr vec2 operator-( const vec2 &other ) const
    {
        return vec2{ this->x - other.x, this->y - other.y };
    }

    constexpr vec2 operator*( f32 value ) const
    {
        return vec2{ this->x * value, this->y * value };
    }

    constexpr vec2 operator/( f32 value ) const
    {
        return vec2{ this->x / value, this->y / value };
    }

    vec2 &operator+=( const vec2 &other )
    {
        this->x += other.x;
        this->y += other.y;
        return *this;
    }

    vec2 &operator-=( const vec2 &other )
    {
        this->x -= other.x;
        this->y -= other.y;
        return *this;
    }

    vec2 &operator*=( f32 value )
    {
        this->x *= value;
        this->y *= value;
        return *this;
    }

    vec2 &operator/=( f32 value )
    {
        this->x /= value;
        this->y /= value;
        return *this;
    }

    constexpr vec2 operator-( ) const
    {
        return vec2{ -this->x, -this->y };
    }

    void clear( )
    {
        this->x = 0.0f;
        this->y = 0.0f;
    }

    constexpr f32 length_sqr( ) const
    {
        return this->x * this->x + this->y * this->y;
    }

    f32 length( ) const
    {
        return math::sqrt( this->length_sqr( ) );
    }

    f32 length2d( ) const
    {
        return this->length( );
    }

    constexpr f32 length2d_sqr( ) const
    {
        return this->length_sqr( );
    }

    constexpr f32 dot( const vec2 &other ) const
    {
        return this->x * other.x + this->y * other.y;
    }

    constexpr f32 dist_to_sqr( const vec2 &other ) const
    {
        return ( *this - other ).length_sqr( );
    }

    f32 dist_to( const vec2 &other ) const
    {
        return math::sqrt( this->dist_to_sqr( other ) );
    }

    bool is_zero( f32 tolerance = epsilon ) const
    {
        return math::abs( this->x ) <= tolerance && math::abs( this->y ) <= tolerance;
    }

    f32 normalize( )
    {
        f32 length_value{ this->length( ) };

        if ( length_value <= epsilon )
        {
            this->clear( );
            return 0.0f;
        }

        f32 inverse{ 1.0f / length_value };
        this->x *= inverse;
        this->y *= inverse;
        return length_value;
    }

    vec2 normalized( ) const
    {
        vec2 output{ *this };
        output.normalize( );
        return output;
    }
};

inline constexpr vec2 operator*( f32 value, const vec2 &vector )
{
    return vector * value;
}

struct vec3
{
    f32 x{ };
    f32 y{ };
    f32 z{ };

    constexpr vec3( ) = default;

    constexpr vec3( f32 x_value, f32 y_value, f32 z_value ) :
        x{ x_value },
        y{ y_value },
        z{ z_value }
    {
    }

    constexpr vec3( const vec2 &value, f32 z_value ) :
        x{ value.x },
        y{ value.y },
        z{ z_value }
    {
    }

    ATLAS_FORCE_INLINE f32 &operator[]( usize index )
    {
        return ( &this->x )[ index ];
    }

    ATLAS_FORCE_INLINE const f32 &operator[]( usize index ) const
    {
        return ( &this->x )[ index ];
    }

    constexpr bool operator==( const vec3 &other ) const
    {
        return this->x == other.x && this->y == other.y && this->z == other.z;
    }

    constexpr bool operator!=( const vec3 &other ) const
    {
        return !( *this == other );
    }

    constexpr vec3 operator+( const vec3 &other ) const
    {
        return vec3{ this->x + other.x, this->y + other.y, this->z + other.z };
    }

    constexpr vec3 operator-( const vec3 &other ) const
    {
        return vec3{ this->x - other.x, this->y - other.y, this->z - other.z };
    }

    constexpr vec3 operator*( f32 value ) const
    {
        return vec3{ this->x * value, this->y * value, this->z * value };
    }

    constexpr vec3 operator/( f32 value ) const
    {
        return vec3{ this->x / value, this->y / value, this->z / value };
    }

    vec3 &operator+=( const vec3 &other )
    {
        this->x += other.x;
        this->y += other.y;
        this->z += other.z;
        return *this;
    }

    vec3 &operator-=( const vec3 &other )
    {
        this->x -= other.x;
        this->y -= other.y;
        this->z -= other.z;
        return *this;
    }

    vec3 &operator*=( f32 value )
    {
        this->x *= value;
        this->y *= value;
        this->z *= value;
        return *this;
    }

    vec3 &operator/=( f32 value )
    {
        this->x /= value;
        this->y /= value;
        this->z /= value;
        return *this;
    }

    constexpr vec3 operator-( ) const
    {
        return vec3{ -this->x, -this->y, -this->z };
    }

    void clear( )
    {
        this->x = 0.0f;
        this->y = 0.0f;
        this->z = 0.0f;
    }

    constexpr f32 length_sqr( ) const
    {
        return this->x * this->x + this->y * this->y + this->z * this->z;
    }

    constexpr f32 length( ) const
    {
        return math::sqrt( this->length_sqr( ) );
    }

    constexpr f32 length2d_sqr( ) const
    {
        return this->x * this->x + this->y * this->y;
    }

    constexpr f32 length2d( ) const
    {
        return math::sqrt( this->length2d_sqr( ) );
    }

    constexpr f32 dot( const vec3 &other ) const
    {
        return this->x * other.x + this->y * other.y + this->z * other.z;
    }

    constexpr vec3 cross( const vec3 &other ) const
    {
        return vec3{
            this->y * other.z - this->z * other.y,
            this->z * other.x - this->x * other.z,
            this->x * other.y - this->y * other.x,
        };
    }

    constexpr f32 dist_to_sqr( const vec3 &other ) const
    {
        return ( *this - other ).length_sqr( );
    }

    constexpr f32 dist_to( const vec3 &other ) const
    {
        return math::sqrt( this->dist_to_sqr( other ) );
    }

    constexpr bool is_zero( f32 tolerance = epsilon ) const
    {
        return math::abs( this->x ) <= tolerance && math::abs( this->y ) <= tolerance &&
               math::abs( this->z ) <= tolerance;
    }

    constexpr f32 normalize( )
    {
        f32 length_value{ this->length( ) };

        if ( length_value <= epsilon )
        {
            this->clear( );
            return 0.0f;
        }

        f32 inverse{ 1.0f / length_value };
        this->x *= inverse;
        this->y *= inverse;
        this->z *= inverse;
        return length_value;
    }

    constexpr vec3 normalized( ) const
    {
        vec3 output{ *this };
        output.normalize( );
        return output;
    }

    constexpr vec2 to_vec2( ) const
    {
        return vec2{ this->x, this->y };
    }

    constexpr vec3 scale_to( const f32 scale ) const
    {
        return vec3{ x * scale, y * scale, z * scale };
    }
};

struct alignas( 16 ) vec3_aligned : public vec3
{
    constexpr vec3_aligned( ) = default;

    constexpr vec3_aligned( f32 x, f32 y, f32 z ) : vec3( x, y, z )
    {
    }

    explicit vec3_aligned( const vec3 &othr ) : vec3( othr.x, othr.y, othr.z )
    {
    }

    vec3_aligned &operator=( const vec3 &othr )
    {
        x = othr.x;
        y = othr.y;
        z = othr.z;
        return *this;
    }

    f32 w{ 0.0f };
};

inline constexpr vec3 operator*( f32 value, const vec3 &vector )
{
    return vector * value;
}

struct quaternion
{
    f32 x{ };
    f32 y{ };
    f32 z{ };
    f32 w{ };
};

// These live here rather than in math.hpp because that header is the one vec3 is built on top of,
// so it cannot see the type; same namespace either way..
namespace math
{
    ATLAS_FORCE_INLINE constexpr void angle_vectors( const vec3 &angles, vec3 *forward, vec3 *right = nullptr,
                                                     vec3 *up = nullptr )
    {
#pragma clang fp contract( off )
        const f32 pitch{ DEG2RAD( angles.x ) };
        const f32 yaw{ DEG2RAD( angles.y ) };
        const f32 roll{ DEG2RAD( angles.z ) };

        const f32 sin_pitch{ crt_sinf( pitch ) }, cos_pitch{ crt_sinf( pitch + PI / 2 ) };
        const f32 sin_yaw{ crt_sinf( yaw ) }, cos_yaw{ crt_sinf( yaw + PI / 2 ) };
        f32       sin_roll{ }, cos_roll{ };

        if ( forward )
        {
            *forward = { cos_pitch * cos_yaw, cos_pitch * sin_yaw, -sin_pitch };
        }

        if ( right || up )
        {
            crt_sincosf( roll, sin_roll, cos_roll );
        }

        if ( right )
        {
            *right = { -sin_roll * sin_pitch * cos_yaw + cos_roll * sin_yaw,
                       -sin_roll * sin_pitch * sin_yaw - cos_roll * cos_yaw,
                       -sin_roll * cos_pitch };
        }

        if ( up )
        {
            *up = { cos_roll * sin_pitch * cos_yaw + sin_roll * sin_yaw,
                    cos_roll * sin_pitch * sin_yaw - sin_roll * cos_yaw,
                    cos_roll * cos_pitch };
        }
    }

    ATLAS_FORCE_INLINE vec3 closest_on_triangle( const vec3 &p, const vec3 &a, const vec3 &b,
                                                 const vec3 &c )
    {
        const vec3 ab{ b - a };
        const vec3 ac{ c - a };
        const vec3 ap{ p - a };

        const f32 d1{ ab.dot( ap ) };
        const f32 d2{ ac.dot( ap ) };

        if ( d1 <= 0.0f && d2 <= 0.0f )
        {
            return a;
        }

        const vec3 bp{ p - b };
        const f32  d3{ ab.dot( bp ) };
        const f32  d4{ ac.dot( bp ) };

        if ( d3 >= 0.0f && d4 <= d3 )
        {
            return b;
        }

        const f32 vc{ d1 * d4 - d3 * d2 };

        if ( vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f )
        {
            return a + ab * ( d1 / ( d1 - d3 ) );
        }

        const vec3 cp{ p - c };
        const f32  d5{ ab.dot( cp ) };
        const f32  d6{ ac.dot( cp ) };

        if ( d6 >= 0.0f && d5 <= d6 )
        {
            return c;
        }

        const f32 vb{ d5 * d2 - d1 * d6 };

        if ( vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f )
        {
            return a + ac * ( d2 / ( d2 - d6 ) );
        }

        const f32 va{ d3 * d6 - d5 * d4 };

        if ( va <= 0.0f && ( d4 - d3 ) >= 0.0f && ( d5 - d6 ) >= 0.0f )
        {
            return b + ( c - b ) * ( ( d4 - d3 ) / ( ( d4 - d3 ) + ( d5 - d6 ) ) );
        }

        const f32 denominator{ 1.0f / ( va + vb + vc ) };

        return a + ab * ( vb * denominator ) + ac * ( vc * denominator );
    }

    ATLAS_FORCE_INLINE constexpr vec3 nearest_on_box( const vec3 &mins, const vec3 &maxs,
                                                      const vec3 &point )
    {
        return vec3{ clamp( point.x, mins.x, maxs.x ), clamp( point.y, mins.y, maxs.y ),
                     clamp( point.z, mins.z, maxs.z ) };
    }

    ATLAS_FORCE_INLINE constexpr vec3 box_corner( const vec3 &mins, const vec3 &maxs, i32 index )
    {
        return vec3{ ( index & 1 ) ? maxs.x : mins.x, ( index & 2 ) ? maxs.y : mins.y, ( index & 4 ) ? maxs.z : mins.z };
    }

    ATLAS_FORCE_INLINE vec3 lower( const vec3 &a, const vec3 &b )
    {
        return vec3( smaller( a.x, b.x ), smaller( a.y, b.y ), smaller( a.z, b.z ) );
    }

    ATLAS_FORCE_INLINE vec3 upper( const vec3 &a, const vec3 &b )
    {
        return vec3( bigger( a.x, b.x ), bigger( a.y, b.y ), bigger( a.z, b.z ) );
    }

    ATLAS_FORCE_INLINE bool overlaps( const vec3 &a, const vec3 &b, const vec3 &c, const vec3 &d )
    {
        return a.x <= d.x && b.x >= c.x && a.y <= d.y && b.y >= c.y && a.z <= d.z && b.z >= c.z;
    }

    ATLAS_FORCE_INLINE f32 segment_box_fraction( const vec3 &from, const vec3 &to, const vec3 &mins, const vec3 &maxs )
    {
        const vec3 delta{ to - from };
        f32        enter{ 0.0f };
        f32        leave{ 1.0f };

        for ( i32 axis{ }; axis < 3; ++axis )
        {
            if ( delta[ axis ] == 0.0f )
            {
                if ( from[ axis ] < mins[ axis ] || from[ axis ] > maxs[ axis ] )
                {
                    return FLT_MAX;
                }
                continue;
            }

            const f32 low{ ( mins[ axis ] - from[ axis ] ) / delta[ axis ] };
            const f32 high{ ( maxs[ axis ] - from[ axis ] ) / delta[ axis ] };
            enter = bigger( enter, smaller( low, high ) );
            leave = smaller( leave, bigger( low, high ) );
            if ( enter > leave )
            {
                return FLT_MAX;
            }
        }

        return enter;
    }

    // per axis rather than euclidean; what it widens is an axis aligned box
    ATLAS_FORCE_INLINE vec3 axis_offset_from_segment( const vec3 &point, const vec3 &from,
                                                      const vec3 &to )
    {
        const vec3 along{ to - from };
        const f32  length_sqr{ along.dot( along ) };

        const f32 fraction{ length_sqr > 0.0f
                                ? clamp( ( point - from ).dot( along ) / length_sqr, 0.0f, 1.0f )
                                : 0.0f };

        const vec3 nearest{ from + along * fraction };

        return vec3( abs( point.x - nearest.x ), abs( point.y - nearest.y ),
                     abs( point.z - nearest.z ) );
    }

    ATLAS_FORCE_INLINE vec3 closest_on_edge( const vec3 &point, const vec3 &a, const vec3 &b )
    {
        const vec3 along{ b - a };
        const f32  length{ along.length_sqr( ) };
        return a + along * ( length > 0.0f ? clamp( ( point - a ).dot( along ) / length, 0.0f, 1.0f ) : 0.0f );
    }

    ATLAS_FORCE_INLINE f32 vector_normalize( vec3 &vector )
    {
        const f32 length{ vector.length( ) };
        vector *= 1.0f / ( length + FLT_EPSILON );
        return length;
    }

    ATLAS_FORCE_INLINE f32 distance_to_ray( const vec3 &position, const vec3 &start, const vec3 &end )
    {
        vec3      direction{ end - start };
        const f32 length{ vector_normalize( direction ) };
        const f32 along{ direction.dot( position - start ) };

        if ( along < 0.0f )
        {
            return -( position - start ).length( );
        }

        if ( along > length )
        {
            return -( position - end ).length( );
        }

        return ( position - ( start + direction * along ) ).length( );
    }

    ATLAS_FORCE_INLINE constexpr vec3 clip_velocity( const vec3 &velocity, const vec3 &normal )
    {
        vec3 out{ velocity - normal * velocity.dot( normal ) };

        if ( const f32 adjust{ out.dot( normal ) }; adjust < 0.0f )
        {
            out -= normal * adjust;
        }

        return out;
    }

    ATLAS_FORCE_INLINE constexpr void vector_angles( const vec3 &direction, vec3 &angles )
    {
#pragma clang fp contract( off )
        const f32 planar{ sqrt( direction.x * direction.x + direction.y * direction.y ) };
        angles.x = static_cast< f32 >( atan2( static_cast< f64 >( -direction.z ), static_cast< f64 >( planar ) ) * ( 180.0 / PI_D ) );
        angles.y = static_cast< f32 >( atan2( static_cast< f64 >( direction.y ), static_cast< f64 >( direction.x ) ) * ( 180.0 / PI_D ) );
        angles.z = 0.0f;
    }

    ATLAS_FORCE_INLINE constexpr void matrix_angles( const matrix3x4 &matrix, vec3 &angles )
    {
        const vec3 forward = { matrix[ 0 ][ 0 ], matrix[ 1 ][ 0 ], matrix[ 2 ][ 0 ] };
        const vec3 left    = { matrix[ 0 ][ 1 ], matrix[ 1 ][ 1 ], matrix[ 2 ][ 1 ] };
        const vec3 up      = { 0.f, 0.f, matrix[ 2 ][ 2 ] };

        f32 len{ forward.length( ) };

        if ( len > 0.001f )
        {
            angles.x = RAD2DEG( math::atan2( -forward.z, len ) );
            angles.y = RAD2DEG( math::atan2( forward.y, forward.x ) );
            angles.z = RAD2DEG( math::atan2( left.z, up.z ) );
        } else
        {
            angles.x = RAD2DEG( math::atan2( -forward.z, len ) );
            angles.y = RAD2DEG( math::atan2( -left.x, left.y ) );
            angles.z = 0;
        }
    }

    ATLAS_FORCE_INLINE constexpr bool launch_angles( const vec3 &direction, float f, float r, float u, vec3 &angles )
    {
        const double length{ sqrt( static_cast< double >( direction.x ) * direction.x +
                                   static_cast< double >( direction.y ) * direction.y +
                                   static_cast< double >( direction.z ) * direction.z ) };
        const double planar{ sqrt( static_cast< double >( f ) * f + static_cast< double >( u ) * u ) };
        if ( length <= 0.0 || planar <= 0.0 )
        {
            return false;
        }
        const double z{ direction.z / length * sqrt( planar * planar + static_cast< double >( r ) * r ) };
        if ( abs( z ) > planar )
        {
            return false;
        }
        const double pitch{ atan2( static_cast< double >( u ), static_cast< double >( f ) ) - asin( z / planar ) };
        const double along{ sqrt( bigger( planar * planar - z * z, 0.0 ) ) };
        const double yaw{ atan2( static_cast< double >( direction.y ), static_cast< double >( direction.x ) ) +
                          atan2( static_cast< double >( r ), along ) };
        const float  degrees{ static_cast< float >( pitch * ( 180.0 / 3.14159265358979323846 ) ) };
        if ( degrees < -89.0f || degrees > 89.0f )
        {
            return false;
        }
        angles = vec3{ degrees, static_cast< float >( yaw * ( 180.0 / 3.14159265358979323846 ) ), 0.0f };
        if ( angles.y > 180.0f )
        {
            angles.y -= 360.0f;
        }
        if ( angles.y < -180.0f )
        {
            angles.y += 360.0f;
        }
        return true;
    }

    ATLAS_FORCE_INLINE constexpr void clamp_angles( vec3 &angles )
    {
        angles.x = clamp( angles.x, -89.0f, 89.0f );

        while ( angles.y > 180.0f )
        {
            angles.y -= 360.0f;
        }

        while ( angles.y < -180.0f )
        {
            angles.y += 360.0f;
        }

        angles.z = 0.0f;
    }

    ATLAS_FORCE_INLINE constexpr f32 calc_fov( const vec3 &source_va, const vec3 &target_va )
    {
        vec3 source{ };
        vec3 target{ };

        angle_vectors( source_va, &source );
        angle_vectors( target_va, &target );

        source.normalize( );
        target.normalize( );

        auto dot = source.dot( target );
        return RAD2DEG( acos( clamp( dot, -1.0f, 1.0f ) ) );
    }

    ATLAS_FORCE_INLINE constexpr f32 normalize_yaw( f32 yaw )
    {
        while ( yaw > 180.0f )
        {
            yaw -= 360.0f;
        }

        while ( yaw < -180.0f )
        {
            yaw += 360.0f;
        }

        return yaw;
    }

    ATLAS_FORCE_INLINE constexpr void angle_matrix( const vec3 &angles, const vec3 &origin, matrix3x4 out )
    {
        const f32 sin_pitch{ sin( DEG2RAD( angles.x ) ) };
        const f32 cos_pitch{ cos( DEG2RAD( angles.x ) ) };
        const f32 sin_yaw{ sin( DEG2RAD( angles.y ) ) };
        const f32 cos_yaw{ cos( DEG2RAD( angles.y ) ) };
        const f32 sin_roll{ sin( DEG2RAD( angles.z ) ) };
        const f32 cos_roll{ cos( DEG2RAD( angles.z ) ) };

        out[ 0 ][ 0 ] = cos_pitch * cos_yaw;
        out[ 1 ][ 0 ] = cos_pitch * sin_yaw;
        out[ 2 ][ 0 ] = -sin_pitch;

        out[ 0 ][ 1 ] = sin_roll * sin_pitch * cos_yaw - cos_roll * sin_yaw;
        out[ 1 ][ 1 ] = sin_roll * sin_pitch * sin_yaw + cos_roll * cos_yaw;
        out[ 2 ][ 1 ] = sin_roll * cos_pitch;

        out[ 0 ][ 2 ] = cos_roll * sin_pitch * cos_yaw + sin_roll * sin_yaw;
        out[ 1 ][ 2 ] = cos_roll * sin_pitch * sin_yaw - sin_roll * cos_yaw;
        out[ 2 ][ 2 ] = cos_roll * cos_pitch;

        out[ 0 ][ 3 ] = origin.x;
        out[ 1 ][ 3 ] = origin.y;
        out[ 2 ][ 3 ] = origin.z;
    }

    ATLAS_FORCE_INLINE constexpr void quaternion_matrix( const quaternion &rotation, const vec3 &origin, matrix3x4 out )
    {
        out[ 0 ][ 0 ] = 1.0f - 2.0f * rotation.y * rotation.y - 2.0f * rotation.z * rotation.z;
        out[ 1 ][ 0 ] = 2.0f * rotation.x * rotation.y + 2.0f * rotation.w * rotation.z;
        out[ 2 ][ 0 ] = 2.0f * rotation.x * rotation.z - 2.0f * rotation.w * rotation.y;

        out[ 0 ][ 1 ] = 2.0f * rotation.x * rotation.y - 2.0f * rotation.w * rotation.z;
        out[ 1 ][ 1 ] = 1.0f - 2.0f * rotation.x * rotation.x - 2.0f * rotation.z * rotation.z;
        out[ 2 ][ 1 ] = 2.0f * rotation.y * rotation.z + 2.0f * rotation.w * rotation.x;

        out[ 0 ][ 2 ] = 2.0f * rotation.x * rotation.z + 2.0f * rotation.w * rotation.y;
        out[ 1 ][ 2 ] = 2.0f * rotation.y * rotation.z - 2.0f * rotation.w * rotation.x;
        out[ 2 ][ 2 ] = 1.0f - 2.0f * rotation.x * rotation.x - 2.0f * rotation.y * rotation.y;

        out[ 0 ][ 3 ] = origin.x;
        out[ 1 ][ 3 ] = origin.y;
        out[ 2 ][ 3 ] = origin.z;
    }

    ATLAS_FORCE_INLINE constexpr void concat_transforms( const matrix3x4 parent, const matrix3x4 local, matrix3x4 out )
    {
        const __m128 row0{ _mm_loadu_ps( local[ 0 ] ) };
        const __m128 row1{ _mm_loadu_ps( local[ 1 ] ) };
        const __m128 row2{ _mm_loadu_ps( local[ 2 ] ) };

        for ( i32 row{ }; row < 3; ++row )
        {
            __m128 result{ _mm_mul_ps( _mm_set_ps1( parent[ row ][ 0 ] ), row0 ) };

            result = _mm_add_ps( result, _mm_mul_ps( _mm_set_ps1( parent[ row ][ 1 ] ), row1 ) );
            result = _mm_add_ps( result, _mm_mul_ps( _mm_set_ps1( parent[ row ][ 2 ] ), row2 ) );
            result = _mm_add_ps( result, _mm_set_ps( parent[ row ][ 3 ], 0.0f, 0.0f, 0.0f ) );

            _mm_storeu_ps( out[ row ], result );
        }
    }

    ATLAS_FORCE_INLINE constexpr void vector_transform( const vec3 &input, const matrix3x4 &matrix, vec3 &output )
    {
        for ( i32 n{ }; n < 3; n++ )
        {
            output[ n ] = input.dot( ( vec3 & )matrix[ n ] ) + matrix[ n ][ 3 ];
        }
    }

    ATLAS_FORCE_INLINE vec3 transform_point( const vec3 &point, const matrix3x4 &matrix )
    {
        vec3 result{ };
        vector_transform( point, matrix, result );
        return result;
    }

    ATLAS_FORCE_INLINE void vector_itransform( const vec3 &input, const matrix3x4 &matrix, vec3 &output )
    {
        const vec3 delta{ input.x - matrix[ 0 ][ 3 ], input.y - matrix[ 1 ][ 3 ], input.z - matrix[ 2 ][ 3 ] };

        for ( i32 axis{ }; axis < 3; ++axis )
        {
            const vec3 column{ matrix[ 0 ][ axis ], matrix[ 1 ][ axis ], matrix[ 2 ][ axis ] };
            const f32  scale{ column.length_sqr( ) };

            output[ axis ] = scale > epsilon ? delta.dot( column ) / scale : 0.0f;
        }
    }

    ATLAS_FORCE_INLINE bool ray_hits_box(
        const vec3      &start,
        const vec3      &through,
        const matrix3x4 &matrix,
        const vec3      &mins,
        const vec3      &maxs,
        f32              shrink,
        f32             *entry = nullptr )
    {
        vec3 local_start{ };
        vec3 local_through{ };

        vector_itransform( start, matrix, local_start );
        vector_itransform( through, matrix, local_through );

        const vec3 direction{ local_through - local_start };

        f32 near_t{ 0.0f };
        f32 far_t{ FLT_MAX };

        for ( i32 axis{ }; axis < 3; ++axis )
        {
            const f32 slope{ direction[ axis ] };
            const f32 minimum{ mins[ axis ] + shrink };
            const f32 maximum{ maxs[ axis ] - shrink };

            if ( minimum >= maximum )
            {
                return false;
            }

            if ( abs( slope ) < epsilon )
            {
                if ( local_start[ axis ] < minimum || local_start[ axis ] > maximum )
                {
                    return false;
                }

                continue;
            }

            const f32 inverse{ 1.0f / slope };

            f32 axis_near{ ( minimum - local_start[ axis ] ) * inverse };
            f32 axis_far{ ( maximum - local_start[ axis ] ) * inverse };

            if ( axis_near > axis_far )
            {
                const f32 swap{ axis_near };
                axis_near = axis_far;
                axis_far  = swap;
            }

            near_t = bigger( near_t, axis_near );
            far_t  = smaller( far_t, axis_far );

            if ( near_t > far_t )
            {
                return false;
            }
        }

        if ( entry )
        {
            *entry = near_t;
        }

        return true;
    }
} // namespace math
