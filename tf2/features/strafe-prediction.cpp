#include "strafe-prediction.hpp"
#include "move_sim.hpp"
#include "player_data.hpp"
#include "runtime/thread_pool.hpp"

constexpr f32 DUCK_SPEED_CROP{ 0.33333333f };
constexpr f32 NON_GROUND_VELOCITY{ 250.0f };
constexpr f32 JUMP_VELOCITY{ 289.0f }; // @ CTFGameMovement::CheckJumpButton

static ATLAS_FORCE_INLINE f32 wrap( f32 value )
{
    while ( value >= 180.f )
    {
        value -= 360.f;
    }
    while ( value < -180.f )
    {
        value += 360.f;
    }
    return value;
}

static ATLAS_FORCE_INLINE f32 heading( f32 x, f32 y )
{
    return RAD2DEG( math::atan2( y, x ) );
}

static ATLAS_FORCE_INLINE f32 surface( f32 value, f32 air_friction = 1.0f )
{
    if ( value > 250.0f )
    {
        return 1.0f;
    }
    return ( value > 0.0f ? 0.25f : 1.0f ) * air_friction;
}

static ATLAS_FORCE_INLINE bool retracks( strafe_prediction::model kind )
{
    return kind == strafe_prediction::model::retrack || kind == strafe_prediction::model::retrack_drift;
}

static ATLAS_FORCE_INLINE f32 center( f32 value, f32 quantum )
{
    return value == 0.f ? 0.f : value + ( value > 0.f ? quantum : -quantum ) * 0.5f;
}

static ATLAS_FORCE_INLINE f32 climbing_surface_friction( f32 velocity_z, f32 grounded,
                                                         f32 air_friction = 1.0f )
{
    return velocity_z > 0.0f && velocity_z <= NON_GROUND_VELOCITY ? 0.25f * air_friction : grounded;
}

static ATLAS_FORCE_INLINE u64 hash_bytes( const void *data, usize size, u64 hash )
{
    const u8 *bytes{ static_cast< const u8 * >( data ) };
    for ( usize at{ }; at < size; ++at )
    {
        hash = ( hash ^ bytes[ at ] ) * 1099511628211ULL;
    }
    return hash;
}

template< typename T >
static ATLAS_FORCE_INLINE u64 hash_value( const T &value, u64 hash )
{
    return hash_bytes( &value, sizeof( value ), hash );
}

namespace air_inference
{
    inline f64 length( f64 x, f64 y )
    {
        return math::sqrt( x * x + y * y );
    }
    inline f64 wrap( f64 x )
    {
        while ( x >= 180.0 )
        {
            x -= 360.0;
        }
        while ( x < -180.0 )
        {
            x += 360.0;
        }
        return x;
    }
    inline f64 heading( f64 x, f64 y )
    {
        return math::atan2( y, x ) * ( 180.0 / math::PI_D );
    }

    struct physics
    {
        f64  dt{ }, gravity{ }, acceleration{ }, cap{ }, maxspeed{ }, quantum{ }, air_friction{ }, maxvelocity{ };
        bool operator==( const physics &other ) const = default;
    };
    struct observation
    {
        f64 x{ }, y{ }, z{ }, vx{ }, vy{ }, vz{ };
        i32 tick{ }, gap{ }, recategorize{ };
    };
    struct state
    {
        f64 x{ }, y{ }, z{ }, vx{ }, vy{ }, vz{ }, friction{ };
        i32 tick{ };
    };
    struct control
    {
        i32 kind{ }, epoch{ };
        f64 yaw{ }, speed{ }, rate{ }, budget{ };
    };
    inline f64 friction( f64 vz, const physics &environment )
    {
        return vz > 250.0 ? 1.0 : ( vz > 0.0 ? 0.25 : 1.0 ) * environment.air_friction;
    }
    inline f64 cell_low( f64 value, f64 q )
    {
        return value - ( value <= 0.0 ? q : 0.0 );
    }
    inline f64 cell_high( f64 value, f64 q )
    {
        return value + ( value >= 0.0 ? q : 0.0 );
    }
    inline f64 center( f64 value, f64 q )
    {
        return ( cell_low( value, q ) + cell_high( value, q ) ) * 0.5;
    }

    inline f64 cell_miss( f64 actual, f64 low, f64 high )
    {
        if ( actual < low )
        {
            return low - actual;
        }
        if ( actual > high )
        {
            return actual - high;
        }
        return 0.0;
    }

    inline physics widen( const strafe_prediction::physics &e )
    {
        return { e.dt, e.gravity, e.acceleration, e.cap, e.maxspeed, e.quantum, e.air_friction, e.max_velocity };
    }
    inline state widen( const strafe_prediction::state &s )
    {
        return { s.origin.x, s.origin.y, s.origin.z, s.velocity.x, s.velocity.y, s.velocity.z, s.friction, s.tick };
    }
    inline control widen( const strafe_prediction::control &c )
    {
        return { static_cast< i32 >( c.kind ), c.epoch, c.yaw, c.speed, c.rate, c.beta };
    }
    inline strafe_prediction::state narrow( const state &s )
    {
        return { { static_cast< f32 >( s.x ), static_cast< f32 >( s.y ), static_cast< f32 >( s.z ) },
                 { static_cast< f32 >( s.vx ), static_cast< f32 >( s.vy ), static_cast< f32 >( s.vz ) },
                 static_cast< f32 >( s.friction ),
                 s.tick };
    }
    inline f64 wish_direction( const state &value, const control &input, const physics &environment, f64 &wish, bool *used_phase = nullptr )
    {
        f64 yaw{ input.yaw };
        wish = input.speed;
        if ( input.kind == 1 )
        {
            yaw += heading( value.vx, value.vy );
        }
        if ( input.kind == 3 || input.kind == 5 )
        {
            yaw += input.rate * ( value.tick - input.epoch );
        }
        if ( input.kind == 4 || input.kind == 5 )
        {
            const f64 h{ heading( value.vx, value.vy ) }, error{ wrap( yaw - h ) };
            yaw = h + ( error > 0.0 ? 90.0 : -90.0 ) + input.budget * error;
        }
        if ( input.kind >= 6 )
        {
            const f64 speed{ length( value.vx, value.vy ) }, h{ heading( value.vx, value.vy ) };
            if ( input.kind == 6 )
            {
                f64 sn{ }, cs{ };
                math::sincos( math::abs( input.rate ) * math::PI_D / 360.0, sn, cs );
                wish = environment.acceleration * environment.dt * value.friction > 0.0
                           ? 2.0 * speed * sn / ( environment.acceleration * environment.dt * value.friction )
                           : 0.0;
                yaw  = h + ( input.rate > 0.0 ? 90.0 : -90.0 ) + input.rate * 0.5;
            } else if ( input.kind == 7 || input.kind == 8 )
            {
                if ( speed <= input.budget )
                {
                    return yaw;
                }
                const f64 limit{ math::asin( input.budget / speed ) * 180.0 / math::PI_D };
                const f64 gain{ math::asin( input.budget * 0.5 / speed ) * 180.0 / math::PI_D };
                const f64 error{ wrap( input.yaw - h ) };
                if ( math::abs( error ) > limit )
                {
                    yaw = h + ( error > 0.0 ? 1.0 : -1.0 ) * ( 90.0 + limit );
                } else
                {
                    if ( used_phase )
                    {
                        *used_phase = true;
                    }
                    yaw =
                        input.yaw + ( ( value.tick - input.epoch ) & 1 ? 1.0 : -1.0 ) * ( input.kind == 7 ? 1.0 : -1.0 ) * ( 90.0 - gain );
                }
            }
        }
        wish = math::clamp( wish, 0.0, environment.maxspeed );
        return yaw;
    }

    struct basis
    {
        f64 x{ }, y{ }, half_sine{ };
    };

    inline f64 advance_coordinate( f64 position, f64 velocity, f64 dt )
    {
#pragma clang fp contract( off )
        return position + velocity * dt;
    }

    inline basis prepare( const control &input )
    {
        basis     out{ };
        const f64 yaw{ input.kind == 6 ? ( input.rate > 0.0 ? 90.0 : -90.0 ) + input.rate * 0.5 : input.yaw };
        math::sincos( yaw * math::PI_D / 180.0, out.y, out.x );
        if ( input.kind == 6 )
        {
            f64 unused{ };
            math::sincos( math::abs( input.rate ) * math::PI_D / 360.0, out.half_sine, unused );
        }
        return out;
    }

    inline void cartesian_direction( const state &value, const control &input, const physics &environment, const basis &prepared, f64 &x,
                                     f64 &y, f64 &wish, bool *used_phase )
    {
        const f64 speed{ length( value.vx, value.vy ) };
        wish = input.speed;
        if ( input.kind == 6 && speed > 0.0 )
        {
            x    = ( value.vx * prepared.x - value.vy * prepared.y ) / speed;
            y    = ( value.vy * prepared.x + value.vx * prepared.y ) / speed;
            wish = environment.acceleration * environment.dt * value.friction > 0.0
                       ? 2.0 * speed * prepared.half_sine / ( environment.acceleration * environment.dt * value.friction )
                       : 0.0;
        } else if ( input.kind >= 7 && speed > input.budget )
        {
            const f64 cross{ value.vx * prepared.y - value.vy * prepared.x }, dot{ value.vx * prepared.x + value.vy * prepared.y };
            const f64 rounding{ 128.0 * 2.2204460492503131e-16 *
                                ( math::abs( value.vx ) + math::abs( value.vy ) + math::abs( input.budget ) + 1.0 ) };
            if ( math::abs( math::abs( cross ) - input.budget ) <= rounding || math::abs( dot ) <= rounding ||
                 ( dot < 0.0 && math::abs( cross ) <= rounding ) || speed - input.budget <= rounding )
            {
                const f64 yaw{ wish_direction( value, input, environment, wish, used_phase ) };
                math::sincos( yaw * math::PI_D / 180.0, y, x );
            } else if ( dot < 0.0 || math::abs( cross ) > input.budget )
            {
                const f64 sine{ input.budget / speed }, cosine{ math::sqrt( math::bigger( 0.0, 1.0 - sine * sine ) ) };
                const f64 side{ cross > 0.0 ? 1.0 : -1.0 };
                x = ( -sine * value.vx - side * cosine * value.vy ) / speed;
                y = ( -sine * value.vy + side * cosine * value.vx ) / speed;
            } else
            {
                if ( used_phase )
                {
                    *used_phase = true;
                }
                const f64 sine{ input.budget * 0.5 / speed }, cosine{ math::sqrt( math::bigger( 0.0, 1.0 - sine * sine ) ) };
                const f64 side{ ( ( value.tick - input.epoch ) & 1 ? 1.0 : -1.0 ) * ( input.kind == 7 ? 1.0 : -1.0 ) };
                x = prepared.x * sine - side * prepared.y * cosine;
                y = prepared.y * sine + side * prepared.x * cosine;
            }
        } else if ( input.kind == 2 )
        {
            x = prepared.x;
            y = prepared.y;
        } else if ( input.kind == 1 && speed > 0.0 )
        {
            x = ( value.vx * prepared.x - value.vy * prepared.y ) / speed;
            y = ( value.vy * prepared.x + value.vx * prepared.y ) / speed;
        } else
        {
            const f64 yaw{ wish_direction( value, input, environment, wish, used_phase ) };
            math::sincos( yaw * math::PI_D / 180.0, y, x );
        }
        wish = math::clamp( wish, 0.0, environment.maxspeed );
    }

    inline void step( state &value, const control &input, const physics &environment, bool *used_phase = nullptr,
                      const basis *cached = nullptr, f64 *moved_x = nullptr, f64 *moved_y = nullptr )
    {
        value.vz -= environment.gravity * environment.dt * 0.5;
        value.vx = math::clamp( value.vx, -environment.maxvelocity, environment.maxvelocity );
        value.vy = math::clamp( value.vy, -environment.maxvelocity, environment.maxvelocity );
        value.vz = math::clamp( value.vz, -environment.maxvelocity, environment.maxvelocity );
        if ( input.kind )
        {
            f64         wish{ }, y{ }, x{ };
            const basis local{ cached ? *cached : prepare( input ) };
            cartesian_direction( value, input, environment, local, x, y, wish, used_phase );
            if ( wish > 0.0 )
            {
                const f64 gain{ math::bigger( 0.0, math::smaller( math::smaller( wish, environment.cap ) - value.vx * x - value.vy * y,
                                                                  environment.acceleration * wish * environment.dt * value.friction ) ) };
                value.vx += gain * x;
                value.vy += gain * y;
            }
        }
        if ( moved_x )
        {
            *moved_x = value.vx;
        }
        if ( moved_y )
        {
            *moved_y = value.vy;
        }
        value.x        = advance_coordinate( value.x, value.vx, environment.dt );
        value.y        = advance_coordinate( value.y, value.vy, environment.dt );
        value.z        = advance_coordinate( value.z, value.vz, environment.dt );
        value.friction = friction( value.vz, environment );
        value.vz -= environment.gravity * environment.dt * 0.5;
        value.vx = math::clamp( value.vx, -environment.maxvelocity, environment.maxvelocity );
        value.vy = math::clamp( value.vy, -environment.maxvelocity, environment.maxvelocity );
        value.vz = math::clamp( value.vz, -environment.maxvelocity, environment.maxvelocity );
        ++value.tick;
    }

    inline constexpr i32 identification_samples{ 67 };
    inline constexpr i32 identification_families{ static_cast< i32 >( strafe_prediction::model::count ) };
    inline constexpr i32 identification_anchors{ 4 };
    inline constexpr u32 identification_state{ 0x0Fu };
    inline constexpr f64 unfit{ 1e300 };
    inline constexpr f64 cell_sigma{ 0.28867513459481288225 };
    inline constexpr f64 inverse_cell_sigma{ 3.4641016151377545871 };

    struct stationary_fit
    {
        f64     parameter[ 8 ]{ };
        f64     square{ unfit }, max_cell{ unfit }, score{ unfit };
        state   initial{ }, terminal{ };
        control input{ }, prefix{ };
        i32     dimensions{ }, observations{ };
        i32     change{ };
        u32     mask{ };
        bool    split{ };
    };

    struct stationary_cache
    {
        stationary_fit families[ identification_families ]{ };
        stationary_fit anchors[ identification_anchors ]{ };
        stationary_fit split{ };
        physics        environment{ };
        i32            tick{ }, selected{ -1 }, next_family{ }, recovery_first_tick{ };
        i32            floor_tick{ };
        i32            anchor_count{ };
        bool           ready{ };
    };

    struct stationary_anchor
    {
        control input{ };
        state   origin{ };
        i32     tick{ };
        i32     first_tick{ };
        i32     support{ };
    };

    struct stationary_seed
    {
        f64 parameter[ 8 ]{ };
        i32 kind{ };
    };

    inline constexpr u64 stationary_search_steps{ 8192 };

    struct stationary_cost
    {
        u64  steps{ };
        u64  limit{ stationary_search_steps };
        u64  ceiling{ ~0ull };
        bool exhausted{ };
    };

    struct stationary_result
    {
        stationary_fit  chosen{ };
        stationary_cost cost{ };
        i32             count{ };
        bool            consistent{ };
    };

    struct stationary_window
    {
        const observation *samples{ };
        i32                count{ };
        physics            environment{ };
        f64                z{ }, vz{ }, sigma{ }, log_count{ };
        stationary_cost   *cost{ };
        control            prefix{ };
        f64                seed[ 4 ]{ };
        i32                change{ };
        u32                mask{ };
        bool               split{ }, seeded{ };
        f64                low[ identification_samples * 2 ], high[ identification_samples * 2 ];
        f64                middle[ identification_samples * 2 ], spread[ identification_samples * 2 ];
    };

    struct stationary_motion
    {
        f64 vx{ }, vy{ }, first_heading{ }, last_heading{ }, target_heading{ };
        f64 early_turn{ }, late_turn{ }, mean_turn{ }, extent{ };
        f64 acceleration_yaw{ }, acceleration_rate{ }, wish{ }, sign{ 1.0 };
    };

    inline f64 stationary_log( i32 count )
    {
        f64 value{ static_cast< f64 >( count ) };
        i32 exponent{ };
        while ( value >= 2.0 )
        {
            value *= 0.5;
            ++exponent;
        }
        const f64 z{ ( value - 1.0 ) / ( value + 1.0 ) }, square{ z * z };
        f64       term{ z }, sum{ };
        for ( i32 n{ 1 }; n < 33; n += 2 )
        {
            sum += term / n;
            term *= square;
        }
        return exponent * 0.69314718055994530942 + 2.0 * sum;
    }

    inline stationary_window stationary_window_of( const observation *samples, i32 count, const physics &environment,
                                                   stationary_cost &cost )
    {
        return { samples, count, environment, 0.0, 0.0, environment.quantum * cell_sigma, stationary_log( count * 2 ), &cost };
    }

    inline i32 stationary_dimensions( i32 kind, i32 *active )
    {
        for ( i32 i{ }; i < 4; ++i )
        {
            active[ i ] = i;
        }
        i32 count{ 4 };
        if ( kind && kind != 6 )
        {
            active[ count++ ] = 4;
        }
        if ( kind == 3 || kind == 5 || kind == 6 )
        {
            active[ count++ ] = 5;
        }
        if ( kind && kind != 6 )
        {
            active[ count++ ] = 6;
        }
        if ( kind == 4 || kind == 5 || kind >= 7 )
        {
            active[ count++ ] = 7;
        }
        return count;
    }

    inline bool stationary_turns_to_goal( i32 kind )
    {
        return kind == static_cast< i32 >( strafe_prediction::model::retrack ) ||
               kind == static_cast< i32 >( strafe_prediction::model::phase_left ) ||
               kind == static_cast< i32 >( strafe_prediction::model::phase_right );
    }

    struct stationary_challenge
    {
        stationary_fit held{ }, rival{ };
        bool           valid{ };
    };

    inline bool stationary_height( stationary_window &in )
    {
        const auto &first{ in.samples[ 0 ] };
        f64         st{ }, sh{ }, stt{ }, sth{ };
        for ( i32 i{ }; i < in.count; ++i )
        {
            for ( i32 axis{ }; axis < 2; ++axis )
            {
                const f64 measured{ axis ? in.samples[ i ].y : in.samples[ i ].x };
                const i32 row{ i * 2 + axis };
                in.low[ row ]    = cell_low( measured, in.environment.quantum );
                in.high[ row ]   = cell_high( measured, in.environment.quantum );
                in.middle[ row ] = ( in.low[ row ] + in.high[ row ] ) * 0.5;
                in.spread[ row ] = ( in.high[ row ] - in.low[ row ] ) * cell_sigma;
            }
            const f64 t{ ( in.samples[ i ].tick - first.tick ) * in.environment.dt };
            const f64 h{ center( in.samples[ i ].z, in.environment.quantum ) + 0.5 * in.environment.gravity * t * t };
            st += t;
            sh += h;
            stt += t * t;
            sth += t * h;
        }
        const f64 determinant{ in.count * stt - st * st };
        if ( determinant <= 0.0 )
        {
            return false;
        }
        in.vz = ( in.count * sth - st * sh ) / determinant;
        in.z  = ( sh - in.vz * st ) / in.count;
        if ( in.vz < -in.environment.maxvelocity || in.vz > in.environment.maxvelocity )
        {
            return false;
        }
        for ( i32 i{ }; i < in.count; ++i )
        {
            const f64 t{ ( in.samples[ i ].tick - first.tick ) * in.environment.dt };
            const f64 predicted{ in.z + in.vz * t - 0.5 * in.environment.gravity * t * t };
            const f64 low{ cell_low( in.samples[ i ].z, in.environment.quantum ) },
                high{ cell_high( in.samples[ i ].z, in.environment.quantum ) };
            if ( predicted < low - in.environment.quantum * 0.5 || predicted > high + in.environment.quantum * 0.5 )
            {
                return false;
            }
        }
        return true;
    }

    inline f64 stationary_slope( const f64 *time, const f64 *value, i32 first, i32 end )
    {
        if ( end - first < 2 )
        {
            return 0.0;
        }
        f64 st{ }, sv{ }, stt{ }, stv{ };
        for ( i32 i{ first }; i < end; ++i )
        {
            st += time[ i ];
            sv += value[ i ];
            stt += time[ i ] * time[ i ];
            stv += time[ i ] * value[ i ];
        }
        const f64 denominator{ ( end - first ) * stt - st * st };
        return denominator > 0.0 ? ( ( end - first ) * stv - st * sv ) / denominator : 0.0;
    }

    inline stationary_motion stationary_kinematics( const stationary_window &in )
    {
        stationary_motion out{ };
        f64               time[ identification_samples ]{ }, heads[ identification_samples ]{ };
        f64               vx[ identification_samples ]{ }, vy[ identification_samples ]{ };
        f64               acceleration_time[ identification_samples ]{ }, acceleration_heading[ identification_samples ]{ };
        i32               accelerations{ };
        const i32         n{ in.count - 1 };
        for ( i32 i{ }; i < n; ++i )
        {
            const auto &a{ in.samples[ i ] }, &b{ in.samples[ i + 1 ] };
            const f64   span{ static_cast< f64 >( b.tick - a.tick ) };
            vx[ i ]   = ( b.x - a.x ) / ( span * in.environment.dt );
            vy[ i ]   = ( b.y - a.y ) / ( span * in.environment.dt );
            time[ i ] = 0.5 * ( a.tick + b.tick ) - in.samples[ 0 ].tick;
            const f64 h{ heading( vx[ i ], vy[ i ] ) };
            heads[ i ] = i ? heads[ i - 1 ] + wrap( h - heads[ i - 1 ] ) : h;
            if ( i )
            {
                const f64 ticks{ time[ i ] - time[ i - 1 ] };
                const f64 ax{ ( vx[ i ] - vx[ i - 1 ] ) / ticks }, ay{ ( vy[ i ] - vy[ i - 1 ] ) / ticks };
                const f64 magnitude{ length( ax, ay ) };
                const f64 noise{ in.environment.quantum / in.environment.dt *
                                 ( 1.0 / span + 1.0 / ( in.samples[ i ].tick - in.samples[ i - 1 ].tick ) ) / ticks };
                if ( magnitude > noise )
                {
                    const f64 yaw{ heading( ax, ay ) };
                    acceleration_time[ accelerations ] = time[ i ];
                    acceleration_heading[ accelerations ] =
                        accelerations ? acceleration_heading[ accelerations - 1 ] + wrap( yaw - acceleration_heading[ accelerations - 1 ] )
                                      : yaw;
                    ++accelerations;
                    out.wish =
                        magnitude / ( in.environment.acceleration * in.environment.dt * math::bigger( 0.25, in.environment.air_friction ) );
                }
            }
        }
        out.vx            = vx[ 0 ];
        out.vy            = vy[ 0 ];
        out.first_heading = heads[ 0 ];
        out.last_heading  = heads[ n - 1 ];
        out.mean_turn     = stationary_slope( time, heads, 0, n );
        out.early_turn    = stationary_slope( time, heads, 0, math::smaller( n, math::bigger( 2, n / 3 ) ) );
        out.late_turn     = stationary_slope( time, heads, math::bigger( 0, n - math::bigger( 3, n / 3 ) ), n );
        out.extent        = math::abs( heads[ n - 1 ] - heads[ 0 ] );
        out.sign          = out.mean_turn + out.late_turn >= 0.0 ? 1.0 : -1.0;
        f64 tx{ }, ty{ };
        for ( i32 i{ math::bigger( 0, n - 5 ) }; i < n; ++i )
        {
            tx += vx[ i ];
            ty += vy[ i ];
        }
        out.target_heading    = heading( tx, ty );
        out.acceleration_rate = stationary_slope( acceleration_time, acceleration_heading, 0, accelerations );
        out.acceleration_yaw  = accelerations ? acceleration_heading[ 0 ] - out.acceleration_rate * acceleration_time[ 0 ]
                                              : out.first_heading + out.sign * 90.0;
        out.wish              = math::clamp( out.wish, 0.0, in.environment.maxspeed );
        return out;
    }

    inline control stationary_control( i32 kind, const f64 *p, i32 epoch )
    {
        return { kind, epoch, p[ 4 ], kind == 0 ? 0.0 : p[ 6 ], kind == 3 || kind == 5 || kind == 6 ? p[ 5 ] : 0.0, p[ 7 ] };
    }

    inline void stationary_constrain( const stationary_window &in, i32 kind, f64 *p )
    {
        p[ 0 ] = math::clamp( p[ 0 ], cell_low( in.samples[ 0 ].x, in.environment.quantum ),
                              cell_high( in.samples[ 0 ].x, in.environment.quantum ) );
        p[ 1 ] = math::clamp( p[ 1 ], cell_low( in.samples[ 0 ].y, in.environment.quantum ),
                              cell_high( in.samples[ 0 ].y, in.environment.quantum ) );
        p[ 2 ] = math::clamp( p[ 2 ], -in.environment.maxvelocity, in.environment.maxvelocity );
        p[ 3 ] = math::clamp( p[ 3 ], -in.environment.maxvelocity, in.environment.maxvelocity );
        p[ 4 ] = wrap( p[ 4 ] );
        p[ 5 ] = math::clamp( p[ 5 ], -20.0, 20.0 );
        p[ 6 ] = math::clamp( p[ 6 ], 0.0, in.environment.maxspeed );
        if ( kind == 4 || kind == 5 )
        {
            p[ 7 ] = math::clamp( p[ 7 ], 0.0, 0.2 );
        }
        if ( kind >= 7 )
        {
            p[ 7 ] = math::clamp( p[ 7 ], 0.0, in.environment.cap );
        }
    }

    inline f64 stationary_residuals( stationary_window &in, i32 kind, const f64 *p, f64 *residual = nullptr, stationary_fit *fit = nullptr,
                                     f64 reject = unfit )
    {
        const u64 span{ static_cast< u64 >( in.samples[ in.count - 1 ].tick - in.samples[ 0 ].tick ) };
        if ( !fit && in.cost->steps + span > in.cost->limit )
        {
            in.cost->exhausted = true;
            return unfit;
        }
        state         value{ p[ 0 ],
                             p[ 1 ],
                             in.z,
                             p[ 2 ],
                             p[ 3 ],
                             in.vz,
                             friction( in.vz + in.environment.gravity * in.environment.dt * 0.5, in.environment ),
                             in.samples[ 0 ].tick };
        const state   initial{ value };
        const control input{ stationary_control( kind, p, value.tick ) };
        const basis   prepared{ prepare( input ) }, before{ in.split ? prepare( in.prefix ) : basis{ } };
        f64           square{ }, max_cell{ };
        for ( i32 at{ }; at < in.count; ++at )
        {
            const auto &o{ in.samples[ at ] };
            while ( value.tick < o.tick )
            {
                if ( o.recategorize && value.tick + 1 == o.tick )
                {
                    value.friction = friction( value.vz, in.environment );
                }
                if ( in.split && value.tick + 1 < in.change )
                {
                    step( value, in.prefix, in.environment, nullptr, &before );
                } else
                {
                    step( value, input, in.environment, nullptr, &prepared );
                }
                ++in.cost->steps;
            }
            for ( i32 axis{ }; axis < 2; ++axis )
            {
                const i32 row{ at * 2 + axis };
                const f64 actual{ axis ? value.y : value.x }, low{ in.low[ row ] }, high{ in.high[ row ] };
                const f64 r{ ( actual - in.middle[ row ] ) / in.spread[ row ] };
                if ( !( r >= -1e100 && r <= 1e100 ) )
                {
                    return unfit;
                }
                square += r * r;
                if ( residual )
                {
                    residual[ row ] = r;
                }
                max_cell = math::bigger( max_cell, cell_miss( actual, low, high ) );
            }
            if ( square > reject + 1e-10 * ( 1.0 + reject ) )
            {
                return square;
            }
        }
        if ( fit )
        {
            fit->initial      = initial;
            fit->terminal     = value;
            fit->input        = input;
            fit->prefix       = in.prefix;
            fit->change       = in.change;
            fit->mask         = in.mask;
            fit->split        = in.split;
            fit->square       = square;
            fit->max_cell     = max_cell;
            fit->observations = in.count;
            fit->score        = square + fit->dimensions * in.log_count;
            for ( i32 i{ }; i < 8; ++i )
            {
                fit->parameter[ i ] = p[ i ];
            }
        }
        return square;
    }

    inline bool stationary_linear( f64 matrix[ 8 ][ 9 ], i32 count, f64 *out )
    {
        for ( i32 column{ }; column < count; ++column )
        {
            i32 pivot{ column };
            for ( i32 row{ column + 1 }; row < count; ++row )
            {
                if ( math::abs( matrix[ row ][ column ] ) > math::abs( matrix[ pivot ][ column ] ) )
                {
                    pivot = row;
                }
            }
            if ( math::abs( matrix[ pivot ][ column ] ) < 1e-14 )
            {
                return false;
            }
            for ( i32 k{ column }; k <= count; ++k )
            {
                const f64 t{ matrix[ column ][ k ] };
                matrix[ column ][ k ] = matrix[ pivot ][ k ];
                matrix[ pivot ][ k ]  = t;
            }
            for ( i32 row{ column + 1 }; row < count; ++row )
            {
                const f64 factor{ matrix[ row ][ column ] / matrix[ column ][ column ] };
                for ( i32 k{ column }; k <= count; ++k )
                {
                    matrix[ row ][ k ] -= factor * matrix[ column ][ k ];
                }
            }
        }
        for ( i32 row{ count - 1 }; row >= 0; --row )
        {
            f64 value{ matrix[ row ][ count ] };
            for ( i32 k{ row + 1 }; k < count; ++k )
            {
                value -= matrix[ row ][ k ] * out[ k ];
            }
            out[ row ] = value / matrix[ row ][ row ];
        }
        return true;
    }

    inline stationary_fit stationary_refine( stationary_window &in, i32 kind, const f64 *seed, i32 iterations )
    {
        stationary_fit result{ };
        i32            active[ 8 ]{ };
        i32            dimensions{ stationary_dimensions( kind, active ) };
        if ( in.mask )
        {
            i32 kept{ };
            for ( i32 i{ }; i < dimensions; ++i )
            {
                if ( in.mask >> active[ i ] & 1u )
                {
                    active[ kept++ ] = active[ i ];
                }
            }
            dimensions = kept;
        }
        const i32 rows{ in.count * 2 };
        f64       parameter[ 8 ]{ }, residual[ identification_samples * 2 ], changed[ identification_samples * 2 ];
        for ( i32 i{ }; i < 8; ++i )
        {
            parameter[ i ] = seed[ i ];
        }
        stationary_constrain( in, kind, parameter );
        f64           score{ stationary_residuals( in, kind, parameter, residual ) }, damping{ 0.001 };
        constexpr f64 increments[ 8 ]{ 0.001, 0.001, 0.01, 0.01, 0.001, 0.0001, 0.01, 0.0001 };
        for ( i32 iteration{ }; iteration < iterations && score < unfit; ++iteration )
        {
            f64 jacobian[ 8 ][ identification_samples * 2 ];
            for ( i32 column{ }; column < dimensions; ++column )
            {
                const i32 variable{ active[ column ] };
                if ( variable < 2 )
                {
                    for ( i32 at{ }; at < rows; ++at )
                    {
                        jacobian[ column ][ at ] = ( at & 1 ) == variable ? inverse_cell_sigma / ( in.high[ at ] - in.low[ at ] ) : 0.0;
                    }
                    continue;
                }
                f64 trial[ 8 ]{ };
                for ( i32 i{ }; i < 8; ++i )
                {
                    trial[ i ] = parameter[ i ];
                }
                const f64 delta{ increments[ variable ] * ( variable == 6 || ( variable == 7 && kind >= 7 )
                                                                ? math::bigger( 1.0, math::abs( parameter[ variable ] ) )
                                                                : 1.0 ) };
                trial[ variable ] += delta;
                stationary_constrain( in, kind, trial );
                if ( trial[ variable ] == parameter[ variable ] )
                {
                    trial[ variable ] -= delta;
                    stationary_constrain( in, kind, trial );
                }
                const f64 actual{ variable == 4 ? wrap( trial[ variable ] - parameter[ variable ] )
                                                : trial[ variable ] - parameter[ variable ] };
                stationary_residuals( in, kind, trial, changed );
                if ( in.cost->exhausted )
                {
                    break;
                }
                for ( i32 at{ }; at < rows; ++at )
                {
                    jacobian[ column ][ at ] = actual ? ( changed[ at ] - residual[ at ] ) / actual : 0.0;
                }
            }
            if ( in.cost->exhausted )
            {
                break;
            }
            f64 normal[ 8 ][ 9 ]{ };
            for ( i32 a{ }; a < dimensions; ++a )
            {
                for ( i32 b{ }; b < dimensions; ++b )
                {
                    for ( i32 at{ }; at < rows; ++at )
                    {
                        normal[ a ][ b ] += jacobian[ a ][ at ] * jacobian[ b ][ at ];
                    }
                }
                for ( i32 at{ }; at < rows; ++at )
                {
                    normal[ a ][ dimensions ] -= jacobian[ a ][ at ] * residual[ at ];
                }
            }
            bool accepted{ }, converged{ };
            for ( i32 attempt{ }; attempt < 8; ++attempt )
            {
                f64 matrix[ 8 ][ 9 ]{ }, update[ 8 ]{ }, trial[ 8 ]{ };
                for ( i32 a{ }; a < dimensions; ++a )
                {
                    for ( i32 b{ }; b <= dimensions; ++b )
                    {
                        matrix[ a ][ b ] = normal[ a ][ b ];
                    }
                    matrix[ a ][ a ] += damping * ( 1.0 + normal[ a ][ a ] );
                }
                if ( !stationary_linear( matrix, dimensions, update ) )
                {
                    damping *= 10.0;
                    continue;
                }
                for ( i32 i{ }; i < 8; ++i )
                {
                    trial[ i ] = parameter[ i ];
                }
                for ( i32 i{ }; i < dimensions; ++i )
                {
                    trial[ active[ i ] ] += active[ i ] == 4 ? math::clamp( update[ i ], -180.0, 180.0 ) : update[ i ];
                }
                stationary_constrain( in, kind, trial );
                const f64 cost{ stationary_residuals( in, kind, trial, changed, nullptr, score ) };
                if ( cost < score )
                {
                    converged = score - cost < 1e-7 * ( 1.0 + score );
                    score     = cost;
                    damping   = math::bigger( 1e-9, damping * 0.3 );
                    accepted  = true;
                    for ( i32 i{ }; i < 8; ++i )
                    {
                        parameter[ i ] = trial[ i ];
                    }
                    for ( i32 i{ }; i < rows; ++i )
                    {
                        residual[ i ] = changed[ i ];
                    }
                    break;
                }
                damping *= 10.0;
            }
            if ( !accepted || converged )
            {
                break;
            }
        }
        result.dimensions = dimensions;
        stationary_residuals( in, kind, parameter, nullptr, &result );
        return result;
    }

    inline bool stationary_adequate( const stationary_fit &fit, i32 observations, const physics &environment )
    {
        return fit.square / ( 2.0 * observations ) <= 1.6 && fit.max_cell <= environment.quantum * 0.5;
    }

    inline state stationary_state_at( const stationary_fit &fit, i32 tick, const physics &environment, stationary_cost &cost )
    {
        state value{ fit.initial };
        while ( value.tick < tick )
        {
            step( value, fit.split && value.tick + 1 < fit.change ? fit.prefix : fit.input, environment );
            ++cost.steps;
        }
        return value;
    }

    inline stationary_fit stationary_family( stationary_window &in, i32 kind, const stationary_motion &motion, const stationary_fit *warm,
                                             bool broad, i32 warm_iterations = 1 )
    {
        stationary_fit best{ };
        f64            base[ 8 ]{ center( in.samples[ 0 ].x, in.environment.quantum ), center( in.samples[ 0 ].y, in.environment.quantum ), motion.vx,
                                  motion.vy };
        if ( in.seeded )
        {
            for ( i32 i{ }; i < 4; ++i )
            {
                base[ i ] = in.seed[ i ];
            }
        }
        if ( warm && warm->score < unfit )
        {
            f64 seed[ 8 ]{ };
            for ( i32 i{ }; i < 8; ++i )
            {
                seed[ i ] = warm->parameter[ i ];
            }
            seed[ 0 ] = base[ 0 ];
            seed[ 1 ] = base[ 1 ];
            seed[ 2 ] = base[ 2 ];
            seed[ 3 ] = base[ 3 ];
            if ( in.samples[ 0 ].tick >= warm->initial.tick && in.samples[ 0 ].tick <= warm->terminal.tick )
            {
                const state start{ stationary_state_at( *warm, in.samples[ 0 ].tick, in.environment, *in.cost ) };
                seed[ 0 ] = start.x;
                seed[ 1 ] = start.y;
                seed[ 2 ] = start.vx;
                seed[ 3 ] = start.vy;
            }
            if ( kind == 3 || kind == 5 )
            {
                seed[ 4 ] += seed[ 5 ] * ( in.samples[ 0 ].tick - warm->input.epoch );
            }
            best = stationary_refine( in, kind, seed, broad ? 8 : warm_iterations );
            if ( !broad )
            {
                return best;
            }
        }
        if ( !broad )
        {
            return best;
        }
        f64        seeds[ 3 ][ 8 ]{ }, costs[ 3 ]{ unfit, unfit, unfit };
        const auto consider{ [ & ]( f64 yaw, f64 rate, f64 wish, f64 gain )
        {
            if ( in.cost->exhausted )
            {
                return;
            }
            f64 p[ 8 ]{ base[ 0 ], base[ 1 ], base[ 2 ], base[ 3 ], yaw, rate, wish, gain };
            stationary_constrain( in, kind, p );
            const f64 cost{ stationary_residuals( in, kind, p, nullptr, nullptr, costs[ 2 ] ) };
            for ( i32 i{ }; i < 3; ++i )
            {
                if ( cost >= costs[ i ] )
                {
                    continue;
                }
                for ( i32 j{ 2 }; j > i; --j )
                {
                    costs[ j ] = costs[ j - 1 ];
                    for ( i32 k{ }; k < 8; ++k )
                    {
                        seeds[ j ][ k ] = seeds[ j - 1 ][ k ];
                    }
                }
                costs[ i ] = cost;
                for ( i32 k{ }; k < 8; ++k )
                {
                    seeds[ i ][ k ] = p[ k ];
                }
                break;
            }
        } };
        const f64     cap_wish{ in.environment.cap /
                                ( in.environment.acceleration * in.environment.dt * math::bigger( 0.25, in.environment.air_friction ) ) };
        const f64     wishes[ 4 ]{ motion.wish, math::smaller( cap_wish, in.environment.maxspeed ), in.environment.maxspeed * 0.5,
                                   in.environment.maxspeed };
        const f64     rates[ 4 ]{ motion.mean_turn, motion.late_turn, motion.early_turn, motion.acceleration_rate };
        const f64     extent{ math::clamp( motion.extent, 15.0, 120.0 ) };
        const f64     targets[ 4 ]{ motion.target_heading, motion.last_heading, motion.last_heading + motion.sign * extent * 0.5,
                                    motion.last_heading + motion.sign * extent };
        constexpr f64 locked_offsets[ 5 ]{ 45.0, 75.0, 90.0, 105.0, 135.0 };
        constexpr f64 sweep_offsets[ 3 ]{ 45.0, 90.0, 135.0 };
        constexpr f64 gains[ 3 ]{ 0.02, 0.06, 0.15 };
        switch ( kind )
        {
            case 0: consider( 0.0, 0.0, 0.0, 0.0 ); break;
            case 1:
                for ( const f64 wish : wishes )
                {
                    consider( wrap( motion.acceleration_yaw - motion.first_heading ), 0.0, wish, 0.0 );
                    for ( const f64 offset : locked_offsets )
                    {
                        consider( motion.sign * offset, 0.0, wish, 0.0 );
                    }
                }
                break;
            case 2:
                for ( const f64 wish : wishes )
                {
                    consider( motion.acceleration_yaw, 0.0, wish, 0.0 );
                    for ( i32 i{ -3 }; i <= 4; ++i )
                    {
                        consider( motion.first_heading + i * 45.0, 0.0, wish, 0.0 );
                    }
                }
                break;
            case 3:
                for ( const f64 rate : rates )
                {
                    for ( const f64 wish : wishes )
                    {
                        consider( motion.acceleration_yaw, rate, wish, 0.0 );
                        for ( const f64 offset : sweep_offsets )
                        {
                            consider( motion.first_heading + motion.sign * offset, rate, wish, 0.0 );
                        }
                    }
                }
                break;
            case 4:
            case 5:
                for ( const f64 target : targets )
                {
                    for ( const f64 wish : wishes )
                    {
                        for ( const f64 gain : gains )
                        {
                            if ( kind == 4 )
                            {
                                consider( target, 0.0, wish, gain );
                            } else
                            {
                                for ( const f64 rate : rates )
                                {
                                    consider( target - rate * ( in.samples[ in.count - 1 ].tick - in.samples[ 0 ].tick ), rate, wish,
                                              gain );
                                }
                            }
                        }
                    }
                }
                break;
            case 6:
                for ( const f64 rate : rates )
                {
                    consider( 0.0, rate, 0.0, 0.0 );
                }
                break;
            default:
                for ( const f64 target : targets )
                {
                    for ( const f64 wish : wishes )
                    {
                        for ( i32 i{ 1 }; i <= 4; ++i )
                        {
                            consider( target, 0.0, wish, in.environment.cap * i * 0.25 );
                        }
                    }
                }
                break;
        }
        for ( i32 i{ }; i < 3 && costs[ i ] < unfit && !in.cost->exhausted; ++i )
        {
            const auto candidate{ stationary_refine( in, kind, seeds[ i ], 6 ) };
            if ( candidate.score < best.score )
            {
                best = candidate;
            }
        }
        return best;
    }

    inline void stationary_publish( stationary_cache &cache, stationary_result &result, const stationary_fit &fit, i32 observations,
                                    i32 tick, const physics &environment )
    {
        const i32 kind{ fit.input.kind };
        cache.families[ kind ]    = fit;
        cache.selected            = kind;
        cache.tick                = tick;
        cache.recovery_first_tick = fit.initial.tick;
        cache.environment         = environment;
        cache.ready               = true;
        if ( cache.anchor_count == identification_anchors )
        {
            for ( i32 at{ 1 }; at < identification_anchors; ++at )
            {
                cache.anchors[ at - 1 ] = cache.anchors[ at ];
            }
            --cache.anchor_count;
        }
        cache.anchors[ cache.anchor_count++ ] = fit;
        cache.split                           = stationary_fit{ };
        result.chosen                         = fit;
        result.count                          = observations;
        result.consistent                     = true;
    }

    inline bool identify_change( const observation *samples, i32 count, const physics &environment, const stationary_anchor &anchor,
                                 stationary_cache &cache, stationary_result &result )
    {
        constexpr i32 before_samples{ 8 }, trusted_samples{ 12 };
        const i32     last{ count - 1 };
        i32           held{ last };
        while ( held >= 0 && samples[ held ].tick > anchor.tick )
        {
            --held;
        }
        if ( held < 0 || held == last )
        {
            return false;
        }
        i32 begin{ held };
        while ( begin > 0 && held - begin + 1 < before_samples && samples[ begin - 1 ].tick >= anchor.first_tick )
        {
            --begin;
        }
        if ( held - begin + 1 < 3 || samples[ begin ].tick < anchor.origin.tick )
        {
            return false;
        }

        const i32         requested{ count - begin };
        stationary_window in{ stationary_window_of( samples + begin, requested, environment, result.cost ) };
        if ( !stationary_height( in ) )
        {
            return false;
        }

        state pinned{ anchor.origin };
        while ( pinned.tick < in.samples[ 0 ].tick )
        {
            step( pinned, anchor.input, environment );
            ++result.cost.steps;
        }
        in.seed[ 0 ] = pinned.x;
        in.seed[ 1 ] = pinned.y;
        in.seed[ 2 ] = pinned.vx;
        in.seed[ 3 ] = pinned.vy;
        in.seeded    = true;

        const i32 first_after{ held - begin + 1 };
        i32       first_bad{ requested - 1 };
        state     cursor{ pinned }, previous{ pinned };
        for ( i32 at{ 1 }; at < requested; ++at )
        {
            const auto &o{ in.samples[ at ] };
            previous = cursor;
            while ( cursor.tick < o.tick )
            {
                if ( o.recategorize && cursor.tick + 1 == o.tick )
                {
                    cursor.friction = friction( cursor.vz, environment );
                }
                step( cursor, anchor.input, environment );
                ++result.cost.steps;
            }
            if ( at < first_after )
            {
                continue;
            }
            const f64 low_x{ cell_low( o.x, environment.quantum ) }, high_x{ cell_high( o.x, environment.quantum ) };
            const f64 low_y{ cell_low( o.y, environment.quantum ) }, high_y{ cell_high( o.y, environment.quantum ) };
            const f64 miss_x{ cell_miss( cursor.x, low_x, high_x ) };
            const f64 miss_y{ cell_miss( cursor.y, low_y, high_y ) };
            if ( math::bigger( miss_x, miss_y ) > environment.quantum * 0.5 )
            {
                first_bad = at;
                break;
            }
        }

        i32       candidates[ 3 ]{ }, candidate_count{ };
        const i32 low{ in.samples[ first_bad - 1 ].tick + 1 }, high{ in.samples[ first_bad ].tick };
        candidates[ candidate_count++ ] = low;
        if ( high != low )
        {
            candidates[ candidate_count++ ] = high;
        }
        if ( first_bad - 1 >= first_after )
        {
            candidates[ candidate_count++ ] = in.samples[ first_bad - 2 ].tick + 1;
        }

        const auto &bad{ in.samples[ first_bad ] };
        const i32   ticks{ bad.tick - previous.tick };
        const f64   reach{ environment.dt * ticks * ( ticks + 1 ) * 0.5 };
        const f64   push_x{ ( center( bad.x, environment.quantum ) - previous.x - previous.vx * ticks * environment.dt ) / reach };
        const f64   push_y{ ( center( bad.y, environment.quantum ) - previous.y - previous.vy * ticks * environment.dt ) / reach };
        const f64   push{ length( push_x, push_y ) }, facing{ heading( previous.vx, previous.vy ) };
        const f64   push_yaw{ push > 0.0 ? heading( push_x, push_y ) : facing };
        const f64   side{ previous.vx * push_y - previous.vy * push_x >= 0.0 ? 1.0 : -1.0 };
        const f64   wish_hint{ math::clamp(
            push / ( environment.acceleration * environment.dt * math::bigger( 0.25, environment.air_friction ) ), 0.0,
            environment.maxspeed ) };
        const f64   turn_hint{ wrap( heading( previous.vx + push_x, previous.vy + push_y ) - facing ) };

        i32       kind{ anchor.input.kind };
        f64       held_parameter[ 8 ]{ pinned.x, pinned.y, pinned.vx, pinned.vy,
                                       anchor.input.yaw, anchor.input.rate, anchor.input.speed, anchor.input.budget };
        const i32 shift{ in.samples[ 0 ].tick - anchor.input.epoch };
        if ( kind == 3 || kind == 5 )
        {
            held_parameter[ 4 ] += held_parameter[ 5 ] * shift;
        }
        if ( kind >= 7 && ( shift & 1 ) )
        {
            kind = 15 - kind;
        }

        stationary_fit best{ };
        const auto     offer{ [ & ]( const stationary_fit &candidate )
        {
            if ( stationary_adequate( candidate, requested, environment ) && candidate.score < best.score )
            {
                best = candidate;
            }
        } };

        const bool trusted{ anchor.support >= trusted_samples };

        in.mask = identification_state;
        offer( stationary_refine( in, kind, held_parameter, 4 ) );

        in.split  = true;
        in.prefix = anchor.input;
        f64 stopped_miss{ unfit };
        for ( i32 at{ }; at < candidate_count && best.score >= unfit && !result.cost.exhausted; ++at )
        {
            in.change = candidates[ at ];
            const stationary_fit stopped{ stationary_refine( in, 0, held_parameter, 4 ) };
            stopped_miss = math::smaller( stopped_miss, stopped.max_cell );
            offer( stopped );
        }
        const bool pushed{ stopped_miss > environment.quantum };
        for ( i32 at{ };
              at < candidate_count && best.score >= unfit && trusted && pushed && anchor.input.kind != 0 && !result.cost.exhausted; ++at )
        {
            in.change = candidates[ at ];
            const i32  since{ in.change - in.samples[ 0 ].tick };
            const bool rotating{ kind == 3 || kind == 5 };
            for ( i32 variable{ 4 }; variable < 8 && !result.cost.exhausted; ++variable )
            {
                i32       active[ 8 ]{ };
                const i32 dimensions{ stationary_dimensions( kind, active ) };
                bool      used{ };
                for ( i32 index{ 4 }; index < dimensions; ++index )
                {
                    used = used || active[ index ] == variable;
                }
                if ( !used || ( rotating && variable == 5 ) )
                {
                    continue;
                }

                f64       values[ 6 ]{ held_parameter[ variable ] };
                i32       value_count{ 1 };
                const f64 old{ held_parameter[ variable ] };
                if ( variable == 4 && kind == 1 )
                {
                    values[ value_count++ ] = -old;
                    values[ value_count++ ] = wrap( push_yaw - facing );
                    values[ value_count++ ] = side * 90.0;
                } else if ( variable == 4 )
                {
                    const f64 offset{ rotating ? held_parameter[ 5 ] * since : 0.0 };
                    values[ value_count++ ] = 2.0 * facing - ( old + offset ) - offset;
                    values[ value_count++ ] = push_yaw - offset;
                    values[ value_count++ ] = facing + side * 45.0 - offset;
                    values[ value_count++ ] = facing + side * 90.0 - offset;
                    values[ value_count++ ] = facing + side * 135.0 - offset;
                } else if ( variable == 5 )
                {
                    values[ value_count++ ] = -old;
                    values[ value_count++ ] = old * 0.5;
                    values[ value_count++ ] = turn_hint;
                } else if ( variable == 6 )
                {
                    values[ value_count++ ] = old * 0.5;
                    values[ value_count++ ] = old * 0.2;
                    values[ value_count++ ] = environment.maxspeed;
                    values[ value_count++ ] = wish_hint;
                } else
                {
                    values[ value_count++ ] = old * 0.5;
                    values[ value_count++ ] = old * 2.0;
                }

                in.mask = identification_state | 1u << variable | ( rotating && variable == 4 ? 1u << 5 : 0u );
                const i32 rates{ rotating && variable == 4 ? 3 : 1 };
                f64       seeds[ 2 ][ 8 ]{ }, costs[ 2 ]{ unfit, unfit };
                for ( i32 value{ }; value < value_count && !result.cost.exhausted; ++value )
                {
                    for ( i32 rate{ }; rate < rates; ++rate )
                    {
                        f64 trial[ 8 ]{ };
                        for ( i32 i{ }; i < 8; ++i )
                        {
                            trial[ i ] = held_parameter[ i ];
                        }
                        trial[ variable ] = values[ value ];
                        if ( rate )
                        {
                            trial[ 5 ] = rate == 1 ? -held_parameter[ 5 ] : 0.0;
                            trial[ 4 ] += ( held_parameter[ 5 ] - trial[ 5 ] ) * since;
                        }
                        stationary_constrain( in, kind, trial );
                        const f64 cost{ stationary_residuals( in, kind, trial, nullptr, nullptr, costs[ 1 ] ) };
                        const i32 slot{ cost < costs[ 0 ] ? 0 : cost < costs[ 1 ] ? 1
                                                                                  : 2 };
                        if ( slot == 2 )
                        {
                            continue;
                        }
                        if ( slot == 0 )
                        {
                            costs[ 1 ] = costs[ 0 ];
                            for ( i32 i{ }; i < 8; ++i )
                            {
                                seeds[ 1 ][ i ] = seeds[ 0 ][ i ];
                            }
                        }
                        costs[ slot ] = cost;
                        for ( i32 i{ }; i < 8; ++i )
                        {
                            seeds[ slot ][ i ] = trial[ i ];
                        }
                    }
                }
                for ( i32 i{ }; i < 2 && costs[ i ] < unfit && !result.cost.exhausted; ++i )
                {
                    offer( stationary_refine( in, kind, seeds[ i ], 4 ) );
                }
            }
        }
        for ( i32 at{ }; at < candidate_count && best.score >= unfit && trusted && pushed && !result.cost.exhausted; ++at )
        {
            in.change = candidates[ at ];
            in.mask   = 0;
            f64 seed[ 8 ]{ pinned.x, pinned.y, pinned.vx, pinned.vy, push_yaw, 0.0, wish_hint, 0.0 };
            offer( stationary_refine( in, 2, seed, 4 ) );
            seed[ 6 ] = environment.maxspeed;
            if ( best.score >= unfit )
            {
                offer( stationary_refine( in, 2, seed, 4 ) );
            }
        }

        if ( best.score >= unfit )
        {
            return false;
        }
        if ( !best.split )
        {
            best.mask = 0;
            stationary_publish( cache, result, best, requested, samples[ last ].tick, environment );
            return true;
        }
        cache.split       = best;
        result.chosen     = best;
        result.count      = requested;
        result.consistent = true;
        return true;
    }

    inline bool recover_stationary( const observation *samples, i32 count, i32 newest, const physics &environment,
                                    const stationary_seed *seeds, i32 seed_count, stationary_cache &cache, stationary_result &result )
    {
        if ( newest < 4 || newest > count )
        {
            return false;
        }
        stationary_window in{ stationary_window_of( samples + count - newest, newest, environment, result.cost ) };
        if ( !stationary_height( in ) )
        {
            return false;
        }
        stationary_fit best{ };
        for ( i32 at{ }; at < seed_count && !result.cost.exhausted; ++at )
        {
            const stationary_fit fit{ stationary_refine( in, seeds[ at ].kind, seeds[ at ].parameter, 6 ) };
            if ( stationary_adequate( fit, newest, environment ) && ( best.score >= unfit || fit.dimensions < best.dimensions ||
                                                                      ( fit.dimensions == best.dimensions && fit.square < best.square ) ) )
            {
                best = fit;
            }
        }
        if ( best.score >= unfit )
        {
            return false;
        }

        i32 older{ };
        while ( older < count - newest && samples[ older ].tick < cache.floor_tick )
        {
            ++older;
        }
        if ( older < count - newest && !result.cost.exhausted )
        {
            const i32         wider{ count - older };
            stationary_window wide{ stationary_window_of( samples + older, wider, environment, result.cost ) };
            if ( stationary_height( wide ) )
            {
                i32 kind{ best.input.kind };
                if ( kind >= 7 && ( ( wide.samples[ 0 ].tick - best.input.epoch ) & 1 ) )
                {
                    kind = 15 - kind;
                }
                const stationary_fit longer{ stationary_family( wide, kind, stationary_kinematics( wide ), &best, false, 3 ) };
                if ( stationary_adequate( longer, wider, environment ) )
                {
                    stationary_publish( cache, result, longer, wider, samples[ count - 1 ].tick, environment );
                    return true;
                }
            }
        }
        stationary_publish( cache, result, best, newest, samples[ count - 1 ].tick, environment );
        return true;
    }

    inline bool identify_change( const observation *samples, i32 count, const physics &environment, stationary_cache &cache,
                                 stationary_result &result )
    {
        constexpr u64 change_limit{ 2048 };
        constexpr i32 anchor_ticks{ 32 };
        if ( count < 4 || cache.anchor_count == 0 )
        {
            return false;
        }
        const stationary_fit *source{ &cache.anchors[ 0 ] };
        for ( i32 at{ cache.anchor_count - 1 }; at > 0; --at )
        {
            if ( cache.anchors[ at ].terminal.tick <= samples[ count - 3 ].tick )
            {
                source = &cache.anchors[ at ];
                break;
            }
        }
        if ( samples[ count - 1 ].tick - source->terminal.tick > anchor_ticks )
        {
            return false;
        }
        const stationary_anchor anchor{ source->input, source->initial, source->terminal.tick, source->initial.tick, source->observations };
        result.cost.limit     = math::smaller( result.cost.steps + change_limit, result.cost.ceiling );
        result.cost.exhausted = false;
        return identify_change( samples, count, environment, anchor, cache, result );
    }

    inline stationary_result identify_stationary( const observation *samples, i32 count, const physics &environment,
                                                  stationary_cache &cache, bool search, bool publish_rival = false,
                                                  stationary_challenge *challenge = nullptr )
    {
        constexpr i32     retire_samples{ 8 };
        stationary_result result{ };
        if ( count < 4 || count > identification_samples || environment.dt <= 0.0 || environment.quantum <= 0.0 ||
             environment.acceleration <= 0.0 || environment.maxspeed <= 0.0 || environment.maxvelocity <= 0.0 )
        {
            return result;
        }
        const i32 tick{ samples[ count - 1 ].tick };
        for ( i32 at{ 1 }; at < count; ++at )
        {
            if ( samples[ at ].tick <= samples[ at - 1 ].tick || samples[ at ].tick - samples[ at - 1 ].tick != samples[ at ].gap ||
                 samples[ at ].gap > 8 || tick - samples[ 0 ].tick > 128 )
            {
                return result;
            }
        }
        const bool warm{ cache.ready && cache.tick < tick && cache.tick >= samples[ 0 ].tick && cache.environment == environment };
        if ( !warm )
        {
            cache = stationary_cache{ };
        }

        if ( cache.split.split )
        {
            const stationary_fit previous{ cache.split };
            cache.split = stationary_fit{ };
            i32 begin{ };
            while ( begin < count && samples[ begin ].tick < previous.initial.tick )
            {
                ++begin;
            }
            if ( begin < count - 3 && samples[ begin ].tick == previous.initial.tick )
            {
                const i32         requested{ count - begin };
                stationary_window in{ stationary_window_of( samples + begin, requested, environment, result.cost ) };
                in.prefix = previous.prefix;
                in.change = previous.change;
                in.mask   = previous.mask;
                in.split  = true;
                if ( stationary_height( in ) )
                {
                    const stationary_fit fit{ stationary_family( in, previous.input.kind, stationary_motion{ }, &previous, false ) };
                    if ( stationary_adequate( fit, requested, environment ) )
                    {
                        i32 after{ begin };
                        while ( samples[ after ].tick < previous.change )
                        {
                            ++after;
                        }
                        const i32 remaining{ count - after };
                        if ( remaining >= retire_samples )
                        {
                            stationary_window alone{ stationary_window_of( samples + after, remaining, environment, result.cost ) };
                            if ( stationary_height( alone ) )
                            {
                                i32 kind{ fit.input.kind };
                                if ( kind >= 7 && ( ( alone.samples[ 0 ].tick - fit.input.epoch ) & 1 ) )
                                {
                                    kind = 15 - kind;
                                }
                                const stationary_fit plain{ stationary_family( alone, kind, stationary_motion{ }, &fit, false, 3 ) };
                                if ( stationary_adequate( plain, remaining, environment ) )
                                {
                                    cache.floor_tick  = alone.samples[ 0 ].tick;
                                    cache.tick        = tick;
                                    cache.environment = environment;
                                    cache.ready       = true;
                                    stationary_publish( cache, result, plain, remaining, tick, environment );
                                    return result;
                                }
                            }
                        }
                        cache.split       = fit;
                        cache.tick        = tick;
                        cache.environment = environment;
                        cache.ready       = true;
                        result.chosen     = fit;
                        result.count      = requested;
                        result.consistent = true;
                        return result;
                    }
                }
            }
        }

        while ( count > 0 && samples[ 0 ].tick < cache.floor_tick )
        {
            ++samples;
            --count;
        }
        if ( count < 4 )
        {
            return result;
        }
        i32 resume_count{ count };
        if ( warm && cache.recovery_first_tick > samples[ 0 ].tick )
        {
            i32 begin{ };
            while ( begin < count - 4 && samples[ begin ].tick < cache.recovery_first_tick )
            {
                ++begin;
            }
            resume_count = count - begin;
        }
        for ( i32 requested{ count }; requested >= 4 && !result.cost.exhausted; )
        {
            stationary_window in{ stationary_window_of( samples + count - requested, requested, environment, result.cost ) };
            if ( stationary_height( in ) )
            {
                const auto     motion{ stationary_kinematics( in ) };
                const bool     cheap_full{ warm && requested == count && resume_count < count };
                stationary_fit fits[ identification_families ]{ };
                stationary_fit best{ };
                i32            held{ -1 };
                if ( warm && cache.selected >= 0 )
                {
                    i32         kind{ cache.selected };
                    const auto &previous{ cache.families[ kind ] };
                    if ( kind >= 7 && ( ( in.samples[ 0 ].tick - previous.input.epoch ) & 1 ) )
                    {
                        kind = 15 - kind;
                    }
                    held         = kind;
                    fits[ kind ] = stationary_family( in, kind, motion, &previous, false, cheap_full ? 0 : 1 );
                    if ( stationary_adequate( fits[ kind ], requested, environment ) )
                    {
                        best = fits[ kind ];
                    }
                }
                const bool           reopen{ challenge && best.score < unfit && stationary_turns_to_goal( best.input.kind ) };
                const i32            resume_family{ cache.next_family };
                const stationary_fit held_fit{ best };
                for ( i32 pass{ warm ? 0 : 1 };
                      search && pass < ( cheap_full ? 1 : 2 ) && ( best.score >= unfit || reopen ) && !result.cost.exhausted; ++pass )
                {
                    const i32 first_kind{ cache.next_family };
                    for ( i32 ordinal{ }; ordinal < identification_families && !result.cost.exhausted; ++ordinal )
                    {
                        const i32 kind{ ( first_kind + ordinal ) % identification_families };
                        cache.next_family = ( kind + 1 ) % identification_families;
                        i32 active[ 8 ]{ };
                        if ( stationary_dimensions( kind, active ) * in.log_count >= best.score )
                        {
                            continue;
                        }
                        i32 previous{ kind };
                        if ( kind >= 7 && ( ( in.samples[ 0 ].tick - cache.families[ kind ].input.epoch ) & 1 ) )
                        {
                            previous = 15 - kind;
                        }
                        fits[ kind ] = stationary_family( in, kind, motion, warm ? &cache.families[ previous ] : nullptr, pass != 0,
                                                          cheap_full ? 0 : 1 );
                        if ( stationary_adequate( fits[ kind ], requested, environment ) && fits[ kind ].score < best.score )
                        {
                            best = fits[ kind ];
                        }
                    }
                    if ( stationary_adequate( best, requested, environment ) && !( reopen && stationary_turns_to_goal( best.input.kind ) ) )
                    {
                        break;
                    }
                }
                if ( reopen )
                {
                    cache.next_family = resume_family;
                    if ( best.input.kind != held_fit.input.kind )
                    {
                        *challenge = { held_fit, best, true };
                        if ( !publish_rival )
                        {
                            best = held_fit;
                        }
                    }
                }
                if ( !cheap_full || best.score < unfit )
                {
                    for ( i32 kind{ }; kind < identification_families; ++kind )
                    {
                        if ( fits[ kind ].score < unfit && ( !reopen || kind == held ) )
                        {
                            cache.families[ kind ] = fits[ kind ];
                        }
                    }
                }
                cache.tick        = tick;
                cache.environment = environment;
                cache.ready       = true;
                if ( stationary_adequate( best, requested, environment ) )
                {
                    stationary_publish( cache, result, best, requested, tick, environment );
                    return result;
                }
            }
            if ( !search && ( requested != count || resume_count == count ) )
            {
                break;
            }
            if ( requested == 4 )
            {
                cache.recovery_first_tick = samples[ 0 ].tick;
                break;
            }
            const i32 next{ requested == count && resume_count < count ? resume_count : math::bigger( 4, requested * 2 / 3 ) };
            if ( result.cost.exhausted )
            {
                cache.recovery_first_tick = samples[ count - next ].tick;
            }
            requested = next;
        }
        return result;
    }

    inline bool phase_observed( const observation *samples, i32 count, const stationary_fit &fit, const physics &environment,
                                stationary_cost &cost, stationary_fit &alternative )
    {
        i32 first{ };
        while ( first < count && samples[ first ].tick < fit.initial.tick )
        {
            ++first;
        }
        const i32 requested{ count - first };
        if ( requested < 4 )
        {
            return true;
        }
        stationary_window in{ stationary_window_of( samples + first, requested, environment, cost ) };
        in.prefix = fit.prefix;
        in.change = fit.change;
        in.split  = fit.split;
        if ( !stationary_height( in ) )
        {
            return true;
        }
        control other{ fit.input };
        other.kind = 15 - other.kind;
        const state begins{ stationary_state_at( fit, fit.split ? fit.change - 1 : fit.initial.tick, environment, cost ) };
        state       held_step{ begins }, other_step{ begins };
        step( held_step, fit.input, environment );
        step( other_step, other, environment );
        f64 seed[ 8 ]{ };
        for ( i32 i{ }; i < 8; ++i )
        {
            seed[ i ] = fit.parameter[ i ];
        }
        seed[ 2 ] += ( held_step.vx - other_step.vx ) * 0.5;
        seed[ 3 ] += ( held_step.vy - other_step.vy ) * 0.5;
        alternative = stationary_refine( in, other.kind, seed, 4 );
        return !stationary_adequate( alternative, requested, environment );
    }

    inline constexpr i32 condition_horizon{ 67 };

    struct fit
    {
        f64     p[ 6 ]{ }, low[ 6 ]{ }, high[ 6 ]{ }, scale[ 6 ]{ };
        i32     kind{ }, cut{ }, count{ }, active[ 6 ]{ }, dimensions{ };
        f64     square{ }, cell_square{ }, max_cell{ }, cell_tolerance{ 1e-7 };
        state   initial{ }, terminal{ };
        control input{ };
        bool    phase_ambiguous{ };
        i32     hidden_idle_tick{ -1 };

        bool continuity_resolved{ }, continuity_attempted{ }, continuity_same_side_veto{ };
        i32  continuity_side{ }, continuity_same_side_paths{ }, continuity_strict_same_side_paths{ };
        f64  continuity_selected_max_cell{ }, continuity_matching_min_cell{ 1e300 };
        u32  continuity_phase_veto_mask{ }, continuity_phase_strict_mask{ };
        u32  phase_mask{ };
    };
    struct statistics
    {
        u64 phase_evaluations{ };
    };
    struct trajectory
    {
        f64 vx[ 32 ]{ }, vy[ 32 ]{ };
        i32 through[ 16 ]{ }, steps{ };
    };
    struct window
    {
        const observation *samples{ };
        i32                count{ };
        physics            environment{ };
        f64                z0{ }, vz0{ };
        statistics        *cost{ };
        f64                low[ 32 ]{ }, high[ 32 ]{ }, center[ 32 ]{ }, sigma[ 32 ]{ };
        f64                roundoff_xy{ 1e-7 }, roundoff_z{ 1e-7 };
    };
    struct result
    {
        fit  chosen{ }, second{ };
        i32  compatible{ }, count{ };
        bool applied{ }, onset{ }, phase_disagreement{ }, large_disagreement{ };
    };

    inline void evaluate( fit &h, const window &in, bool cells, f64 *residual, trajectory *tape = nullptr )
    {
        const auto &e{ in.environment };
        state       s{ h.p[ 0 ] * h.scale[ 0 ],
                       h.p[ 1 ] * h.scale[ 1 ],
                       in.z0,
                       h.p[ 2 ] * h.scale[ 2 ],
                       h.p[ 3 ] * h.scale[ 3 ],
                       in.vz0,
                       friction( in.vz0 + e.gravity * e.dt * 0.5, e ),
                       in.samples[ 0 ].tick };
        h.input = { h.kind,
                    h.kind == 2 ? h.cut - 1 : in.samples[ 0 ].tick,
                    h.p[ 4 ] * h.scale[ 4 ],
                    h.kind == 2   ? h.p[ 5 ] * h.scale[ 5 ]
                    : h.kind >= 7 ? math::smaller( 450.0, e.maxspeed )
                                  : 0.0,
                    h.kind == 6 ? h.p[ 5 ] * h.scale[ 5 ] : 0.0,
                    math::smaller( 30.0, e.acceleration * math::smaller( 450.0, e.maxspeed ) * e.dt ) };
        const control coast{ };
        const basis   cached{ prepare( h.input ) };
        h.initial = s;
        bool used_phase{ };
        h.square = h.cell_square = h.max_cell = 0.0;
        h.phase_mask                          = 0;
        if ( tape )
        {
            tape->steps = 0;
        }
        for ( i32 at{ }; at < in.count; ++at )
        {
            const auto &o{ in.samples[ at ] };
            while ( s.tick < o.tick )
            {
                bool      near_step{ };
                const i32 input_tick{ s.tick };
                const f64 before_x{ s.vx }, before_y{ s.vy };
                step( s, h.kind == 2 && s.tick + 1 < h.cut ? coast : h.input, e, &near_step, &cached,
                      tape ? &tape->vx[ tape->steps ] : nullptr, tape ? &tape->vy[ tape->steps ] : nullptr );
                used_phase = used_phase || near_step;
                if ( near_step && ( s.vx != before_x || s.vy != before_y ) )
                {
                    const i32 side{ ( ( input_tick - h.input.epoch ) & 1 ? 1 : -1 ) * ( h.kind == 7 ? 1 : -1 ) };
                    h.phase_mask |= side > 0 ? 1u : 2u;
                }
                if ( tape )
                {
                    ++tape->steps;
                }
            }
            if ( tape )
            {
                tape->through[ at ] = tape->steps;
            }
            for ( i32 axis{ }; axis < 2; ++axis )
            {
                const i32 index{ at * 2 + axis };
                const f64 value{ axis ? s.y : s.x }, low{ in.low[ index ] }, high{ in.high[ index ] }, sigma{ in.sigma[ index ] };
                const f64 r{ value - in.center[ index ] };
                const f64 d{ value < low ? value - low : value > high ? value - high
                                                                      : 0.0 };
                h.square += ( r / sigma ) * ( r / sigma );
                h.cell_square += ( d / sigma ) * ( d / sigma );
                h.max_cell                = math::bigger( h.max_cell, math::abs( d ) );
                residual[ at * 2 + axis ] = ( cells ? d : r ) / sigma;
            }
        }
        h.terminal = s;
        if ( used_phase )
        {
            ++in.cost->phase_evaluations;
        }
    }

    inline bool linear( f64 matrix[ 6 ][ 7 ], i32 n, f64 *x )
    {
        for ( i32 col{ }; col < n; ++col )
        {
            i32 pivot{ col };
            for ( i32 j{ col + 1 }; j < n; ++j )
            {
                if ( math::abs( matrix[ j ][ col ] ) > math::abs( matrix[ pivot ][ col ] ) )
                {
                    pivot = j;
                }
            }
            if ( math::abs( matrix[ pivot ][ col ] ) < 1e-15 )
            {
                return false;
            }
            for ( i32 j{ col }; j <= n; ++j )
            {
                const f64 t{ matrix[ col ][ j ] };
                matrix[ col ][ j ]   = matrix[ pivot ][ j ];
                matrix[ pivot ][ j ] = t;
            }
            const f64 d{ matrix[ col ][ col ] };
            for ( i32 j{ col }; j <= n; ++j )
            {
                matrix[ col ][ j ] /= d;
            }
            for ( i32 i{ }; i < n; ++i )
            {
                if ( i != col )
                {
                    const f64 f{ matrix[ i ][ col ] };
                    for ( i32 j{ col }; j <= n; ++j )
                    {
                        matrix[ i ][ j ] -= f * matrix[ col ][ j ];
                    }
                }
            }
        }
        for ( i32 i{ }; i < n; ++i )
        {
            x[ i ] = matrix[ i ][ n ];
        }
        return true;
    }

    inline f64 norm( const f64 *v, i32 n )
    {
        f64 total{ };
        for ( i32 i{ }; i < n; ++i )
        {
            total += v[ i ] * v[ i ];
        }
        return total;
    }

    inline void optimize( fit &h, const window &in, bool cells, i32 iterations )
    {
        f64        damping{ 0.01 }, residual[ 32 ]{ };
        trajectory current_path{ };
        evaluate( h, in, cells, residual, &current_path );
        const i32 n{ h.dimensions }, size{ in.count * 2 };
        f64       score{ norm( residual, size ) };
        if ( score == 0.0 )
        {
            return;
        }
        for ( i32 iteration{ }; iteration < iterations; ++iteration )
        {
            f64 jacobian[ 6 ][ 32 ]{ };
            for ( i32 j{ }; j < n; ++j )
            {
                const i32 k{ h.active[ j ] };
                fit       trial{ h };
                f64       delta{ 1e-4 };
                if ( h.p[ k ] + delta > h.high[ k ] )
                {
                    delta = -delta;
                }
                trial.p[ k ] = math::clamp( h.p[ k ] + delta, h.low[ k ], h.high[ k ] );
                delta        = trial.p[ k ] - h.p[ k ];
                if ( k < 2 )
                {
                    f64 position{ trial.p[ k ] * trial.scale[ k ] };
                    i32 through{ };
                    for ( i32 at{ }; at < in.count; ++at )
                    {
                        while ( through < current_path.through[ at ] )
                        {
                            position = advance_coordinate( position, k ? current_path.vy[ through ] : current_path.vx[ through ],
                                                           in.environment.dt );
                            ++through;
                        }
                        const i32 r{ at * 2 + k };
                        const f64 low{ in.low[ r ] }, high{ in.high[ r ] };
                        const f64 d{ position < low ? position - low : position > high ? position - high
                                                                                       : 0.0 };
                        const f64 value{ ( cells ? d : position - in.center[ r ] ) / in.sigma[ r ] };
                        jacobian[ j ][ r ] = delta ? ( value - residual[ r ] ) / delta : 0.0;
                    }
                    continue;
                }
                evaluate( trial, in, cells, jacobian[ j ] );
                for ( i32 r{ }; r < size; ++r )
                {
                    jacobian[ j ][ r ] = delta ? ( jacobian[ j ][ r ] - residual[ r ] ) / delta : 0.0;
                }
            }
            f64 normal[ 6 ][ 7 ]{ };
            for ( i32 a{ }; a < n; ++a )
            {
                for ( i32 b{ }; b < n; ++b )
                {
                    for ( i32 r{ }; r < size; ++r )
                    {
                        normal[ a ][ b ] += jacobian[ a ][ r ] * jacobian[ b ][ r ];
                    }
                }
                for ( i32 r{ }; r < size; ++r )
                {
                    normal[ a ][ n ] -= jacobian[ a ][ r ] * residual[ r ];
                }
            }
            bool accepted{ };
            for ( i32 attempt{ }; attempt < 7; ++attempt )
            {
                f64 matrix[ 6 ][ 7 ]{ }, dx[ 6 ]{ };
                for ( i32 a{ }; a < n; ++a )
                {
                    for ( i32 b{ }; b <= n; ++b )
                    {
                        matrix[ a ][ b ] = normal[ a ][ b ];
                    }
                    matrix[ a ][ a ] += damping * ( 1.0 + matrix[ a ][ a ] );
                }
                if ( !linear( matrix, n, dx ) )
                {
                    break;
                }
                fit trial{ h };
                for ( i32 j{ }; j < n; ++j )
                {
                    const i32 k{ h.active[ j ] };
                    trial.p[ k ] = math::clamp( h.p[ k ] + dx[ j ], h.low[ k ], h.high[ k ] );
                }
                f64        next_residual[ 32 ]{ };
                trajectory next_path{ };
                evaluate( trial, in, cells, next_residual, &next_path );
                const f64 next{ norm( next_residual, size ) };
                if ( next < score )
                {
                    h = trial;
                    for ( i32 r{ }; r < size; ++r )
                    {
                        residual[ r ] = next_residual[ r ];
                    }
                    current_path = next_path;
                    score        = next;
                    damping      = math::bigger( 1e-10, damping * 0.25 );
                    accepted     = true;
                    break;
                }
                damping *= 8.0;
            }
            if ( !accepted || score < 1e-14 )
            {
                break;
            }
        }
    }

    inline void parameter( fit &h, i32 i, f64 value, f64 lo, f64 hi, f64 scale, bool active )
    {
        h.scale[ i ] = scale;
        h.low[ i ]   = lo / scale;
        h.high[ i ]  = hi / scale;
        h.p[ i ]     = math::clamp( value, lo, hi ) / scale;
        if ( active )
        {
            h.active[ h.dimensions++ ] = i;
        }
    }

    inline fit make( const window &in, i32 kind, f64 yaw, f64 amount, i32 cut )
    {
        fit h{ };
        h.kind           = kind;
        h.cut            = cut;
        h.count          = in.count;
        h.cell_tolerance = in.roundoff_xy;
        const auto &o{ in.samples[ 0 ] };
        const auto &e{ in.environment };
        for ( i32 axis{ }; axis < 2; ++axis )
        {
            const f64 value{ axis ? o.y : o.x };
            parameter( h, axis, center( value, e.quantum ), cell_low( value, e.quantum ), cell_high( value, e.quantum ), e.quantum, true );
        }
        const f64 scale{ math::bigger( 1.0, 2.0 * e.quantum / ( math::bigger( o.gap, 1 ) * e.dt ) ) };
        parameter( h, 2, o.vx, -e.maxvelocity, e.maxvelocity, scale, true );
        parameter( h, 3, o.vy, -e.maxvelocity, e.maxvelocity, scale, true );
        parameter( h, 4, wrap( yaw ), -540.0, 540.0, 30.0, kind == 2 || kind >= 7 );
        parameter( h, 5, amount, kind == 6 ? -45.0 : 0.0, kind == 6 ? 45.0 : e.maxspeed,
                   kind == 6 ? 5.0 : math::bigger( 30.0, e.maxspeed * 0.25 ), kind == 2 || kind == 6 );
        return h;
    }

    inline bool better( const fit &a, const fit &b )
    {
        const bool af{ a.max_cell <= 1e-7 }, bf{ b.max_cell <= 1e-7 };
        if ( af != bf )
        {
            return af;
        }
        if ( !af && a.cell_square != b.cell_square )
        {
            return a.cell_square < b.cell_square;
        }
        if ( a.dimensions != b.dimensions )
        {
            return a.dimensions < b.dimensions;
        }
        return a.square < b.square;
    }

    inline void height( window &in )
    {
        f64 st{ }, sy{ }, stt{ }, sty{ };
        for ( i32 i{ }; i < in.count; ++i )
        {
            const auto &o{ in.samples[ i ] };
            const f64   t{ ( o.tick - in.samples[ 0 ].tick ) * in.environment.dt };
            const f64   y{ center( o.z, in.environment.quantum ) + 0.5 * in.environment.gravity * t * t };
            st += t;
            sy += y;
            stt += t * t;
            sty += t * y;
            for ( i32 axis{ }; axis < 2; ++axis )
            {
                const i32 index{ i * 2 + axis };
                const f64 value{ axis ? o.y : o.x };
                in.low[ index ]    = cell_low( value, in.environment.quantum );
                in.high[ index ]   = cell_high( value, in.environment.quantum );
                in.center[ index ] = ( in.low[ index ] + in.high[ index ] ) * 0.5;
                in.sigma[ index ]  = ( in.high[ index ] - in.low[ index ] ) / math::sqrt( 12.0 );
            }
        }
        in.vz0 = ( sty - st * sy / in.count ) / ( stt - st * st / in.count );
        in.z0  = ( sy - in.vz0 * st ) / in.count;
        const f64 steps{ static_cast< f64 >( in.samples[ in.count - 1 ].tick - in.samples[ 0 ].tick ) };
        const f64 operations{ 2.0 * steps + 1.0 }, unit{ 5.9604644775390625e-8 };
        const f64 gamma{ operations * unit / ( 1.0 - operations * unit ) };
        f64       xy{ }, z{ };
        for ( i32 i{ }; i < in.count; ++i )
        {
            xy = math::bigger( xy, math::bigger( math::abs( in.samples[ i ].x ), math::abs( in.samples[ i ].y ) ) );
            z  = math::bigger( z, math::abs( in.samples[ i ].z ) );
        }
        const f64 travel{ steps * in.environment.maxvelocity * in.environment.dt };
        in.roundoff_xy = math::bigger( 1e-7, gamma * ( xy + travel ) );
        in.roundoff_z  = math::bigger( 1e-7, gamma * ( z + travel ) );
    }

    struct velocity_box
    {
        f64 lx{ }, hx{ }, ly{ }, hy{ }, low_speed{ }, high_speed{ }, heading{ }, radius{ };
    };
    struct target_domain
    {
        bool constrained{ }, impossible{ };
        f64  centre{ }, low{ }, high{ };
    };

    inline velocity_box velocity_cell( const window &in, i32 at )
    {
        const auto  &a{ in.samples[ at - 1 ] }, &b{ in.samples[ at ] };
        const auto  &e{ in.environment };
        const f64    magnitude{ math::bigger( math::bigger( math::abs( a.x ), math::abs( a.y ) ),
                                              math::bigger( math::abs( b.x ), math::abs( b.y ) ) ) };
        const f64    pad{ in.roundoff_xy + 128.0 * 2.2204460492503131e-16 * ( magnitude + 32.0 * e.maxvelocity * e.dt + 1.0 ) };
        velocity_box v{ };
        v.lx =
            math::clamp( ( cell_low( b.x, e.quantum ) - cell_high( a.x, e.quantum ) - 2.0 * pad ) / e.dt, -e.maxvelocity, e.maxvelocity );
        v.hx =
            math::clamp( ( cell_high( b.x, e.quantum ) - cell_low( a.x, e.quantum ) + 2.0 * pad ) / e.dt, -e.maxvelocity, e.maxvelocity );
        v.ly =
            math::clamp( ( cell_low( b.y, e.quantum ) - cell_high( a.y, e.quantum ) - 2.0 * pad ) / e.dt, -e.maxvelocity, e.maxvelocity );
        v.hy =
            math::clamp( ( cell_high( b.y, e.quantum ) - cell_low( a.y, e.quantum ) + 2.0 * pad ) / e.dt, -e.maxvelocity, e.maxvelocity );
        v.low_speed = length( v.lx > 0.0 ? v.lx : v.hx < 0.0 ? v.hx
                                                             : 0.0,
                              v.ly > 0.0 ? v.ly : v.hy < 0.0 ? v.hy
                                                             : 0.0 );
        v.heading   = heading( ( v.lx + v.hx ) * 0.5, ( v.ly + v.hy ) * 0.5 );
        const f64 xs[ 2 ]{ v.lx, v.hx }, ys[ 2 ]{ v.ly, v.hy };
        for ( const f64 x : xs )
        {
            for ( const f64 y : ys )
            {
                v.high_speed = math::bigger( v.high_speed, length( x, y ) );
                v.radius     = math::bigger( v.radius, math::abs( wrap( heading( x, y ) - v.heading ) ) );
            }
        }
        return v;
    }

    inline target_domain bound_target( const window &in )
    {
        target_domain out{ };
        const auto   &e{ in.environment };
        const f64     wish{ math::smaller( 450.0, e.maxspeed ) }, budget{ math::smaller( 30.0, e.acceleration * wish * e.dt ) };
        f64           vz_low{ -e.maxvelocity }, vz_high{ e.maxvelocity };
        for ( i32 j{ 1 }; j < in.count; ++j )
        {
            for ( i32 i{ }; i < j; ++i )
            {
                const f64 ti{ ( in.samples[ i ].tick - in.samples[ 0 ].tick ) * e.dt };
                const f64 tj{ ( in.samples[ j ].tick - in.samples[ 0 ].tick ) * e.dt };
                const f64 yi{ in.samples[ i ].z + 0.5 * e.gravity * ti * ti };
                const f64 yj{ in.samples[ j ].z + 0.5 * e.gravity * tj * tj };
                const f64 pad{ 2.0 * e.quantum + 2.0 * in.roundoff_z };
                vz_low  = math::bigger( vz_low, ( yj - yi - pad ) / ( tj - ti ) );
                vz_high = math::smaller( vz_high, ( yj - yi + pad ) / ( tj - ti ) );
            }
        }
        if ( vz_low > vz_high || vz_low <= -e.maxvelocity || vz_high >= e.maxvelocity )
        {
            return out;
        }
        if ( vz_low - e.gravity * e.dt * ( in.samples[ in.count - 1 ].tick - in.samples[ 0 ].tick ) <= -e.maxvelocity )
        {
            return out;
        }
        for ( i32 at{ 1 }; at + 1 < in.count; ++at )
        {
            if ( in.samples[ at ].gap != 1 || in.samples[ at + 1 ].gap != 1 )
            {
                continue;
            }
            const auto a{ velocity_cell( in, at ) }, b{ velocity_cell( in, at + 1 ) };
            if ( a.low_speed <= budget || b.low_speed <= 0.0 )
            {
                continue;
            }
            const f64 z_low{ vz_low - e.gravity * e.dt * ( in.samples[ at ].tick - in.samples[ 0 ].tick ) };
            const f64 z_high{ vz_high - e.gravity * e.dt * ( in.samples[ at ].tick - in.samples[ 0 ].tick - 0.5 ) };
            f64       minimum_friction{ 1.0 };
            if ( z_low <= 0.0 )
            {
                minimum_friction = math::smaller( minimum_friction, e.air_friction );
            }
            if ( z_high > 0.0 && z_low <= 250.0 )
            {
                minimum_friction = math::smaller( minimum_friction, 0.25 * e.air_friction );
            }
            const f64 gain{ math::smaller( math::smaller( wish, e.cap ) + budget, e.acceleration * wish * e.dt * minimum_friction ) };
            if ( math::bigger( math::bigger( math::abs( a.lx ), math::abs( a.hx ) ),
                               math::bigger( math::abs( a.ly ), math::abs( a.hy ) ) ) +
                     gain >=
                 e.maxvelocity )
            {
                continue;
            }
            const auto far_turn{ [ & ]( f64 speed )
            {
                const f64 ratio{ budget / speed };
                return math::atan2( gain * math::sqrt( math::bigger( 0.0, 1.0 - ratio * ratio ) ), speed - gain * ratio ) * 180.0 /
                       math::PI_D;
            } };
            const f64 far_minimum{ math::smaller( far_turn( a.low_speed ), far_turn( a.high_speed ) ) };
            const f64 observed_maximum{ math::abs( wrap( b.heading - a.heading ) ) + a.radius + b.radius };
            const f64 roundoff{ 128.0 * 2.2204460492503131e-16 * ( far_minimum + observed_maximum + 1.0 ) };
            if ( far_minimum <= observed_maximum + roundoff )
            {
                continue;
            }
            const f64 half{ a.radius + math::asin( budget / a.low_speed ) * 180.0 / math::PI_D + roundoff };
            if ( half >= 90.0 )
            {
                continue;
            }
            if ( !out.constrained )
            {
                out.constrained = true;
                out.centre      = a.heading;
                out.low         = -half;
                out.high        = half;
            } else
            {
                const f64 offset{ wrap( a.heading - out.centre ) };
                out.low  = math::bigger( out.low, offset - half );
                out.high = math::smaller( out.high, offset + half );
                if ( out.low > out.high )
                {
                    out.impossible = true;
                    return out;
                }
            }
        }
        return out;
    }

    inline i32 continuity_phase_side( const state &terminal, const control &input )
    {
        if ( input.kind != 7 && input.kind != 8 )
        {
            return 0;
        }
        return ( ( terminal.tick - input.epoch ) & 1 ? 1 : -1 ) * ( input.kind == 7 ? 1 : -1 );
    }

    inline fit fit_window( window &in, i32 continuity_side = 0 )
    {
        height( in );
        const i32  end{ in.samples[ in.count - 1 ].tick };
        fit        best{ };
        bool       first{ true };
        fit        phases[ 14 ]{ };
        i32        phase_count{ };
        const auto consider{ [ & ]( const fit &h )
        {
            if ( h.kind >= 7 && h.max_cell <= h.cell_tolerance )
            {
                phases[ phase_count++ ] = h;
            }
            if ( first || better( h, best ) )
            {
                best  = h;
                first = false;
            }
        } };
        const auto test{ [ & ]( fit h )
        {
            optimize( h, in, false, 24 );
            optimize( h, in, true, 20 );
            consider( h );
        } };
        test( make( in, 0, 0.0, 0.0, end + 1 ) );
        if ( best.max_cell <= 1e-7 )
        {
            return best;
        }
        const auto &a{ in.samples[ 0 ] }, &b{ in.samples[ in.count - 1 ] };
        const f64   rate{ wrap( heading( b.vx, b.vy ) - heading( a.vx, a.vy ) ) / ( b.tick - a.tick ) };
        test( make( in, 6, 0.0, rate, end + 1 ) );
        test( make( in, 6, 0.0, -rate, end + 1 ) );
        f64  vx{ }, vy{ };
        bool domain{ true };
        for ( i32 i{ }; i < in.count; ++i )
        {
            const auto &o{ in.samples[ i ] };
            vx += o.vx;
            vy += o.vy;
            domain = domain && length( o.vx, o.vy ) > 30.0;
        }
        if ( domain )
        {
            const f64 mean{ heading( vx, vy ) };
            const f64 targets[ 7 ]{ mean, heading( b.vx, b.vy ), mean - 5.0, mean + 5.0, mean - 90.0, mean + 90.0, mean + 180.0 };
            struct phase_pair
            {
                fit        left{ }, right{ };
                statistics cost{ };
                bool       reused{ };
            };
            phase_pair          pairs[ 7 ]{ };
            i32                 pair_count{ };
            const target_domain bound{ bound_target( in ) };
            f64                 prior_targets[ 7 ]{ };
            i32                 used_targets{ };
            for ( const f64 yaw : targets )
            {
                if ( bound.impossible )
                {
                    break;
                }
                const f64 target{ bound.constrained ? bound.centre + math::clamp( wrap( yaw - bound.centre ), bound.low, bound.high )
                                                    : wrap( yaw ) };
                bool      repeated{ };
                for ( i32 previous{ }; previous < used_targets; ++previous )
                {
                    repeated = repeated || target == prior_targets[ previous ];
                }
                if ( repeated )
                {
                    continue;
                }
                prior_targets[ used_targets++ ] = target;
                const auto prepared_fit{ [ & ]( i32 kind )
                {
                    fit h{ make( in, kind, target, 0.0, end + 1 ) };
                    if ( bound.constrained )
                    {
                        h.low[ 4 ]  = ( bound.centre + bound.low ) / h.scale[ 4 ];
                        h.high[ 4 ] = ( bound.centre + bound.high ) / h.scale[ 4 ];
                        h.p[ 4 ]    = target / h.scale[ 4 ];
                    }
                    return h;
                } };
                pairs[ pair_count ].left    = prepared_fit( 7 );
                pairs[ pair_count++ ].right = prepared_fit( 8 );
            }
            struct phase_batch
            {
                const window *in;
                phase_pair   *pairs;
            } batch{ &in, pairs };
            const auto run{ +[]( void *context, i32 index, i32 worker )
            {
                auto  &batch{ *static_cast< phase_batch * >( context ) };
                auto  &pair{ batch.pairs[ index ] };
                window local{ *batch.in };
                local.cost = &pair.cost;
                optimize( pair.left, local, false, 24 );
                optimize( pair.left, local, true, 20 );
                pair.reused = pair.cost.phase_evaluations == 0;
                if ( pair.reused )
                {
                    pair.right            = pair.left;
                    pair.right.kind       = 8;
                    pair.right.input.kind = 8;
                } else
                {
                    optimize( pair.right, local, false, 24 );
                    optimize( pair.right, local, true, 20 );
                }
            } };
            if ( pair_count < 2 || g_thread_pool.concurrency( ) < 2 || !g_thread_pool.run( run, &batch, pair_count ) )
            {
                for ( i32 at{ }; at < pair_count; ++at )
                {
                    run( &batch, at, 0 );
                }
            }

            for ( i32 at{ }; at < pair_count; ++at )
            {
                const auto &pair{ pairs[ at ] };
                consider( pair.left );
                if ( pair.reused )
                {
                    if ( pair.right.max_cell <= pair.right.cell_tolerance )
                    {
                        phases[ phase_count++ ] = pair.right;
                    }
                } else
                {
                    consider( pair.right );
                }
            }
        }
        if ( in.count >= 5 )
        {
            window prefix{ in };
            --prefix.count;
            fit old{ make( prefix, 0, 0.0, 0.0, end ) };
            optimize( old, prefix, false, 24 );
            optimize( old, prefix, true, 20 );
            if ( old.max_cell <= old.cell_tolerance )
            {
                const f64 dx{ b.vx - old.terminal.vx }, dy{ b.vy - old.terminal.vy };
                const f64 wish{ math::clamp( length( dx, dy ) / ( in.environment.acceleration * in.environment.dt * old.terminal.friction ),
                                             0.0, in.environment.maxspeed ) };
                fit       h{ make( in, 2, heading( dx, dy ), wish, end ) };
                for ( i32 i{ }; i < 4; ++i )
                {
                    h.p[ i ] = old.p[ i ];
                }
                test( h );
            }
        }
        if ( best.kind >= 7 && best.max_cell <= 1e-7 )
        {
            const auto conflicts_pair{ [ & ]( const fit &chosen_fit, const state &terminal, const control &input )
            {
                state next{ terminal }, chosen_next{ chosen_fit.terminal };
                for ( i32 horizon{ 1 }; horizon <= condition_horizon; ++horizon )
                {
                    const state previous{ next }, chosen_previous{ chosen_next };
                    step( next, input, in.environment );
                    step( chosen_next, chosen_fit.input, in.environment );
                    const f64 a{ chosen_previous.vx * chosen_next.vy - chosen_previous.vy * chosen_next.vx };
                    const f64 b{ previous.vx * next.vy - previous.vy * next.vx };
                    const f64 ar{ 128.0 * 2.2204460492503131e-16 *
                                  ( math::abs( chosen_previous.vx * chosen_next.vy ) + math::abs( chosen_previous.vy * chosen_next.vx ) +
                                    1.0 ) };
                    const f64 br{ 128.0 * 2.2204460492503131e-16 *
                                  ( math::abs( previous.vx * next.vy ) + math::abs( previous.vy * next.vx ) + 1.0 ) };
                    if ( ( a * b < 0.0 && math::abs( a ) > ar && math::abs( b ) > br ) ||
                         ( horizon == 1 &&
                           length( chosen_next.vx - next.vx, chosen_next.vy - next.vy ) > in.environment.quantum / in.environment.dt ) ||
                         length( chosen_next.x - next.x, chosen_next.y - next.y ) > 24.0 )
                    {
                        return true;
                    }
                }
                return false;
            } };
            u32 phase_veto_mask{ }, phase_strict_mask{ };
            for ( i32 at{ }; at < phase_count; ++at )
            {
                const i32 side{ continuity_phase_side( phases[ at ].terminal, phases[ at ].input ) };
                if ( !side )
                {
                    continue;
                }
                phase_veto_mask |= side > 0 ? 1u : 2u;
                if ( phases[ at ].max_cell <= 1e-7 && phases[ at ].phase_mask == 3u )
                {
                    phase_strict_mask |= side > 0 ? 1u : 2u;
                }
            }
            best.continuity_phase_veto_mask   = phase_veto_mask;
            best.continuity_phase_strict_mask = phase_strict_mask;
            const bool underidentified{ best.phase_mask != 3u };
            bool       phase_conflict{ };
            for ( i32 at{ }; at < phase_count; ++at )
            {
                if ( conflicts_pair( best, phases[ at ].terminal, phases[ at ].input ) )
                {
                    phase_conflict = true;
                }
            }
            best.phase_ambiguous = underidentified || phase_conflict;
            if ( continuity_side && phase_strict_mask == 3u && !underidentified && phase_conflict )
            {
                best.continuity_attempted = true;
                best.continuity_side      = continuity_side;
                fit  selected{ };
                bool have_selected{ };
                i32  same_side_paths{ }, strict_same_side_paths{ };
                f64  minimum_cell{ 1e300 };
                for ( i32 at{ }; at < phase_count; ++at )
                {
                    const auto &candidate{ phases[ at ] };
                    if ( continuity_phase_side( candidate.terminal, candidate.input ) != continuity_side )
                    {
                        continue;
                    }
                    ++same_side_paths;
                    minimum_cell = math::smaller( minimum_cell, candidate.max_cell );
                    if ( candidate.max_cell <= 1e-7 && candidate.phase_mask == 3u )
                    {
                        ++strict_same_side_paths;
                        if ( !have_selected || better( candidate, selected ) )
                        {
                            selected      = candidate;
                            have_selected = true;
                        }
                    }
                }
                best.continuity_same_side_paths        = same_side_paths;
                best.continuity_strict_same_side_paths = strict_same_side_paths;
                best.continuity_matching_min_cell      = minimum_cell;
                bool same_side_veto{ };
                if ( have_selected )
                {
                    for ( i32 at{ }; at < phase_count; ++at )
                    {
                        const auto &candidate{ phases[ at ] };
                        if ( continuity_phase_side( candidate.terminal, candidate.input ) == continuity_side &&
                             conflicts_pair( selected, candidate.terminal, candidate.input ) )
                        {
                            same_side_veto = true;
                        }
                    }
                }
                best.continuity_same_side_veto = same_side_veto;
                if ( have_selected && !same_side_veto )
                {
                    selected.continuity_resolved               = true;
                    selected.continuity_attempted              = true;
                    selected.continuity_side                   = continuity_side;
                    selected.continuity_same_side_paths        = same_side_paths;
                    selected.continuity_strict_same_side_paths = strict_same_side_paths;
                    selected.continuity_selected_max_cell      = selected.max_cell;
                    selected.continuity_matching_min_cell      = minimum_cell;
                    selected.continuity_same_side_veto         = false;
                    selected.continuity_phase_veto_mask        = phase_veto_mask;
                    selected.continuity_phase_strict_mask      = phase_strict_mask;
                    selected.phase_ambiguous                   = false;
                    selected.hidden_idle_tick                  = -1;
                    best                                       = selected;
                }
            }
            for ( i32 idle{ in.samples[ 0 ].tick + 1 }; idle <= end && !best.phase_ambiguous; ++idle )
            {
                state   value{ best.initial };
                control input{ best.input };
                bool    compatible{ true };
                for ( i32 at{ 1 }; at < in.count && compatible; ++at )
                {
                    while ( value.tick < in.samples[ at ].tick )
                    {
                        if ( value.tick + 1 == idle )
                        {
                            step( value, control{ }, in.environment );
                            input.kind = input.kind == 7 ? 8 : 7;
                        } else
                        {
                            step( value, input, in.environment );
                        }
                    }
                    for ( i32 axis{ }; axis < 2; ++axis )
                    {
                        const i32 index{ at * 2 + axis };
                        const f64 position{ axis ? value.y : value.x };
                        if ( position < in.low[ index ] - best.cell_tolerance || position > in.high[ index ] + best.cell_tolerance )
                        {
                            compatible = false;
                        }
                    }
                }
                if ( compatible && conflicts_pair( best, value, input ) )
                {
                    const i32 idle_side{ continuity_phase_side( value, input ) };
                    if ( idle_side )
                    {
                        best.continuity_phase_veto_mask |= idle_side > 0 ? 1u : 2u;
                    }
                    best.phase_ambiguous  = true;
                    best.hidden_idle_tick = idle;
                }
            }
        }
        return best;
    }

    result infer( const observation *samples, i32 count, const physics &environment, const state &baseline, f64 baseline_next_x,
                  f64 baseline_next_y, i32 baseline_kind, bool observed_hull_change, i32 continuity_side )
    {
        result     out{ };
        statistics cost{ };
        if ( count < 4 || count > 16 || environment.dt <= 0.0 || environment.quantum <= 0.0 || environment.acceleration <= 0.0 ||
             environment.maxspeed <= 0.0 || environment.air_friction < 0.0 )
        {
            return out;
        }
        if ( samples[ count - 1 ].tick - samples[ 0 ].tick > 32 )
        {
            return out;
        }
        const i32   windows[ 5 ]{ 16, 12, 9, 6, 4 };
        const auto &last{ samples[ count - 1 ] }, &older{ samples[ count - 2 ] };
        const f64   budget{ 2.0 * environment.quantum / environment.dt };
        const f64   baseline_cross{ baseline.vx * baseline_next_y - baseline.vy * baseline_next_x };
        bool        outside{ };
        if ( last.gap == 1 )
        {
            for ( i32 axis{ }; axis < 2; ++axis )
            {
                const f64 a{ axis ? older.y : older.x }, b{ axis ? last.y : last.x }, v{ axis ? baseline.vy : baseline.vx };
                const f64 low{ ( cell_low( b, environment.quantum ) - cell_high( a, environment.quantum ) ) / environment.dt };
                const f64 high{ ( cell_high( b, environment.quantum ) - cell_low( a, environment.quantum ) ) / environment.dt };
                outside = outside || v < low || v > high;
            }
        }
        i32 previous{ };
        for ( const i32 requested : windows )
        {
            const i32 n{ requested < count ? requested : count };
            if ( n == previous )
            {
                continue;
            }
            previous = n;
            window    in{ samples + count - n, n, environment, 0.0, 0.0, &cost };
            const fit h{ fit_window( in, continuity_side ) };
            if ( h.max_cell > 1e-7 )
            {
                continue;
            }
            if ( !out.compatible )
            {
                out.chosen = h;
                out.count  = n;
                if ( h.phase_ambiguous )
                {
                    out.compatible = 1;
                    return out;
                }
                state first_next{ h.terminal };
                step( first_next, h.input, environment );
                out.large_disagreement = length( baseline_next_x - first_next.vx, baseline_next_y - first_next.vy ) > budget;
                out.onset              = h.kind == 2 && h.cut == last.tick && observed_hull_change && outside;
                if ( out.onset )
                {
                    out.compatible = 1;
                    out.applied    = true;
                    return out;
                }
                const f64  cross{ h.terminal.vx * first_next.vy - h.terminal.vy * first_next.vx };
                const bool supported{ n >= 8 };
                if ( ( baseline_kind == 0 && h.kind == 0 ) || !supported || h.cut <= last.tick ||
                     ( !out.large_disagreement && ( h.kind < 7 || baseline_cross * cross >= 0.0 ) ) )
                {
                    out.compatible = 1;
                    return out;
                }
            } else
            {
                out.second = h;
                if ( h.phase_ambiguous )
                {
                    out.compatible = 2;
                    return out;
                }
            }
            if ( ++out.compatible == 2 )
            {
                break;
            }
        }
        if ( !out.compatible )
        {
            return out;
        }
        const auto &h{ out.chosen };
        state       next{ h.terminal };
        step( next, h.input, environment );
        bool  agreement{ };
        state second_next{ out.second.terminal };
        if ( out.compatible == 2 )
        {
            step( second_next, out.second.input, environment );
            agreement = length( h.terminal.vx - out.second.terminal.vx, h.terminal.vy - out.second.terminal.vy ) <= budget &&
                        length( next.vx - second_next.vx, next.vy - second_next.vy ) <= budget;
        }
        out.large_disagreement = length( baseline_next_x - next.vx, baseline_next_y - next.vy ) > budget && out.compatible == 2 &&
                                 length( baseline_next_x - second_next.vx, baseline_next_y - second_next.vy ) > budget;
        const bool supported{ out.count >= 8 };
        if ( agreement && supported && h.kind >= 7 && out.second.kind >= 7 )
        {
            const f64 rx{ next.vx - h.terminal.vx }, ry{ next.vy - h.terminal.vy };
            const f64 sx{ second_next.vx - out.second.terminal.vx }, sy{ second_next.vy - out.second.terminal.vy };
            const f64 bx{ baseline_next_x - baseline.vx }, by{ baseline_next_y - baseline.vy };
            const f64 candidate_cross{ h.terminal.vx * next.vy - h.terminal.vy * next.vx };
            out.phase_disagreement =
                baseline_cross * candidate_cross < 0.0 && length( rx - sx, ry - sy ) < math::smaller( length( rx, ry ), length( bx, by ) );
        }
        out.onset = h.kind == 2 && h.cut == last.tick && observed_hull_change && outside;
        const bool persistent{ agreement && supported && h.cut > last.tick };
        out.applied = !( baseline_kind == 0 && h.kind == 0 ) &&
                      ( ( persistent && ( out.large_disagreement || out.phase_disagreement ) ) || out.onset );
        return out;
    }
}

static ATLAS_FORCE_INLINE f32 input_yaw( const strafe_prediction::state &value, const strafe_prediction::control &input,
                                         const f32 *known_heading = nullptr )
{
    const f32 yaw{ known_heading ? *known_heading : heading( value.velocity.x, value.velocity.y ) };
    if ( input.kind == strafe_prediction::model::locked )
    {
        return yaw + input.yaw;
    }
    const f32 desired{ input.yaw + input.rate * static_cast< f32 >( value.tick - input.epoch ) };
    if ( !retracks( input.kind ) )
    {
        return desired;
    }
    const f32 error{ wrap( desired - yaw ) };
    return yaw + ( error > 0.f ? 90.f : -90.f ) + input.beta * error;
}

struct air_wish_basis
{
    f32 yaw, x, y;
};

struct air_replay_wishes
{
    air_wish_basis values[ 32 ];
    i32            first, count;
};

static ATLAS_FORCE_INLINE air_wish_basis prepare_air_wish( f32 wish_yaw )
{
    const f32 yaw{ DEG2RAD( wish_yaw ) };
    const f32 cosine{ math::cos( yaw ) }, sine{ math::sin( yaw ) };
    const f32 length{ math::sqrt( cosine * cosine + sine * sine ) };
    return { wish_yaw, cosine / length, sine / length };
}

static ATLAS_FORCE_INLINE f32 accelerate_air( strafe_prediction::state &value, const strafe_prediction::control &input,
                                              const strafe_prediction::physics &environment, const f32 *known_heading = nullptr,
                                              const air_wish_basis *known_wish = nullptr )
{
    using strafe_prediction::model;
    if ( input.kind == model::feedback || input.kind == model::phase_left || input.kind == model::phase_right )
    {
        const air_inference::physics physics{ air_inference::widen( environment ) };
        f64                          wish{ };
        const f64                    yaw{ air_inference::wish_direction( air_inference::widen( value ), air_inference::widen( input ), physics, wish ) };
        f64                          y{ }, x{ };
        math::sincos( yaw * math::PI_D / 180.0, y, x );
        const f64 gain{ math::bigger(
            0.0, math::smaller( math::smaller( wish, static_cast< f64 >( environment.cap ) ) - value.velocity.x * x - value.velocity.y * y,
                                static_cast< f64 >( environment.acceleration ) * wish * environment.dt * value.friction ) ) };
        value.velocity.x = static_cast< f32 >( value.velocity.x + gain * x );
        value.velocity.y = static_cast< f32 >( value.velocity.y + gain * y );
        return static_cast< f32 >( yaw );
    }

    const f32 speed{ math::clamp( input.speed, 0.f, environment.maxspeed ) };
    const f32 wish_yaw{ known_wish ? known_wish->yaw : input_yaw( value, input, known_heading ) };
    if ( input.kind != model::coast && speed > 0.f )
    {
        const air_wish_basis wish{ known_wish ? *known_wish : prepare_air_wish( wish_yaw ) };
        const f32            x{ wish.x }, y{ wish.y };
        const f32            missing{ math::smaller( speed, environment.cap ) - value.velocity.x * x - value.velocity.y * y };
        const f32            gain{ math::bigger( 0.f, math::smaller( missing, environment.acceleration * speed * environment.dt * value.friction ) ) };
        value.velocity.x += gain * x;
        value.velocity.y += gain * y;
    }
    return wish_yaw;
}

static f32 accelerate_phase_mean( strafe_prediction::state &value, strafe_prediction::control &input,
                                  const strafe_prediction::physics &environment )
{
    strafe_prediction::state   held{ value }, other{ value };
    strafe_prediction::control opposite{ input };
    ++opposite.epoch;
    held.velocity.x -= input.phase_spread_x * 0.5f;
    held.velocity.y -= input.phase_spread_y * 0.5f;
    other.velocity.x += input.phase_spread_x * 0.5f;
    other.velocity.y += input.phase_spread_y * 0.5f;
    const f32 yaw{ accelerate_air( held, input, environment ) };
    accelerate_air( other, opposite, environment );
    value.velocity.x     = ( held.velocity.x + other.velocity.x ) * 0.5f;
    value.velocity.y     = ( held.velocity.y + other.velocity.y ) * 0.5f;
    input.phase_spread_x = other.velocity.x - held.velocity.x;
    input.phase_spread_y = other.velocity.y - held.velocity.y;
    return yaw;
}

static ATLAS_FORCE_INLINE void step_air( strafe_prediction::state &value, const strafe_prediction::control &input,
                                         const strafe_prediction::physics &environment, const f32 *known_heading = nullptr,
                                         const air_wish_basis *known_wish = nullptr )
{
    value.velocity.z -= environment.gravity * environment.dt * 0.5f;
    value.velocity.x = math::clamp( value.velocity.x, -environment.max_velocity, environment.max_velocity );
    value.velocity.y = math::clamp( value.velocity.y, -environment.max_velocity, environment.max_velocity );
    value.velocity.z = math::clamp( value.velocity.z, -environment.max_velocity, environment.max_velocity );
    accelerate_air( value, input, environment, known_heading, known_wish );
    value.origin += value.velocity * environment.dt;
    value.friction = surface( value.velocity.z, environment.air_friction );
    value.velocity.z -= environment.gravity * environment.dt * 0.5f;
    value.velocity.x = math::clamp( value.velocity.x, -environment.max_velocity, environment.max_velocity );
    value.velocity.y = math::clamp( value.velocity.y, -environment.max_velocity, environment.max_velocity );
    value.velocity.z = math::clamp( value.velocity.z, -environment.max_velocity, environment.max_velocity );
    ++value.tick;
}

static ATLAS_FORCE_INLINE strafe_prediction::state initial_state( const strafe_prediction::observation *samples, i32 count,
                                                                  const strafe_prediction::physics &environment )
{
    f64 time_sum{ }, height_sum{ };
    for ( i32 at{ }; at < count; ++at )
    {
        const f64 time{ static_cast< f64 >( samples[ at ].tick - samples[ 0 ].tick ) * environment.dt };
        time_sum += time;
        height_sum += center( samples[ at ].origin.z, environment.quantum ) + 0.5 * environment.gravity * time * time;
    }
    const f64 mt{ time_sum / count }, mh{ height_sum / count };
    f64       xx{ }, xy{ };
    for ( i32 at{ }; at < count; ++at )
    {
        const f64 time{ static_cast< f64 >( samples[ at ].tick - samples[ 0 ].tick ) * environment.dt };
        const f64 height{ center( samples[ at ].origin.z, environment.quantum ) + 0.5 * environment.gravity * time * time };
        xx += ( time - mt ) * ( time - mt );
        xy += ( time - mt ) * ( height - mh );
    }
    const f32 vz{ static_cast< f32 >( xy / xx ) };
    return { { center( samples[ 0 ].origin.x, environment.quantum ), center( samples[ 0 ].origin.y, environment.quantum ),
               static_cast< f32 >( mh - vz * mt ) },
             { samples[ 0 ].velocity.x, samples[ 0 ].velocity.y, vz },
             surface( vz + environment.gravity * environment.dt * 0.5f, environment.air_friction ),
             samples[ 0 ].tick };
}

static i32 fit_height( const strafe_prediction::height_sample *samples, i32 count, const strafe_prediction::physics &environment,
                       vec2 &height_velocity, i32 *consistent_samples = nullptr )
{
    f64 lower[ 16 ]{ }, upper[ 16 ]{ }, times[ 16 ]{ };
    f64 min_velocity{ -FLT_MAX }, max_velocity{ FLT_MAX };
    i32 used{ };
    for ( i32 at{ count - 1 }; at >= 0; --at )
    {
        const strafe_prediction::height_sample &sample{ samples[ at ] };
        const f64                               time{ static_cast< f64 >( sample.tick - samples[ count - 1 ].tick ) * environment.dt };
        const f64                               gravity{ 0.5 * environment.gravity * time * time };
        const f64                               low{ sample.height + sample.shift + gravity - ( sample.height <= 0.f ? environment.quantum : 0.f ) };
        const f64                               high{ sample.height + sample.shift + gravity + ( sample.height >= 0.f ? environment.quantum : 0.f ) };
        f64                                     next_min{ min_velocity }, next_max{ max_velocity };
        for ( i32 newer{ }; newer < used; ++newer )
        {
            const f64 span{ times[ newer ] - time };
            next_min = math::bigger( next_min, ( lower[ newer ] - high ) / span );
            next_max = math::smaller( next_max, ( upper[ newer ] - low ) / span );
        }
        if ( next_min > next_max )
        {
            break; // an impulse or another change of vertical motion ends this segment.
        }
        min_velocity    = next_min;
        max_velocity    = next_max;
        lower[ used ]   = low;
        upper[ used ]   = high;
        times[ used++ ] = time;
    }
    if ( consistent_samples )
    {
        *consistent_samples = used;
    }
    if ( used < 4 )
    {
        return 0;
    }

    const f64 velocity{ ( min_velocity + max_velocity ) * 0.5 };
    f64       min_height{ -FLT_MAX }, max_height{ FLT_MAX };
    for ( i32 at{ }; at < used; ++at )
    {
        min_height = math::bigger( min_height, lower[ at ] - velocity * times[ at ] );
        max_height = math::smaller( max_height, upper[ at ] - velocity * times[ at ] );
    }
    height_velocity = { static_cast< f32 >( ( min_height + max_height ) * 0.5 ), static_cast< f32 >( velocity ) };
    return used;
}

struct air_replay_result
{
    f32                      residual[ 16 ]{ };
    strafe_prediction::state current{ };
    f64                      square{ };
    i32                      switches{ };
};

static ATLAS_FORCE_INLINE air_replay_result replay( const strafe_prediction::observation *samples, i32 count,
                                                    const strafe_prediction::state &initial, const strafe_prediction::control &input,
                                                    const strafe_prediction::physics &environment,
                                                    const air_replay_wishes *wishes = nullptr, f64 reject_above = -1.0 )
{
    air_replay_result out{ };
    out.current = initial;
    i32 previous_side{ };
    for ( i32 at{ 1 }; at < count; ++at )
    {
        while ( out.current.tick < samples[ at ].tick )
        {
            if ( samples[ at ].recategorize && out.current.tick + 1 == samples[ at ].tick )
            {
                // FinishDuck/FinishUnDuck call CategorizePosition before StartGravity.
                out.current.friction = surface( out.current.velocity.z, environment.air_friction );
            }
            if ( retracks( input.kind ) )
            {
                const f32 yaw{ heading( out.current.velocity.x, out.current.velocity.y ) };
                const f32 error{ wrap( input.yaw + input.rate * static_cast< f32 >( out.current.tick - input.epoch ) - yaw ) };
                const i32 side{ error > 0.f ? 1 : -1 };
                out.switches += previous_side != 0 && side != previous_side;
                previous_side = side;
                const bool unchanged_xy{ out.current.velocity.x >= -environment.max_velocity &&
                                         out.current.velocity.x <= environment.max_velocity &&
                                         out.current.velocity.y >= -environment.max_velocity &&
                                         out.current.velocity.y <= environment.max_velocity };
                step_air( out.current, input, environment, unchanged_xy ? &yaw : nullptr );
            } else if ( wishes && out.current.tick >= wishes->first && out.current.tick - wishes->first < wishes->count )
            {
                step_air( out.current, input, environment, nullptr, &wishes->values[ out.current.tick - wishes->first ] );
            } else
            {
                step_air( out.current, input, environment );
            }
        }
        const i32 offset{ ( at - 1 ) * 2 };
        out.residual[ offset ]     = out.current.origin.x - center( samples[ at ].origin.x, environment.quantum );
        out.residual[ offset + 1 ] = out.current.origin.y - center( samples[ at ].origin.y, environment.quantum );
        out.square += static_cast< f64 >( out.residual[ offset ] ) * out.residual[ offset ] +
                      static_cast< f64 >( out.residual[ offset + 1 ] ) * out.residual[ offset + 1 ];
        if ( reject_above >= 0.0 && at + 1 < count && out.square <= 1.7976931348623157e308 )
        {
            const f64 margin{ 512.0 * 2.2204460492503131e-16 * math::bigger( out.square, reject_above ) + 128.0 * 2.2250738585072014e-308 };
            if ( out.square > reject_above && out.square - reject_above > margin )
            {
                return out;
            }
        }
    }
    return out;
}

static ATLAS_FORCE_INLINE strafe_prediction::fit local_fit( const strafe_prediction::observation *samples, i32 count,
                                                            const strafe_prediction::control &input,
                                                            const strafe_prediction::physics &environment,
                                                            const strafe_prediction::state &original, f32 bound, i32 iterations, vec2 start,
                                                            const air_replay_wishes *wishes = nullptr )
{
    strafe_prediction::state initial{ original };
    initial.velocity.x += start.x;
    initial.velocity.y += start.y;
    air_replay_result current{ replay( samples, count, initial, input, environment, wishes ) };
    const i32         residuals{ ( count - 1 ) * 2 };
    for ( i32 iteration{ }; iteration < iterations && current.square > 1e-12; ++iteration )
    {
        strafe_prediction::state shifted{ initial };
        shifted.velocity.x += 0.5f;
        const air_replay_result rx{ replay( samples, count, shifted, input, environment, wishes ) };
        shifted = initial;
        shifted.velocity.y += 0.5f;
        const air_replay_result ry{ replay( samples, count, shifted, input, environment, wishes ) };
        f64                     xx{ }, yy{ }, xy{ }, gx{ }, gy{ };
        for ( i32 at{ }; at < residuals; ++at )
        {
            const f64 x{ ( rx.residual[ at ] - current.residual[ at ] ) * 2.0 };
            const f64 y{ ( ry.residual[ at ] - current.residual[ at ] ) * 2.0 };
            xx += x * x;
            yy += y * y;
            xy += x * y;
            gx += x * current.residual[ at ];
            gy += y * current.residual[ at ];
        }
        const f64 determinant{ xx * yy - xy * xy }, trace{ xx + yy };
        f32       dx{ }, dy{ };
        if ( determinant <= math::bigger( 1e-12, trace * trace * 1e-6 ) )
        {
            if ( trace <= 1e-12 )
            {
                break;
            }
            dx = static_cast< f32 >( -gx / trace );
            dy = static_cast< f32 >( -gy / trace );
        } else
        {
            dx = static_cast< f32 >( ( -gx * yy + gy * xy ) / determinant );
            dy = static_cast< f32 >( ( -gy * xx + gx * xy ) / determinant );
        }
        bool accepted{ };
        for ( f32 scale{ 1.f }; scale >= 0.25f; scale *= 0.5f )
        {
            strafe_prediction::state proposed{ initial };
            proposed.velocity.x = math::clamp( initial.velocity.x + dx * scale, original.velocity.x - bound, original.velocity.x + bound );
            proposed.velocity.y = math::clamp( initial.velocity.y + dy * scale, original.velocity.y - bound, original.velocity.y + bound );
            const air_replay_result trial{ replay( samples, count, proposed, input, environment, wishes, current.square - 1e-12 ) };
            if ( trial.square < current.square - 1e-12 )
            {
                current  = trial;
                initial  = proposed;
                accepted = true;
                break;
            }
        }
        if ( !accepted || math::bigger( math::abs( dx ), math::abs( dy ) ) < 0.01f )
        {
            break;
        }
    }
    if ( input.kind == strafe_prediction::model::world &&
         current.square > environment.quantum * environment.quantum * ( count - 1 ) * 0.25f )
    {
        const f32  radians{ DEG2RAD( input.yaw ) };
        const f32  cosine{ math::cos( radians ) }, sine{ math::sin( radians ) };
        const f32  length{ math::sqrt( cosine * cosine + sine * sine ) };
        const vec2 perpendicular{ -sine / length, cosine / length };
        f64        numerator{ }, denominator{ };
        for ( i32 at{ 1 }; at < count; ++at )
        {
            const f64 time{ static_cast< f64 >( samples[ at ].tick - samples[ 0 ].tick ) * environment.dt };
            numerator +=
                time * ( current.residual[ ( at - 1 ) * 2 ] * perpendicular.x + current.residual[ ( at - 1 ) * 2 + 1 ] * perpendicular.y );
            denominator += time * time;
        }
        // fixed wish cannot change perpendicular velocity; use that exact derivative when
        // float position rounding makes the two-axis finite-difference system singular;
        const f32                correction{ static_cast< f32 >( -numerator / denominator ) };
        strafe_prediction::state proposed{ initial };
        proposed.velocity.x =
            math::clamp( initial.velocity.x + perpendicular.x * correction, original.velocity.x - bound, original.velocity.x + bound );
        proposed.velocity.y =
            math::clamp( initial.velocity.y + perpendicular.y * correction, original.velocity.y - bound, original.velocity.y + bound );
        const air_replay_result trial{ replay( samples, count, proposed, input, environment, wishes, current.square ) };
        if ( trial.square < current.square )
        {
            current = trial;
            initial = proposed;
        }
    }
    return { input, initial, current.current, math::sqrt( static_cast< f32 >( current.square / ( count - 1 ) ) ), current.switches };
}

static strafe_prediction::fit fit_air_state( const strafe_prediction::observation *samples, i32 count,
                                             const strafe_prediction::control &input, const strafe_prediction::physics &environment,
                                             bool full = true, const strafe_prediction::state *known_initial = nullptr )
{
    using strafe_prediction::model;
    const strafe_prediction::state original{ known_initial ? *known_initial : initial_state( samples, count, environment ) };
    const i32                      gap{ math::bigger( samples[ 0 ].gap, 1 ) };
    const f32                      bound{ environment.quantum / ( gap * environment.dt ) +
                                          environment.acceleration * environment.maxspeed * environment.dt * ( gap - 1 ) * 0.5f };
    air_replay_wishes              prepared;
    prepared.first = original.tick;
    prepared.count = 0;
    const i32 span{ samples[ count - 1 ].tick - original.tick };
    if ( ( input.kind == model::world || input.kind == model::sweep ) && math::clamp( input.speed, 0.f, environment.maxspeed ) > 0.f &&
         span > 0 && span <= 32 )
    {
        prepared.count = span;
        for ( i32 at{ }; at < span; ++at )
        {
            const f32 desired{ input.yaw + input.rate * static_cast< f32 >( original.tick + at - input.epoch ) };
            prepared.values[ at ] = prepare_air_wish( desired );
        }
    }
    const air_replay_wishes *wishes{ prepared.count ? &prepared : nullptr };
    strafe_prediction::fit   best{ local_fit( samples, count, input, environment, original, bound, full ? 3 : 2, { }, wishes ) };
    if ( full && retracks( input.kind ) && best.switches && best.error > environment.quantum * 0.75f && bound > 0.f )
    {
        f64  errors[ 4 ]{ 1e100, 1e100, 1e100, 1e100 };
        vec2 starts[ 4 ]{ };
        for ( i32 x{ -4 }; x <= 4; ++x )
        {
            for ( i32 y{ -4 }; y <= 4; ++y )
            {
                const vec2               correction{ x * bound * 0.25f, y * bound * 0.25f };
                strafe_prediction::state initial{ original };
                initial.velocity.x += correction.x;
                initial.velocity.y += correction.y;
                const f64 error{ replay( samples, count, initial, input, environment, nullptr, errors[ 3 ] ).square };
                for ( i32 at{ }; at < 4; ++at )
                {
                    if ( error < errors[ at ] )
                    {
                        for ( i32 back{ 3 }; back > at; --back )
                        {
                            errors[ back ] = errors[ back - 1 ];
                            starts[ back ] = starts[ back - 1 ];
                        }
                        errors[ at ] = error;
                        starts[ at ] = correction;
                        break;
                    }
                }
            }
        }
        for ( const vec2 &start : starts )
        {
            const strafe_prediction::fit trial{ local_fit( samples, count, input, environment, original, bound, 3, start ) };
            if ( trial.error < best.error )
            {
                best = trial;
            }
        }
    }
    return best;
}

static strafe_prediction::fit structural_phase_evaluate( const strafe_prediction::observation *samples, i32 count,
                                                         const strafe_prediction::control &input,
                                                         const strafe_prediction::physics &environment, bool full,
                                                         const strafe_prediction::state &initial )
{
    const i32 gap{ math::bigger( samples[ 0 ].gap, 1 ) };
    const f32 bound{ environment.quantum / ( gap * environment.dt ) +
                     environment.acceleration * environment.maxspeed * environment.dt * ( gap - 1 ) * 0.5f };
    return local_fit( samples, count, input, environment, initial, bound, full ? 3 : 2, { } );
}

struct air_search
{
    const strafe_prediction::observation *samples{ };
    i32                                   count{ };
    const strafe_prediction::physics     &environment;
    strafe_prediction::fit                best[ 6 ]{ };
    strafe_prediction::fit                cheap[ 6 ][ 2 ]{ };
    const strafe_prediction::state       *phase_initial{ };
    i32                                   phase_budget{ }, phase_evaluations{ };
    strafe_prediction::state              original{ initial_state( samples, count, environment ) };

    strafe_prediction::fit evaluate( strafe_prediction::control input, bool full = false )
    {
        input.yaw   = wrap( input.yaw );
        input.beta  = math::clamp( input.beta, 0.f, 0.1f );
        input.speed = math::clamp( input.speed, 0.f, environment.maxspeed );
        if ( phase_initial && phase_evaluations++ >= phase_budget )
        {
            return strafe_prediction::fit{ };
        }
        const strafe_prediction::fit value{ phase_initial
                                                ? structural_phase_evaluate( samples, count, input, environment, full, *phase_initial )
                                                : fit_air_state( samples, count, input, environment, full, &original ) };
        const i32                    kind{ static_cast< i32 >( input.kind ) };
        if ( value.error < best[ kind ].error )
        {
            best[ kind ] = value;
        }
        if ( !full && input != cheap[ kind ][ 0 ].input && input != cheap[ kind ][ 1 ].input )
        {
            if ( value.error < cheap[ kind ][ 0 ].error )
            {
                cheap[ kind ][ 1 ] = cheap[ kind ][ 0 ];
                cheap[ kind ][ 0 ] = value;
            } else if ( value.error < cheap[ kind ][ 1 ].error )
            {
                cheap[ kind ][ 1 ] = value;
            }
        }
        return value;
    }
};

static ATLAS_FORCE_INLINE vec2 line_fit( const f32 *values, const f32 *times, i32 count )
{
    f32 unwrapped[ 9 ]{ values[ 0 ] };
    f32 mt{ }, mv{ };
    for ( i32 at{ 1 }; at < count; ++at )
    {
        unwrapped[ at ] = unwrapped[ at - 1 ] + wrap( values[ at ] - values[ at - 1 ] );
    }
    for ( i32 at{ }; at < count; ++at )
    {
        mt += times[ at ];
        mv += unwrapped[ at ];
    }
    mt /= count;
    mv /= count;
    f32 xx{ }, xy{ };
    for ( i32 at{ }; at < count; ++at )
    {
        xx += ( times[ at ] - mt ) * ( times[ at ] - mt );
        xy += ( times[ at ] - mt ) * ( unwrapped[ at ] - mv );
    }
    const f32 rate{ xx > 0.f ? xy / xx : 0.f };
    return { wrap( mv - rate * mt ), rate };
}

static ATLAS_FORCE_INLINE void propose( air_search &work )
{
    const i32 epoch{ work.samples[ 0 ].tick };
    f32       headings[ 9 ]{ }, times[ 9 ]{ }, worlds[ 8 ]{ }, relative[ 8 ]{ }, weights[ 8 ]{ }, push_times[ 8 ]{ };
    i32       pushes{ };
    for ( i32 at{ }; at < work.count; ++at )
    {
        const auto &sample{ work.samples[ at ] };
        headings[ at ] = heading( sample.velocity.x, sample.velocity.y );
        times[ at ]    = static_cast< f32 >( sample.tick - epoch );
        if ( at == 0 )
        {
            continue;
        }
        const auto &older{ work.samples[ at - 1 ] };
        const i32   a{ math::bigger( older.gap, 1 ) }, b{ math::bigger( sample.gap, 1 ) };
        const f32   span{ ( a + b ) * 0.5f };
        const vec3  push{ ( sample.velocity - older.velocity ) / span };
        const f32   weight{ push.length2d( ) };
        if ( weight <= 1e-6f )
        {
            continue;
        }
        worlds[ pushes ]     = heading( push.x, push.y );
        relative[ pushes ]   = math::abs( wrap( worlds[ pushes ] - headings[ at - 1 ] ) );
        weights[ pushes ]    = weight;
        push_times[ pushes ] = ( ( older.tick - ( a - 1 ) * 0.5f ) + ( sample.tick - ( b - 1 ) * 0.5f ) ) * 0.5f - 0.5f - epoch;
        ++pushes;
    }
    const vec2                 head{ line_fit( headings, times, work.count ) };
    const f32                  delta{ pushes ? math::median_of( relative, pushes ) : 90.f };
    strafe_prediction::control control{ strafe_prediction::model::coast, 0.f, 0.f, 0.05f, 0.f, epoch };
    work.evaluate( control );
    control.speed = work.environment.maxspeed;
    control.kind  = strafe_prediction::model::locked;
    const f32 offsets[]{ 93.f, delta };
    for ( i32 sign{ -1 }; sign <= 1; sign += 2 )
    {
        for ( const f32 offset : offsets )
        {
            control.yaw = sign * offset;
            work.evaluate( control );
        }
    }
    control.kind = strafe_prediction::model::world;
    if ( pushes )
    {
        f32 sine{ }, cosine{ };
        for ( i32 at{ }; at < pushes; ++at )
        {
            sine += weights[ at ] * math::sin( DEG2RAD( worlds[ at ] ) );
            cosine += weights[ at ] * math::cos( DEG2RAD( worlds[ at ] ) );
        }
        const f32 seeds[]{ heading( cosine, sine ), worlds[ pushes - 1 ], worlds[ 0 ] };
        for ( const f32 yaw : seeds )
        {
            control.yaw = yaw;
            work.evaluate( control );
        }
    } else
    {
        control.yaw = headings[ work.count - 1 ];
        work.evaluate( control );
    }
    control.kind = strafe_prediction::model::sweep;
    control.rate = head.y;
    for ( i32 sign{ -1 }; sign <= 1; sign += 2 )
    {
        control.yaw = head.x + sign * delta;
        work.evaluate( control );
    }
    if ( pushes >= 3 )
    {
        const vec2 world{ line_fit( worlds, push_times, pushes ) };
        if ( math::abs( world.y ) <= 12.f )
        {
            control.yaw  = world.x;
            control.rate = world.y;
            work.evaluate( control );
        }
    }
    f32 centers[ 7 ]{ }, center_times[ 7 ]{ };
    i32 points{ };
    for ( i32 at{ 2 }; at < work.count; ++at )
    {
        const vec3 chord{ work.samples[ at ].origin - work.samples[ at - 2 ].origin };
        if ( chord.length2d( ) <= work.environment.quantum )
        {
            continue;
        }
        centers[ points ]      = heading( chord.x, chord.y );
        center_times[ points ] = ( work.samples[ at ].tick + work.samples[ at - 2 ].tick ) * 0.5f - epoch;
        ++points;
    }
    vec2 drift{ head };
    f32  goals[ 3 ]{ headings[ work.count - 1 ] };
    i32  goals_count{ 1 };
    if ( points )
    {
        drift = line_fit( centers, center_times, points );
        const i32  local_count{ math::smaller( points, 3 ) };
        const vec2 local{ line_fit( centers + points - local_count, center_times + points - local_count, local_count ) };
        goals[ 0 ]  = centers[ points - 1 ];
        goals[ 1 ]  = wrap( local.x + local.y * times[ work.count - 1 ] );
        goals[ 2 ]  = wrap( drift.x + drift.y * times[ work.count - 1 ] );
        goals_count = 3;
    }
    control.kind = strafe_prediction::model::retrack;
    control.rate = 0.f;
    constexpr f32 betas[]{ 0.f, 0.05f, 0.1f };
    for ( i32 at{ }; at < goals_count; ++at )
    {
        for ( const f32 beta : betas )
        {
            control.yaw  = goals[ at ];
            control.beta = beta;
            work.evaluate( control );
        }
    }
    control.kind = strafe_prediction::model::retrack_drift;
    control.beta = 0.05f;
    constexpr f32 goal_offsets[]{ -3.f, 0.f, 3.f };
    const f32     rates[]{ drift.y, head.y };
    for ( const f32 offset : goal_offsets )
    {
        for ( const f32 rate : rates )
        {
            control.yaw  = drift.x + offset;
            control.rate = rate;
            work.evaluate( control );
        }
    }
}

static ATLAS_FORCE_INLINE void refine_air_kind( air_search &work, i32 kind )
{
    constexpr f32                widths[]{ 4.f, 1.f, 0.25f };
    constexpr f32                scales[]{ 0.25f, 0.5f, 0.75f };
    constexpr f32                final_widths[]{ 0.5f, 0.125f };
    const strafe_prediction::fit starts[]{ work.cheap[ kind ][ 0 ], work.cheap[ kind ][ 1 ] };
    for ( strafe_prediction::fit value : starts )
    {
        if ( value.error == FLT_MAX )
        {
            continue;
        }
        for ( const f32 width : widths )
        {
            const i32 dimensions{ retracks( value.input.kind ) ? 3 : 2 };
            for ( i32 dimension{ }; dimension < dimensions; ++dimension )
            {
                if ( dimension == 1 && value.input.kind != strafe_prediction::model::sweep &&
                     value.input.kind != strafe_prediction::model::retrack_drift )
                {
                    continue;
                }
                strafe_prediction::fit best{ value };
                for ( i32 sign{ -1 }; sign <= 1; sign += 2 )
                {
                    auto input{ value.input };
                    if ( dimension == 0 )
                    {
                        input.yaw += sign * width;
                    } else if ( dimension == 1 )
                    {
                        input.rate += sign * width * 0.25f;
                    } else
                    {
                        input.beta += sign * width / 200.f;
                    }
                    const auto trial{ work.evaluate( input ) };
                    if ( trial.error < best.error )
                    {
                        best = trial;
                    }
                }
                value = best;
            }
        }
        for ( const f32 scale : scales )
        {
            auto input{ value.input };
            input.speed = work.environment.maxspeed * scale;
            work.evaluate( input );
        }
    }
    const strafe_prediction::fit finals[]{ work.cheap[ kind ][ 0 ], work.cheap[ kind ][ 1 ] };
    for ( auto value : finals )
    {
        if ( value.error == FLT_MAX )
        {
            continue;
        }
        value = work.evaluate( value.input, true );
        for ( const f32 width : final_widths )
        {
            auto best{ value };
            for ( i32 sign{ -1 }; sign <= 1; sign += 2 )
            {
                auto input{ value.input };
                input.yaw += sign * width;
                const auto trial{ work.evaluate( input, true ) };
                if ( trial.error < best.error )
                {
                    best = trial;
                }
            }
            value = best;
        }
    }
}

static ATLAS_FORCE_INLINE void refine( air_search &work )
{
    if ( work.count >= 4 && !work.phase_initial && g_thread_pool.concurrency( ) >= 2 )
    {
        const auto run{ +[]( void *context, i32 index, i32 worker )
        { refine_air_kind( *static_cast< air_search * >( context ), index + 1 ); } };
        if ( g_thread_pool.run( run, &work, 5 ) )
        {
            return;
        }
    }
    for ( i32 kind{ 1 }; kind < 6; ++kind )
    {
        refine_air_kind( work, kind );
    }
}

static bool terminalize_sparse_air_state( const strafe_prediction::observation *samples, i32 count,
                                          const strafe_prediction::physics &environment, strafe_prediction::state &terminal, f32 &error )
{
    constexpr f32 MIN_OBSERVABLE_SPEED{ 60.0f };

    if ( !samples || count < 2 || count > 3 || environment.dt <= 0.0f || environment.quantum <= 0.0f || environment.maxspeed <= 0.0f )
    {
        return false;
    }

    if ( count == 2 && ( samples[ 0 ].gap != 1 || samples[ 1 ].gap < 2 ) )
    {
        return false;
    }

    for ( i32 at{ }; at < count; ++at )
    {
        if ( samples[ at ].gap < 1 || samples[ at ].gap > 3 || samples[ at ].velocity.length2d( ) < MIN_OBSERVABLE_SPEED )
        {
            return false;
        }
        if ( at > 0 )
        {
            const i32 span{ samples[ at ].tick - samples[ at - 1 ].tick };
            if ( span < 1 || span > 3 || samples[ at ].gap != span )
            {
                return false;
            }
        }
    }

    air_search work{ samples, count, environment };
    propose( work );

    i32 best{ -1 };
    f32 minimum{ FLT_MAX };
    for ( i32 kind{ }; kind < 6; ++kind )
    {
        if ( retracks( static_cast< strafe_prediction::model >( kind ) ) && work.best[ kind ].switches == 0 )
        {
            continue;
        }
        if ( work.best[ kind ].error < minimum )
        {
            minimum = work.best[ kind ].error;
            best    = kind;
        }
    }

    if ( best < 0 || minimum > environment.quantum )
    {
        return false;
    }

    const f32  model_band{ environment.quantum * 0.5f };
    const f32  velocity_tolerance{ environment.quantum / environment.dt };
    const vec3 reference{ work.best[ best ].current.velocity };
    i32        agreeing{ };
    for ( i32 kind{ }; kind < 6; ++kind )
    {
        const strafe_prediction::fit &fit{ work.best[ kind ] };
        if ( fit.error > minimum + model_band || fit.error == FLT_MAX ||
             ( retracks( static_cast< strafe_prediction::model >( kind ) ) && fit.switches == 0 ) )
        {
            continue;
        }

        const vec3 delta{ fit.current.velocity - reference };
        if ( delta.length2d( ) > velocity_tolerance )
        {
            return false;
        }
        ++agreeing;
    }

    if ( agreeing < 2 )
    {
        return false;
    }

    terminal = work.best[ best ].current;
    error    = minimum;
    return true;
}

static ATLAS_FORCE_INLINE void expand_failed_sweep( air_search &work )
{
    f32       worlds[ 8 ]{ }, push_times[ 8 ]{ };
    i32       pushes{ };
    const i32 epoch{ work.samples[ 0 ].tick };
    for ( i32 at{ 1 }; at < work.count; ++at )
    {
        const auto &sample{ work.samples[ at ] };
        const auto &older{ work.samples[ at - 1 ] };
        const i32   a{ math::bigger( older.gap, 1 ) }, b{ math::bigger( sample.gap, 1 ) };
        const f32   span{ ( a + b ) * 0.5f };
        const vec3  push{ ( sample.velocity - older.velocity ) / span };
        if ( push.length2d( ) <= 1e-6f )
        {
            continue;
        }
        worlds[ pushes ]     = heading( push.x, push.y );
        push_times[ pushes ] = ( ( older.tick - ( a - 1 ) * 0.5f ) + ( sample.tick - ( b - 1 ) * 0.5f ) ) * 0.5f - 0.5f - epoch;
        ++pushes;
    }
    if ( pushes < 3 )
    {
        return;
    }

    constexpr f32              rates[]{ -9.f, -7.f, -5.f, -3.f, 3.f, 5.f, 7.f, 9.f };
    const i32                  latest{ pushes - 1 };
    strafe_prediction::control control{ strafe_prediction::model::sweep, 0.f, 0.f, 0.05f, work.environment.maxspeed, epoch };
    for ( const f32 rate : rates )
    {
        control.yaw  = wrap( worlds[ latest ] - rate * push_times[ latest ] );
        control.rate = rate;
        work.evaluate( control );
    }
}

static bool solve_air( const strafe_prediction::observation *samples, i32 count, const strafe_prediction::physics &environment,
                       strafe_prediction::solution &out )
{
    using strafe_prediction::model;
    if ( count < 4 || count > 9 )
    {
        return false;
    }
    for ( i32 at{ 1 }; at < count; ++at )
    {
        if ( samples[ at ].tick <= samples[ at - 1 ].tick )
        {
            return false;
        }
    }
    air_search work{ samples, count, environment };
    propose( work );
    refine( work );
    const auto minimum_error{ [ & ]( )
    {
        f32 value{ FLT_MAX };
        for ( i32 kind{ }; kind < 6; ++kind )
        {
            // a controller excluded from selection cannot justify retaining the older history;
            if ( retracks( static_cast< model >( kind ) ) && work.best[ kind ].switches == 0 )
            {
                continue;
            }
            value = math::smaller( value, work.best[ kind ].error );
        }
        return value;
    } };
    f32 minimum{ minimum_error( ) };
    if ( count > 4 && minimum > 0.1f )
    {
        expand_failed_sweep( work );
        refine_air_kind( work, static_cast< i32 >( model::sweep ) );
        minimum = minimum_error( );
    }
    if ( count > 4 && minimum > 0.1f )
    {
        return solve_air( samples + count - 4, 4, environment, out );
    }
    for ( i32 kind{ }; kind < 6; ++kind )
    {
        if ( retracks( static_cast< model >( kind ) ) && work.best[ kind ].switches == 0 )
        {
            continue;
        }
        if ( work.best[ kind ].error <= minimum + environment.quantum * 0.5f )
        {
            out.chosen = work.best[ kind ];
            break;
        }
    }

    if ( out.chosen.input.kind == model::locked && math::abs( wrap( out.chosen.input.yaw ) ) > 135.f )
    {
        const vec3 chord{ samples[ count - 1 ].origin - samples[ 0 ].origin };
        if ( chord.length2d( ) > environment.quantum )
        {
            const strafe_prediction::control input{ model::world, heading( -chord.x, -chord.y ), 0.f,
                                                    0.05f, out.chosen.input.speed, samples[ 0 ].tick };
            const strafe_prediction::fit     braking{ fit_air_state( samples, count, input, environment ) };
            if ( braking.error <= minimum + environment.quantum * 0.5f )
            {
                out.chosen = braking;
            }
        }
    }
    return out.chosen.error <= environment.quantum * 2.f;
}

static f32 structural_cell_distance( f32 value, f32 observed, f32 quantum )
{
    const f32 low{ observed - ( observed <= 0.0f ? quantum : 0.0f ) };
    const f32 high{ observed + ( observed >= 0.0f ? quantum : 0.0f ) };
    return value < low ? low - value : value > high ? value - high
                                                    : 0.0f;
}

static bool structural_selectable( const strafe_prediction::fit &value )
{
    return value.error < FLT_MAX && ( !retracks( value.input.kind ) || value.switches > 0 );
}

static f32 structural_tail_error( const strafe_prediction::observation *samples, i32 count, const strafe_prediction::fit &prefix,
                                  const strafe_prediction::physics &environment, bool include_z = false )
{
    auto current{ prefix.current };
    f64  square{ };
    i32  used{ };
    for ( i32 at{ }; at < count; ++at )
    {
        if ( samples[ at ].tick <= prefix.current.tick )
        {
            continue;
        }
        while ( current.tick < samples[ at ].tick )
        {
            if ( samples[ at ].recategorize && current.tick + 1 == samples[ at ].tick )
            {
                current.friction = surface( current.velocity.z, environment.air_friction );
            }
            step_air( current, prefix.input, environment );
        }
        const f32 x{ current.origin.x - center( samples[ at ].origin.x, environment.quantum ) };
        const f32 y{ current.origin.y - center( samples[ at ].origin.y, environment.quantum ) };
        const f32 z{ include_z ? structural_cell_distance( current.origin.z, samples[ at ].origin.z, environment.quantum ) : 0.0f };
        square += static_cast< f64 >( x ) * x + static_cast< f64 >( y ) * y + static_cast< f64 >( z ) * z;
        ++used;
    }
    return used ? math::sqrt( static_cast< f32 >( square / used ) ) : FLT_MAX;
}

static bool structural_align_window( const strafe_prediction::observation *&samples, i32 &count,
                                     const strafe_prediction::solution &solution )
{
    while ( count > 0 && samples[ 0 ].tick < solution.chosen.initial.tick )
    {
        ++samples;
        --count;
    }
    return count >= 4 && samples[ 0 ].tick == solution.chosen.initial.tick;
}

static u64 structural_authority( strafe_prediction::state value, const strafe_prediction::observation *samples, i32 count,
                                 const strafe_prediction::physics &environment )
{
    u64 pattern{ };
    i32 bit{ };
    for ( i32 at{ 1 }; at < count; ++at )
    {
        while ( value.tick < samples[ at ].tick )
        {
            if ( samples[ at ].recategorize && value.tick + 1 == samples[ at ].tick )
            {
                value.friction = surface( value.velocity.z, environment.air_friction );
            }
            const u64 code{ value.friction == 1.0f ? 2ULL : value.friction == environment.air_friction ? 1ULL
                                                                                                       : 0ULL };
            pattern |= code << ( bit * 2 );
            value.velocity.z -= environment.gravity * environment.dt * 0.5f;
            value.friction = surface( value.velocity.z, environment.air_friction );
            value.velocity.z -= environment.gravity * environment.dt * 0.5f;
            ++value.tick;
            ++bit;
        }
    }
    const u64 terminal_code{ value.friction == 1.0f ? 2ULL : value.friction == environment.air_friction ? 1ULL
                                                                                                        : 0ULL };
    return pattern | ( terminal_code << ( bit * 2 ) );
}

static strafe_prediction::fit structural_speed_profile( const strafe_prediction::observation *samples, i32 count,
                                                        const strafe_prediction::physics &environment, strafe_prediction::fit current )
{
    constexpr f32 fractions[]{ 0.0625f, 0.03125f, 0.015625f };
    for ( const f32 fraction : fractions )
    {
        const f32 step_size{ environment.maxspeed * fraction };
        auto      middle_input{ current.input };
        middle_input.speed = math::clamp( current.input.speed, step_size, environment.maxspeed - step_size );
        const auto middle{ middle_input.speed == current.input.speed ? current
                                                                     : fit_air_state( samples, count, middle_input, environment, true ) };
        auto       low_input{ middle_input }, high_input{ middle_input };
        low_input.speed -= step_size;
        high_input.speed += step_size;
        const auto low{ fit_air_state( samples, count, low_input, environment, true ) };
        const auto high{ fit_air_state( samples, count, high_input, environment, true ) };
        const f64  lo{ static_cast< f64 >( low.error ) * low.error };
        const f64  mid{ static_cast< f64 >( middle.error ) * middle.error };
        const f64  hi{ static_cast< f64 >( high.error ) * high.error };
        const f64  curvature{ lo - 2.0 * mid + hi };
        if ( curvature <= 1e-10 )
        {
            break;
        }
        auto input{ middle_input };
        input.speed = math::clamp( input.speed + static_cast< f32 >( 0.5 * step_size * ( lo - hi ) / curvature ),
                                   math::bigger( 0.0f, current.input.speed - 2.0f * step_size ),
                                   math::smaller( environment.maxspeed, current.input.speed + 2.0f * step_size ) );
        const auto trial{ fit_air_state( samples, count, input, environment, true ) };
        if ( trial.error >= current.error )
        {
            break;
        }
        current = trial;
    }
    return current;
}

static bool refine_air_magnitude( const strafe_prediction::observation *samples, i32 count, const strafe_prediction::physics &environment,
                                  strafe_prediction::solution &solution )
{
    if ( !structural_align_window( samples, count, solution ) || count < 6 ||
         solution.chosen.input.kind == strafe_prediction::model::coast || retracks( solution.chosen.input.kind ) ||
         environment.acceleration <= 0.0f || environment.air_friction <= 0.0f )
    {
        return false;
    }
    const auto phase{ initial_state( samples, count, environment ) };
    const f32  end_friction{ surface( phase.velocity.z -
                                          environment.gravity * environment.dt * ( samples[ count - 1 ].tick - samples[ 0 ].tick - 0.5f ),
                                      environment.air_friction ) };
    if ( phase.friction != end_friction )
    {
        return false;
    }
    for ( i32 at{ }; at < count; ++at )
    {
        if ( samples[ at ].recategorize )
        {
            return false;
        }
    }
    const i32  prefix_count{ count - 2 };
    air_search prefix{ samples, prefix_count, environment };
    propose( prefix );
    refine( prefix );
    const i32  kind{ static_cast< i32 >( solution.chosen.input.kind ) };
    const auto original{ prefix.best[ kind ] };
    if ( !structural_selectable( original ) || original.error > environment.quantum * 2.0f )
    {
        return false;
    }
    const f32 original_tail{ structural_tail_error( samples, count, original, environment ) };
    f32       competitor{ FLT_MAX };
    for ( i32 other{ }; other < 6; ++other )
    {
        if ( other == kind || !structural_selectable( prefix.best[ other ] ) )
        {
            continue;
        }
        const f32 error{ structural_tail_error( samples, count, prefix.best[ other ], environment ) };
        competitor = math::smaller( competitor, error );
    }
    const f32 independent{ original_tail };
    if ( competitor <= independent + environment.quantum * 0.5f )
    {
        return false;
    }
    auto fitted{ structural_speed_profile( samples, prefix_count, environment, original ) };
    if ( fitted.input.speed == original.input.speed )
    {
        return false;
    }
    const f32 tail{ structural_tail_error( samples, count, fitted, environment ) };
    if ( tail >= original_tail - environment.quantum * 0.125f || tail > environment.quantum * 2.0f )
    {
        return false;
    }
    fitted = fit_air_state( samples, count, fitted.input, environment, true );
    if ( fitted.error >= solution.chosen.error || fitted.error > environment.quantum || fitted.input.kind != solution.chosen.input.kind )
    {
        return false;
    }
    solution.chosen = fitted;
    return true;
}

struct structural_phases
{
    strafe_prediction::state initial[ 3 ]{ };
    i32                      count{ };
    f32                      velocity_span{ };
};

static structural_phases structural_phase_candidates( const strafe_prediction::observation *samples, i32 count,
                                                      const strafe_prediction::height_sample *heights, i32 height_count,
                                                      const strafe_prediction::physics &environment )
{
    structural_phases out{ };
    if ( height_count < 4 || height_count > 16 || count < 4 || count > 9 || environment.gravity <= 0.0f ||
         heights[ height_count - 1 ].tick != samples[ count - 1 ].tick )
    {
        return out;
    }
    f64       low[ 16 ]{ }, high[ 16 ]{ }, times[ 16 ]{ };
    f64       minimum{ -environment.max_velocity }, maximum{ environment.max_velocity };
    const i32 latest{ samples[ count - 1 ].tick };
    for ( i32 at{ }; at < height_count; ++at )
    {
        const auto &h{ heights[ at ] };
        times[ at ] = static_cast< f64 >( h.tick - latest ) * environment.dt;
        const f64 gravity{ 0.5 * environment.gravity * times[ at ] * times[ at ] };
        low[ at ]  = h.height + h.shift + gravity - ( h.height <= 0.0f ? environment.quantum : 0.0f );
        high[ at ] = h.height + h.shift + gravity + ( h.height >= 0.0f ? environment.quantum : 0.0f );
        for ( i32 older{ }; older < at; ++older )
        {
            const f64 span{ times[ at ] - times[ older ] };
            if ( span <= 0.0 )
            {
                return out;
            }
            minimum = math::bigger( minimum, ( low[ at ] - high[ older ] ) / span );
            maximum = math::smaller( maximum, ( high[ at ] - low[ older ] ) / span );
        }
    }
    if ( minimum >= maximum )
    {
        return out;
    }
    out.velocity_span = static_cast< f32 >( maximum - minimum );
    f64        cuts[ 4 ]{ minimum, maximum };
    i32        cuts_count{ 2 };
    const auto add_cut{ [ & ]( f64 cut )
    {
        if ( cut <= minimum || cut >= maximum )
        {
            return true;
        }
        for ( i32 at{ }; at < cuts_count; ++at )
        {
            if ( math::abs( cuts[ at ] - cut ) < 1e-8 )
            {
                return true;
            }
        }
        if ( cuts_count == 4 )
        {
            return false;
        }
        i32 put{ cuts_count++ };
        while ( put > 0 && cuts[ put - 1 ] > cut )
        {
            cuts[ put ] = cuts[ put - 1 ];
            --put;
        }
        cuts[ put ] = cut;
        return true;
    } };
    constexpr f64 thresholds[]{ 0.0, 250.0 };
    for ( i32 tick{ samples[ 0 ].tick }; tick <= latest; ++tick )
    {
        for ( const f64 threshold : thresholds )
        {
            const f64 boundary{ threshold - environment.gravity * environment.dt * ( latest - tick + 0.5 ) };
            if ( !add_cut( boundary ) )
            {
                return structural_phases{ };
            }
        }
    }
    for ( i32 at{ 1 }; at < count; ++at )
    {
        if ( !samples[ at ].recategorize )
        {
            continue;
        }
        for ( const f64 threshold : thresholds )
        {
            if ( !add_cut( threshold - environment.gravity * environment.dt * ( latest - samples[ at ].tick + 1 ) ) )
            {
                return structural_phases{ };
            }
        }
    }
    for ( i32 region{ 1 }; region < cuts_count; ++region )
    {
        const f64 velocity{ ( cuts[ region - 1 ] + cuts[ region ] ) * 0.5 };
        f64       lower_height{ -FLT_MAX }, upper_height{ FLT_MAX };
        for ( i32 at{ }; at < height_count; ++at )
        {
            lower_height = math::bigger( lower_height, low[ at ] - velocity * times[ at ] );
            upper_height = math::smaller( upper_height, high[ at ] - velocity * times[ at ] );
        }
        if ( lower_height > upper_height )
        {
            continue;
        }
        const f64                z{ ( lower_height + upper_height ) * 0.5 };
        const f64                start_time{ times[ 0 ] };
        f32                      pz{ static_cast< f32 >( z + velocity * start_time - 0.5 * environment.gravity * start_time * start_time ) };
        f32                      vz{ static_cast< f32 >( velocity - environment.gravity * start_time ) };
        i32                      tick{ heights[ 0 ].tick };
        strafe_prediction::state initial{ };
        bool                     compatible{ true }, found{ false };
        for ( i32 at{ }; at < height_count; ++at )
        {
            while ( tick < heights[ at ].tick )
            {
                vz -= environment.gravity * environment.dt * 0.5f;
                pz += vz * environment.dt;
                vz -= environment.gravity * environment.dt * 0.5f;
                ++tick;
            }
            const f32 raw{ pz - heights[ at ].shift };
            const f32 float_slack{ 4.0f * 1.1920928955078125e-7f * math::bigger( math::abs( raw ), 1.0f ) };
            compatible = compatible && structural_cell_distance( raw, heights[ at ].height, environment.quantum ) <= float_slack;
            if ( tick == samples[ 0 ].tick )
            {
                initial = { { center( samples[ 0 ].origin.x, environment.quantum ), center( samples[ 0 ].origin.y, environment.quantum ),
                              pz },
                            { samples[ 0 ].velocity.x, samples[ 0 ].velocity.y, vz },
                            surface( vz + environment.gravity * environment.dt * 0.5f, environment.air_friction ),
                            tick };
                found   = true;
            }
        }
        if ( !compatible || !found )
        {
            continue;
        }
        const u64 pattern{ structural_authority( initial, samples, count, environment ) };
        bool      duplicate{ false };
        for ( i32 at{ }; at < out.count; ++at )
        {
            duplicate = duplicate || structural_authority( out.initial[ at ], samples, count, environment ) == pattern;
        }
        if ( !duplicate )
        {
            out.initial[ out.count++ ] = initial;
        }
    }
    return out;
}

static bool structural_xy_cells( const strafe_prediction::observation *samples, i32 count, const strafe_prediction::fit &fit,
                                 const strafe_prediction::physics &environment )
{
    auto state{ fit.initial };
    for ( i32 at{ 1 }; at < count; ++at )
    {
        while ( state.tick < samples[ at ].tick )
        {
            if ( samples[ at ].recategorize && state.tick + 1 == samples[ at ].tick )
            {
                state.friction = surface( state.velocity.z, environment.air_friction );
            }
            step_air( state, fit.input, environment );
        }
        const f32 slack{ 4.0f * 1.1920928955078125e-7f *
                         math::bigger( math::bigger( math::abs( state.origin.x ), math::abs( state.origin.y ) ), 1.0f ) };
        if ( structural_cell_distance( state.origin.x, samples[ at ].origin.x, environment.quantum ) > slack ||
             structural_cell_distance( state.origin.y, samples[ at ].origin.y, environment.quantum ) > slack )
        {
            return false;
        }
    }
    return true;
}

static strafe_prediction::fit structural_search_phase( const strafe_prediction::observation *samples, i32 count,
                                                       const strafe_prediction::physics &environment,
                                                       const strafe_prediction::state &initial, i32 budget = 128 )
{
    air_search work{ samples, count, environment };
    work.phase_initial = &initial;
    work.phase_budget  = budget;
    propose( work );
    for ( i32 kind{ 1 }; kind <= 3; ++kind )
    {
        refine_air_kind( work, kind );
    }
    f32 minimum{ FLT_MAX };
    for ( i32 kind{ }; kind <= 3; ++kind )
    {
        minimum = math::smaller( minimum, work.best[ kind ].error );
    }
    for ( i32 kind{ }; kind <= 3; ++kind )
    {
        if ( work.best[ kind ].error <= minimum + environment.quantum * 0.5f )
        {
            return work.best[ kind ];
        }
    }
    return strafe_prediction::fit{ };
}

static bool refine_air_joint( const strafe_prediction::observation *samples, i32 count, const strafe_prediction::height_sample *heights,
                              i32 height_count, const strafe_prediction::physics &environment, strafe_prediction::solution &solution )
{
    if ( count < 5 || count > 9 || retracks( solution.chosen.input.kind ) )
    {
        return false;
    }
    const auto phases{ structural_phase_candidates( samples, count, heights, height_count, environment ) };
    if ( !phases.count || phases.count > 2 )
    {
        return false;
    }
    const auto baseline_initial{ initial_state( samples, count, environment ) };
    const u64  baseline_pattern{ structural_authority( baseline_initial, samples, count, environment ) };
    bool       any_different{ false };
    for ( i32 at{ }; at < phases.count; ++at )
    {
        any_different = any_different || structural_authority( phases.initial[ at ], samples, count, environment ) != baseline_pattern;
    }
    if ( !any_different )
    {
        return false;
    }
    strafe_prediction::fit best{ };
    f32                    second_error{ FLT_MAX };
    for ( i32 at{ }; at < phases.count; ++at )
    {
        const auto trial{ structural_search_phase( samples, count, environment, phases.initial[ at ] ) };
        if ( trial.error < best.error )
        {
            second_error = best.error;
            best         = trial;
        } else
        {
            second_error = math::smaller( second_error, trial.error );
        }
    }
    if ( best.error >= solution.chosen.error - environment.quantum * 0.05f ||
         structural_authority( best.initial, samples, count, environment ) == baseline_pattern ||
         !structural_xy_cells( samples, count, best, environment ) )
    {
        return false;
    }
    if ( second_error <= best.error + environment.quantum * 0.25f )
    {
        return false;
    }
    const i32 prefix_count{ count - 1 };
    i32       prefix_heights{ height_count };
    while ( prefix_heights > 0 && heights[ prefix_heights - 1 ].tick > samples[ prefix_count - 1 ].tick )
    {
        --prefix_heights;
    }
    const auto prefix_phases{ structural_phase_candidates( samples, prefix_count, heights, prefix_heights, environment ) };
    if ( !prefix_phases.count || prefix_phases.count > 2 )
    {
        return false;
    }
    strafe_prediction::fit prefix_best{ };
    f32                    prefix_second{ FLT_MAX };
    for ( i32 at{ }; at < prefix_phases.count; ++at )
    {
        const auto trial{ structural_search_phase( samples, prefix_count, environment, prefix_phases.initial[ at ], 64 ) };
        if ( trial.error < prefix_best.error )
        {
            prefix_second = prefix_best.error;
            prefix_best   = trial;
        } else
        {
            prefix_second = math::smaller( prefix_second, trial.error );
        }
    }
    if ( prefix_best.input.kind != best.input.kind || prefix_second <= prefix_best.error + environment.quantum * 0.25f ||
         structural_authority( prefix_best.initial, samples, prefix_count, environment ) !=
             structural_authority( best.initial, samples, prefix_count, environment ) )
    {
        return false;
    }
    const f32 past_error{ structural_tail_error( samples, count, prefix_best, environment, true ) };
    if ( past_error > environment.quantum * 2.0f )
    {
        return false;
    }
    solution.chosen = best;
    return true;
}

static bool enhance_public_air( const strafe_prediction::observation *samples, i32 count, const strafe_prediction::physics &environment,
                                strafe_prediction::state &baseline, strafe_prediction::control &input, i32 continuity_side,
                                i32 *next_phase_singleton = nullptr, strafe_prediction::state *initial = nullptr )
{
    if ( next_phase_singleton )
    {
        *next_phase_singleton = 0;
    }
    if ( count < 4 || count > 16 )
    {
        return false;
    }
    air_inference::observation observed[ 16 ]{ };
    for ( i32 at{ }; at < count; ++at )
    {
        const auto &sample{ samples[ at ] };
        observed[ at ] = { sample.origin.x, sample.origin.y, sample.origin.z, sample.velocity.x,
                           sample.velocity.y, sample.velocity.z, sample.tick, sample.gap };
    }
    const air_inference::physics physics{ air_inference::widen( environment ) };
    strafe_prediction::state     next{ baseline };
    step_air( next, input, environment );
    const auto inference{ air_inference::infer( observed, count, physics, air_inference::widen( baseline ), next.velocity.x,
                                                next.velocity.y, static_cast< i32 >( input.kind ), samples[ count - 1 ].recategorize != 0,
                                                continuity_side ) };

    const auto &public_fit{ inference.chosen };
    i32         public_singleton{ };
    if ( public_fit.continuity_phase_strict_mask == 1u && public_fit.continuity_phase_veto_mask == 1u )
    {
        public_singleton = 1;
    } else if ( public_fit.continuity_phase_strict_mask == 2u && public_fit.continuity_phase_veto_mask == 2u )
    {
        public_singleton = -1;
    }
    const bool continuity_resolved{ inference.chosen.continuity_resolved || inference.second.continuity_resolved };
    if ( !continuity_resolved && public_fit.hidden_idle_tick >= 0 )
    {
        public_singleton = 0;
    }
    if ( next_phase_singleton )
    {
        *next_phase_singleton = public_singleton;
    }

    if ( !inference.applied )
    {
        return false;
    }
    if ( continuity_resolved && public_fit.continuity_phase_strict_mask != 3u )
    {
        return false;
    }

    const auto &selected{ inference.chosen };
    if ( initial )
    {
        *initial = air_inference::narrow( selected.initial );
    }
    baseline.origin.x   = static_cast< f32 >( selected.terminal.x );
    baseline.origin.y   = static_cast< f32 >( selected.terminal.y );
    baseline.velocity.x = static_cast< f32 >( selected.terminal.vx );
    baseline.velocity.y = static_cast< f32 >( selected.terminal.vy );
    input.kind          = static_cast< strafe_prediction::model >( selected.input.kind );
    input.epoch         = selected.input.epoch;
    input.yaw           = static_cast< f32 >( selected.input.yaw );
    input.rate          = static_cast< f32 >( selected.input.rate );
    input.beta          = static_cast< f32 >( selected.input.budget );
    f64 wish{ };
    air_inference::wish_direction( selected.terminal, selected.input, physics, wish );
    input.speed = static_cast< f32 >( wish );
    return true;
}

static ATLAS_FORCE_INLINE bool same_ground_regime( const velocity_sample_t *newer,
                                                   const velocity_sample_t *older,
                                                   i32                      max_gap = 3 )
{
    if ( !newer || !older || !newer->m_on_ground || !older->m_on_ground ||
         newer->m_wall || older->m_wall || newer->m_swimming || older->m_swimming ||
         newer->m_charging != older->m_charging || newer->m_ducked != older->m_ducked ||
         newer->m_mins != older->m_mins || newer->m_maxs != older->m_maxs ||
         newer->m_lost_footing != older->m_lost_footing ||
         newer->m_surface_friction != older->m_surface_friction ||
         math::abs( newer->m_maxspeed - older->m_maxspeed ) > 1.0f )
    {
        return false;
    }

    const bool newer_world{ newer->m_ground_index == 0 };
    const bool older_world{ older->m_ground_index == 0 };
    if ( newer->m_ground_index < 0 || older->m_ground_index < 0 || newer_world != older_world )
    {
        return false;
    }
    if ( !newer_world &&
         ( !newer->m_ground_conveyor || !older->m_ground_conveyor ||
           !newer->m_transport_valid || !older->m_transport_valid ||
           newer->m_ground_index != older->m_ground_index ||
           newer->m_ground_serial != older->m_ground_serial ) )
    {
        return false;
    }

    const i32 gap{ newer->m_tick - older->m_tick };
    return gap >= 1 && gap <= max_gap && newer->m_gap == gap &&
           older->m_gap >= 1 && older->m_gap <= max_gap;
}

static ATLAS_FORCE_INLINE i32 ground_departure_gap( const velocity_sample_t *ground,
                                                    const velocity_sample_t *air )
{
    if ( !ground || !air || !ground->m_on_ground || air->m_on_ground ||
         ground->m_ducked || air->m_ducked || ground->m_swimming || air->m_swimming ||
         ground->m_charging || air->m_charging || ground->m_wall || air->m_wall ||
         ground->m_mins != air->m_mins || ground->m_maxs != air->m_maxs )
    {
        return 0;
    }

    const i32 gap{ air->m_tick - ground->m_tick };
    return gap >= 1 && gap <= 3 && air->m_gap == gap ? gap : 0;
}

static ATLAS_FORCE_INLINE bool observed_ground_jump( const velocity_sample_t *ground,
                                                     const velocity_sample_t *air, f32 gravity,
                                                     f32 *terminal_velocity = nullptr )
{
    const i32 gap{ ground_departure_gap( ground, air ) };
    for ( i32 ticks{ 1 }; ticks <= gap; ++ticks )
    {
        const f32 duration{ TICKS_TO_TIME( ticks ) };
        const f32 rise{ duration * ( JUMP_VELOCITY - gravity * TICK_INTERVAL *
                                                         static_cast< f32 >( ticks + 1 ) * 0.5f ) };
        if ( math::abs( air->m_origin.z - ground->m_origin.z - rise ) <= 0.25f )
        {
            if ( terminal_velocity )
            {
                *terminal_velocity = ( air->m_origin.z - ground->m_origin.z ) / duration -
                                     gravity * duration * 0.5f;
            }
            return true;
        }
    }
    return false;
}

static ATLAS_FORCE_INLINE f32 air_control_acceleration( const move_sim_ctx_t *ctx )
{
    const f32 multiplier{ ctx->m_pred_air_current ? g_tf2.tf_movement_aircurrent_aircontrol_mult->GetFloat( ) : 1.0f };
    return g_tf2.sv_airaccelerate->GetFloat( ) * multiplier;
}

static ATLAS_FORCE_INLINE f32 air_current_friction( const move_sim_ctx_t *ctx )
{
    return ctx->m_pred_air_current ? g_tf2.tf_movement_aircurrent_friction_mult->GetFloat( ) : 1.0f;
}

static ATLAS_FORCE_INLINE strafe_prediction::physics air_environment( const move_sim_ctx_t *ctx )
{
    C_TFPlayer *player{ ctx->m_player_to_predict };
    const f32   gravity{ player->m_flGravity( ) != 0.f ? player->m_flGravity( ) : 1.f };
    const f32   movement_maxspeed{ strafe_prediction::movement_max_speed( ctx->m_max_speed, ctx->m_pred_lost_footing, ctx->m_charging,
                                                                          g_tf2.tf_max_charge_speed->GetFloat( ) ) };
    return { TICK_INTERVAL,
             g_tf2.sv_gravity->GetFloat( ) * gravity,
             air_control_acceleration( ctx ),
             ctx->m_air_speed_cap,
             movement_maxspeed,
             0.125f,
             air_current_friction( ctx ),
             g_tf2.sv_maxvelocity->GetFloat( ) };
}

constexpr i32 GOAL_TRIAL_HORIZON{ 24 };
constexpr i32 GOAL_TRIALS{ 32 };
constexpr f32 GOAL_TRIAL_SEPARATION{ 1.0f };
constexpr i32 GOAL_TRIALS_TO_TRUST{ 8 };
constexpr f32 GOAL_TRUST_MARGIN{ 0.5f };

struct goal_trial_t
{
    vec2 held{ }, rival{ };
    i32  tick{ };
};

struct air_seed_cache_t
{
    air_inference::stationary_cache stationary{ };
    goal_trial_t                    trials[ GOAL_TRIALS ]{ };
    f32                             goal_evidence{ };
    i32                             goal_scored{ };
    i32                             trial_count{ };
    strafe_prediction::solution     result{ };
    strafe_prediction::state        sparse_terminal{ };
    vec2                            height_velocity{ };
    f32                             sparse_error{ FLT_MAX };
    i32                             height_samples{ };
    i32                             ballistic_samples{ };
    i32                             controller_begin{ };
    u64                             fingerprint{ };
    u64                             physics_fingerprint{ };
    i32                             serial{ };
    i32                             tick{ };
    i32                             public_phase_singleton{ };
    bool                            ready{ };
    bool                            solved{ };
    bool                            sparse_terminalized{ };
    bool                            joint_terminal{ };
};

static ATLAS_FORCE_INLINE i32 air_continuity_side( const strafe_prediction::observation *samples, i32 count, i32 previous_tick,
                                                   i32 previous_side )
{
    if ( !previous_side )
    {
        return 0;
    }
    for ( i32 at{ count - 2 }; at >= 0; --at )
    {
        const i32 delta{ samples[ at + 1 ].tick - samples[ at ].tick };
        if ( delta <= 0 || samples[ at + 1 ].gap != delta )
        {
            return 0;
        }
        if ( samples[ at ].tick == previous_tick )
        {
            return ( samples[ count - 1 ].tick - previous_tick ) & 1 ? -previous_side : previous_side;
        }
    }
    return 0;
}

static ATLAS_FORCE_INLINE void constrain_public_air_terminal_xy( const velocity_sample_t *newest, const velocity_sample_t *previous,
                                                                 const strafe_prediction::physics &environment, i32 terminal_tick,
                                                                 vec3 &velocity )
{
    if ( !newest || !previous || environment.dt <= 0.0f || environment.quantum <= 0.0f || terminal_tick != newest->m_tick ||
         newest->m_gap != 1 || newest->m_tick - previous->m_tick != 1 || newest->m_on_ground || previous->m_on_ground ||
         newest->m_water_level != 0 || previous->m_water_level != 0 || newest->m_wall || previous->m_wall || newest->m_air_current ||
         previous->m_air_current || newest->m_hull != previous->m_hull || newest->m_ducked != previous->m_ducked )
    {
        return;
    }

    const auto terminal_bounds{ [ & ]( f32 current, f32 older )
    {
        const f32 current_low{ current - ( current <= 0.0f ? environment.quantum : 0.0f ) };
        const f32 current_high{ current + ( current >= 0.0f ? environment.quantum : 0.0f ) };
        const f32 older_low{ older - ( older <= 0.0f ? environment.quantum : 0.0f ) };
        const f32 older_high{ older + ( older >= 0.0f ? environment.quantum : 0.0f ) };
        return vec2{ ( current_low - older_high ) / environment.dt, ( current_high - older_low ) / environment.dt };
    } };

    const vec2 x{ terminal_bounds( newest->m_origin.x, previous->m_origin.x ) };
    const vec2 y{ terminal_bounds( newest->m_origin.y, previous->m_origin.y ) };
    velocity.x = math::clamp( velocity.x, x.x, x.y );
    velocity.y = math::clamp( velocity.y, y.x, y.y );
}

static bool air_velocity_break( const velocity_sample_t &older, const velocity_sample_t &newer,
                                const strafe_prediction::physics &environment )
{
    if ( older.m_gap < 1 || older.m_gap > 8 || newer.m_gap < 1 || newer.m_gap > 8 || newer.m_tick - older.m_tick != newer.m_gap ||
         environment.acceleration < 0.0f )
    {
        return false;
    }

    const f32 acceleration{ environment.acceleration * math::bigger( environment.maxspeed, 520.0f ) * environment.dt *
                            math::bigger( environment.air_friction, 1.0f ) };
    const f32 span{ 0.5f * static_cast< f32 >( older.m_gap + newer.m_gap ) };
    const f32 noise{ 2.0f * math::sqrt( 2.0f ) * environment.quantum / environment.dt * ( 1.0f / older.m_gap + 1.0f / newer.m_gap ) };
    const f32 limit{ acceleration * span + noise };
    return ( newer.m_velocity - older.m_velocity ).length2d_sqr( ) > limit * limit;
}

static void recover_air_window( const strafe_prediction::observation *samples, i32 count, i32 air_begin,
                                const air_inference::observation *history, i32 history_count, const strafe_prediction::physics &environment,
                                const air_inference::physics &physical, air_inference::stationary_cache &cache,
                                air_inference::stationary_result &result )
{
    constexpr u64 RECOVERY_STEPS{ 6144 };
    constexpr i32 MAX_TAIL{ 9 };

    i32 begin{ math::bigger( math::bigger( math::smaller( air_begin, count - 4 ), count - MAX_TAIL ), 0 ) };
    while ( begin < count && samples[ begin ].tick < cache.floor_tick )
    {
        ++begin;
    }
    result.cost.limit     = math::smaller( result.cost.steps + RECOVERY_STEPS, result.cost.ceiling );
    result.cost.exhausted = false;

    for ( i32 tail{ count - begin }; tail >= 4; tail = tail > 4 ? 4 : 0 )
    {
        air_search work{ samples + count - tail, tail, environment };
        propose( work );
        air_inference::stationary_seed seeds[ 6 ]{ };
        i32                            seed_count{ };
        for ( i32 kind{ }; kind < 6; ++kind )
        {
            const strafe_prediction::fit &fit{ work.best[ kind ] };
            if ( fit.error == FLT_MAX )
            {
                continue;
            }
            air_inference::stationary_seed &seed{ seeds[ seed_count++ ] };
            seed.kind           = kind;
            seed.parameter[ 0 ] = fit.initial.origin.x;
            seed.parameter[ 1 ] = fit.initial.origin.y;
            seed.parameter[ 2 ] = fit.initial.velocity.x;
            seed.parameter[ 3 ] = fit.initial.velocity.y;
            seed.parameter[ 4 ] = fit.input.yaw;
            seed.parameter[ 5 ] = fit.input.rate;
            seed.parameter[ 6 ] = fit.input.speed;
            seed.parameter[ 7 ] = fit.input.beta;
            if ( kind == 3 || kind == 5 )
            {
                seed.parameter[ 4 ] += seed.parameter[ 5 ] * ( fit.initial.tick - fit.input.epoch );
            }
        }
        if ( air_inference::recover_stationary( history, history_count, tail, physical, seeds, seed_count, cache, result ) )
        {
            return;
        }
    }
}

struct air_search_allowance_t
{
    i32 frame{ -1 };
    u64 spent{ };
};
ATLAS_NO_DESTROY static air_search_allowance_t g_air_search_allowance{ };

constexpr f32 AIR_ROUNDING_RMS{ 0.40824829f };
constexpr f32 AIR_TURN_KEPT_PER_TICK{ 0.84f };

static vec2 goal_trial_endpoint( const air_inference::stationary_fit &fit, const air_inference::physics &physical )
{
    air_inference::state value{ fit.terminal };
    for ( i32 tick{ }; tick < GOAL_TRIAL_HORIZON; ++tick )
    {
        air_inference::step( value, fit.input, physical );
    }
    return { static_cast< f32 >( value.x ), static_cast< f32 >( value.y ) };
}

static bool goal_trial_outcome( i32 index, const goal_trial_t &trial, const strafe_prediction::physics &environment, vec2 &actual )
{
    const i32                scored{ trial.tick + GOAL_TRIAL_HORIZON };
    i32                      age{ };
    const velocity_sample_t *newer{ g_player_data.velocity_sample( index, age ) };
    while ( newer && newer->m_tick > scored )
    {
        newer = g_player_data.velocity_sample( index, ++age );
    }
    if ( !newer || newer->m_tick != scored )
    {
        return false;
    }
    actual = { newer->m_origin.x, newer->m_origin.y };
    while ( newer->m_tick > trial.tick )
    {
        const velocity_sample_t *older{ g_player_data.velocity_sample( index, ++age ) };
        if ( !older || older->m_tick < trial.tick || newer->m_tick - older->m_tick != newer->m_gap || older->m_on_ground ||
             newer->m_on_ground || older->m_wall || newer->m_wall || older->m_swimming || newer->m_swimming ||
             air_velocity_break( *older, *newer, environment ) )
        {
            return false;
        }
        newer = older;
    }
    return true;
}

static void score_goal_trials( air_seed_cache_t &cached, i32 index, i32 newest_tick, const strafe_prediction::physics &environment )
{
    i32 kept{ };
    for ( i32 at{ }; at < cached.trial_count; ++at )
    {
        const goal_trial_t trial{ cached.trials[ at ] };
        if ( trial.tick + GOAL_TRIAL_HORIZON > newest_tick )
        {
            cached.trials[ kept++ ] = trial;
            continue;
        }
        vec2 actual{ };
        if ( goal_trial_outcome( index, trial, environment, actual ) )
        {
            cached.goal_evidence += trial.rival.dist_to( actual ) - trial.held.dist_to( actual );
            ++cached.goal_scored;
        }
    }
    cached.trial_count = kept;
}

static ATLAS_FORCE_INLINE bool goal_turns_trusted( const air_seed_cache_t &cached )
{
    return cached.goal_scored >= GOAL_TRIALS_TO_TRUST &&
           cached.goal_evidence < -GOAL_TRUST_MARGIN * static_cast< f32 >( cached.goal_scored );
}

constexpr i32 GOAL_HOLD_TICKS{ 8 };
constexpr f32 GOAL_APPROACH_MIN_DEG{ 10.0f };

constexpr i32 PHASE_CARRY_TICKS{ 8 };

static f32 recent_turn( i32 index, const strafe_prediction::physics &environment )
{
    f32 sum{ };
    for ( i32 age{ 1 }; age <= PHASE_CARRY_TICKS; ++age )
    {
        const velocity_sample_t *newer{ g_player_data.velocity_sample( index, age - 1 ) };
        const velocity_sample_t *older{ g_player_data.velocity_sample( index, age ) };
        if ( !older || older->m_on_ground || newer->m_on_ground || newer->m_gap != 1 || newer->m_tick - older->m_tick != 1 ||
             older->m_wall || newer->m_wall || older->m_swimming || newer->m_swimming || air_velocity_break( *older, *newer, environment ) )
        {
            return 0.0f;
        }
        const f32 speed{ older->m_velocity.length2d( ) };
        if ( speed <= 0.0f )
        {
            return 0.0f;
        }
        const vec3 change{ newer->m_velocity - older->m_velocity };
        sum += ( older->m_velocity.x * change.y - older->m_velocity.y * change.x ) / speed;
    }
    return sum / static_cast< f32 >( PHASE_CARRY_TICKS );
}

constexpr i32 POST_IMPULSE_WINDOW{ 8 };
constexpr f32 SATURATED_TURN_SHARE{ 0.6f };

static f32 saturated_turn_across_impulse( i32 index, const strafe_prediction::physics &environment )
{
    const f32 ceiling{ math::smaller( environment.cap,
                                      environment.acceleration * environment.maxspeed * environment.dt * environment.air_friction ) };
    const f32 noise{ 2.0f * math::sqrt( 2.0f ) * environment.quantum / environment.dt };
    if ( ceiling <= 0.0f )
    {
        return 0.0f;
    }
    f32  sum{ };
    i32  steps{ };
    bool crossed{ };
    for ( i32 age{ 1 }; age <= POST_IMPULSE_WINDOW + 1; ++age )
    {
        const velocity_sample_t *newer{ g_player_data.velocity_sample( index, age - 1 ) };
        const velocity_sample_t *older{ g_player_data.velocity_sample( index, age ) };
        if ( !older || older->m_on_ground || newer->m_on_ground || newer->m_gap != 1 || newer->m_tick - older->m_tick != 1 ||
             older->m_wall || newer->m_wall || older->m_swimming || newer->m_swimming || older->m_charging || newer->m_charging ||
             older->m_air_current || newer->m_air_current )
        {
            return 0.0f;
        }
        if ( air_velocity_break( *older, *newer, environment ) )
        {
            if ( crossed || steps < 2 )
            {
                return 0.0f;
            }
            crossed = true;
            continue;
        }
        const f32 speed{ older->m_velocity.length2d( ) };
        if ( speed <= 0.0f )
        {
            return 0.0f;
        }
        const vec3 change{ newer->m_velocity - older->m_velocity };
        const f32  turn{ ( older->m_velocity.x * change.y - older->m_velocity.y * change.x ) / speed };
        const f32  size{ math::abs( turn ) };
        if ( size < SATURATED_TURN_SHARE * ceiling || size > ceiling + noise || ( steps > 0 && ( turn > 0.0f ) != ( sum > 0.0f ) ) )
        {
            return 0.0f;
        }
        if ( crossed )
        {
            return sum / static_cast< f32 >( steps );
        }
        sum += turn;
        ++steps;
    }
    return 0.0f;
}

static void seed_air( move_sim_ctx_t *ctx )
{
    const i32                        index{ ctx->m_player_to_predict->entindex( ) };
    const i32                        tick{ TIME_TO_TICKS( ctx->m_player_to_predict->m_flSimulationTime( ) ) };
    const velocity_sample_t         *newest{ g_player_data.velocity_sample( index, 0 ) };
    const strafe_prediction::physics environment{ air_environment( ctx ) };

    ctx->m_air_seed_tick     = tick;
    ctx->m_air_control       = strafe_prediction::control{ };
    ctx->m_air_control.speed = 0.f;
    ctx->m_air_control.epoch = tick;
    ctx->m_wish_blast_age    = -1;

    if ( newest && !newest->m_on_ground && newest->m_gap >= 1 && newest->m_gap <= 8 )
    {
        ctx->m_air_seed_tick = newest->m_tick;
        ctx->m_pred_origin   = newest->m_origin;
        ctx->m_pred_velocity = newest->m_velocity;
        const velocity_sample_t *previous{ g_player_data.velocity_sample( index, 1 ) };
        if ( previous && previous->m_on_ground && !previous->m_swimming && newest->m_ducked && !previous->m_ducked &&
             newest->m_velocity.z > JUMP_VELOCITY )
        {
            const f32 lift{ previous->m_hull.z - newest->m_hull.z };
            if ( lift > 0.f && newest->m_origin.z - previous->m_origin.z >= lift )
            {
                ctx->m_pred_velocity.z -= lift / ( environment.dt * newest->m_gap );
            }
        }
        ctx->m_pred_velocity.z -= environment.gravity * environment.dt * newest->m_gap * 0.5f;
        if ( previous && previous->m_on_ground && ctx->m_player_to_predict->InCond( TF_COND_BLASTJUMPING ) &&
             ctx->m_pred_velocity.z > JUMP_VELOCITY + 2.0f * environment.quantum / ( environment.dt * newest->m_gap ) )
        {
            ctx->m_wish_blast_age = 0;
        }
        if ( newest->m_gap > 1 && ground_departure_gap( previous, newest ) )
        {
            f32 terminal{ };
            if ( !previous->m_lost_footing && !newest->m_lost_footing &&
                 observed_ground_jump( previous, newest, environment.gravity, &terminal ) )
            {
                // A rising step ending over an edge can imitate jump displacement. A
                // walkable support within TF's retention probe would have kept WalkMove
                // grounded; require it before assigning an ordinary upward jump state.
                trace_t    support{ };
                const vec3 below{ newest->m_origin.x, newest->m_origin.y, newest->m_origin.z - 2.0f - g_tf2.sv_stepsize->GetFloat( ) };
                g_move_sim.perform_player_move_trace( ctx->m_player_to_predict, newest->m_origin, below, support );
                if ( support.m_pEnt && !support.startsolid && !support.allsolid && support.fraction < 1.0f &&
                     support.plane.normal.z >= 0.7f )
                {
                    ctx->m_pred_velocity.z = terminal;
                }
            } else
            {
                const velocity_sample_t *older{ g_player_data.velocity_sample( index, 2 ) };
                if ( older && same_ground_regime( previous, older ) && previous->m_origin.z == older->m_origin.z )
                {
                    i32 airborne{ };
                    for ( i32 ticks{ 1 }; ticks <= newest->m_gap; ++ticks )
                    {
                        const f32 fall{ -0.5f * environment.gravity * environment.dt * environment.dt *
                                        static_cast< f32 >( ticks * ( ticks - 1 ) ) };
                        if ( math::abs( newest->m_origin.z - previous->m_origin.z - fall ) > environment.quantum )
                        {
                            continue;
                        }
                        if ( airborne != 0 )
                        {
                            airborne = 0; // two departure times remain compatible with the observed cells
                            break;
                        }
                        airborne = ticks;
                    }
                    if ( airborne > 0 )
                    {
                        // WalkMove's departure command has no vertical displacement;
                        // its terminal state has only the final half-step of gravity.
                        ctx->m_pred_velocity.z = -environment.gravity * environment.dt * ( static_cast< f32 >( airborne ) - 0.5f );
                    }
                }
            }
        }
    } else
    {
        ctx->m_pred_velocity = ctx->m_player_to_predict->m_vecVelocity( );
        const i32 raw_gap{ TIME_TO_TICKS( ctx->m_player_to_predict->m_flSimulationTime( ) -
                                          ctx->m_player_to_predict->m_flOldSimulationTime( ) ) };
        if ( raw_gap >= 1 && raw_gap <= 8 )
        {
            ctx->m_pred_velocity.z -= environment.gravity * environment.dt * raw_gap * 0.5f;
        }
    }
    const f32 mid_z{ ctx->m_pred_velocity.z + environment.gravity * environment.dt * 0.5f };
    ctx->m_pred_surface_friction = surface( mid_z, environment.air_friction );

    strafe_prediction::observation   samples[ air_inference::identification_samples ]{ };
    strafe_prediction::height_sample heights[ air_inference::identification_samples ]{ };
    i32                              count{ };
    if ( newest && !ctx->m_player_to_predict->InCond( TF_COND_PARACHUTE_ACTIVE ) )
    {
        for ( i32 age{ }; age < air_inference::identification_samples; ++age )
        {
            const velocity_sample_t *sample{ g_player_data.velocity_sample( index, age ) };
            if ( !sample )
            {
                break;
            }
            if ( newest->m_tick - sample->m_tick > 128 )
            {
                break;
            }
            if ( sample->m_on_ground || sample->m_swimming || sample->m_wall || sample->m_charging != ctx->m_charging ||
                 sample->m_air_current != newest->m_air_current || sample->m_lost_footing != newest->m_lost_footing )
            {
                break;
            }
            const bool changed_bounds{ sample->m_mins != newest->m_mins || sample->m_maxs.x != newest->m_maxs.x ||
                                       sample->m_maxs.y != newest->m_maxs.y ||
                                       ( sample->m_maxs.z != newest->m_maxs.z && sample->m_ducked == newest->m_ducked ) };
            if ( changed_bounds || math::abs( sample->m_maxspeed - newest->m_maxspeed ) > 1.f )
            {
                break;
            }
            if ( count > 0 )
            {
                const velocity_sample_t *newer{ g_player_data.velocity_sample( index, age - 1 ) };
                if ( air_velocity_break( *sample, *newer, environment ) )
                {
                    ctx->m_wish_blast_age = newest->m_tick - newer->m_tick;
                    break;
                }
                const bool duck_changed{ sample->m_ducked != newer->m_ducked || sample->m_hull != newer->m_hull };
                if ( duck_changed )
                {
                    const i32 gap{ newer->m_tick - sample->m_tick };
                    if ( gap == 1 )
                    {
                        samples[ count - 1 ].recategorize = 1;
                    } else
                    {
                        const i32 old_gap{ math::bigger( sample->m_gap, 1 ) };
                        const f32 gravity_step{ environment.gravity * environment.dt };
                        const f32 end_z{ sample->m_velocity.z - gravity_step * old_gap * 0.5f };
                        const f32 noise{ environment.quantum / ( environment.dt * old_gap ) };
                        const f32 low{ end_z - gravity_step * ( gap - 1 ) - noise };
                        const f32 high{ end_z + gravity_step * 0.5f + noise };
                        if ( ( low <= 0.f && high >= 0.f ) || ( low <= NON_GROUND_VELOCITY && high >= NON_GROUND_VELOCITY ) )
                        {
                            break;
                        }
                    }
                }
            }

            const vec3 hull_delta{ sample->m_hull - newest->m_hull };
            heights[ count ]   = { sample->m_origin.z, hull_delta.z, sample->m_tick };
            samples[ count++ ] = { sample->m_origin + hull_delta, sample->m_velocity, sample->m_tick, sample->m_gap };
        }
    }
    for ( i32 at{ }; at < count / 2; ++at )
    {
        const strafe_prediction::observation swap{ samples[ at ] };
        samples[ at ]             = samples[ count - 1 - at ];
        samples[ count - 1 - at ] = swap;
        const strafe_prediction::height_sample height{ heights[ at ] };
        heights[ at ]             = heights[ count - 1 - at ];
        heights[ count - 1 - at ] = height;
    }
    air_inference::observation history[ air_inference::identification_samples ]{ };
    const i32                  history_count{ count };
    for ( i32 at{ }; at < count; ++at )
    {
        const auto &sample{ samples[ at ] };
        history[ at ] = { sample.origin.x, sample.origin.y, sample.origin.z, sample.velocity.x, sample.velocity.y,
                          sample.velocity.z, sample.tick, sample.gap, sample.recategorize };
    }
    i32 legacy_begin{ math::bigger( 0, count - 16 ) };
    while ( legacy_begin < count && samples[ count - 1 ].tick - samples[ legacy_begin ].tick > 15 )
    {
        ++legacy_begin;
    }
    count -= legacy_begin;
    for ( i32 at{ }; at < count; ++at )
    {
        samples[ at ] = samples[ at + legacy_begin ];
        heights[ at ] = heights[ at + legacy_begin ];
    }
    i32 air_begin{ };
    while ( air_begin < count && samples[ count - 1 ].tick - samples[ air_begin ].tick > 8 )
    {
        ++air_begin;
    }
    i32 air_count{ count - air_begin };

    if ( index <= 0 || index >= MAX_TRACKED_PLAYERS )
    {
        return;
    }
    ATLAS_NO_DESTROY static air_seed_cache_t cache[ MAX_TRACKED_PLAYERS ]{ };
    air_seed_cache_t                        &cached{ cache[ index ] };
    const i32                                serial{ ctx->m_player_to_predict->GetRefEHandle( ).GetSerialNumber( ) };
    const bool                               ordinary_air_control{ ctx->m_ordinary_air_control };
    const u64 physics_fingerprint{ hash_bytes( &environment, sizeof( environment ), 14695981039346656037ULL ) };
    u64       fingerprint{ hash_bytes( samples, sizeof( samples[ 0 ] ) * count, 14695981039346656037ULL ) };
    fingerprint = hash_bytes( history, sizeof( history[ 0 ] ) * history_count, fingerprint );
    fingerprint = hash_bytes( &ordinary_air_control, sizeof( ordinary_air_control ), fingerprint );
    fingerprint = hash_bytes( heights, sizeof( heights[ 0 ] ) * count, fingerprint );
    fingerprint = hash_bytes( &environment, sizeof( environment ), fingerprint );
    fingerprint = hash_bytes( &ctx->m_charging, sizeof( ctx->m_charging ), fingerprint );
    const bool reused{ cached.ready && cached.serial == serial && cached.tick == ctx->m_air_seed_tick &&
                       cached.fingerprint == fingerprint };
    if ( !reused )
    {
        if ( cached.serial != serial )
        {
            cached.trial_count   = 0;
            cached.goal_evidence = 0.f;
            cached.goal_scored   = 0;
        }
        if ( newest )
        {
            score_goal_trials( cached, index, newest->m_tick, environment );
        }
        const i32  previous_phase_tick{ cached.tick };
        const i32  previous_phase_singleton{ cached.ready && cached.serial == serial && cached.tick < ctx->m_air_seed_tick
                                                 ? cached.public_phase_singleton
                                                 : 0 };
        i32        next_phase_singleton{ };
        const auto previous_control{ cached.result.chosen.input };
        const bool continuation_eligible{ cached.ready && cached.solved && cached.serial == serial && cached.tick < ctx->m_air_seed_tick &&
                                          cached.physics_fingerprint == physics_fingerprint && count >= 4 &&
                                          samples[ 0 ].tick <= cached.tick && static_cast< i32 >( previous_control.kind ) < 6 &&
                                          ordinary_air_control && !ctx->m_charging && !ctx->m_pred_lost_footing &&
                                          !ctx->m_pred_air_current && newest->m_water_level == 0 };
        cached.result          = strafe_prediction::solution{ };
        cached.sparse_terminal = strafe_prediction::state{ };
        cached.sparse_error    = FLT_MAX;
        cached.height_samples  = fit_height( heights, count, environment, cached.height_velocity, &cached.ballistic_samples );
        if ( cached.ballistic_samples < count )
        {
            air_begin = math::bigger( air_begin, count - cached.ballistic_samples + 1 );
            air_begin = math::smaller( air_begin, count );
            air_count = count - air_begin;
        }
        cached.controller_begin = air_begin;
        if ( !cached.ready || cached.serial != serial || cached.tick >= ctx->m_air_seed_tick )
        {
            cached.stationary = air_inference::stationary_cache{ };
        }
        bool stationary_eligible{ ( ordinary_air_control || ctx->m_charging ) && !ctx->m_pred_lost_footing && !ctx->m_pred_air_current &&
                                  history_count >= 4 };
        for ( i32 age{ }; age < history_count && stationary_eligible; ++age )
        {
            stationary_eligible = g_player_data.velocity_sample( index, age )->m_water_level == 0;
        }
        air_inference::stationary_result stationary{ };
        if ( stationary_eligible )
        {
            const air_inference::physics physical{ air_inference::widen( environment ) };
            constexpr u64                FRAME_SEARCH_STEPS{ 16384 };
            auto                        &allowance{ g_air_search_allowance };
            if ( allowance.frame != g_tf2.m_globals->framecount )
            {
                allowance = { g_tf2.m_globals->framecount, 0 };
            }
            if ( cached.stationary.ready )
            {
                for ( i32 end{ 3 }; end < history_count - 1; ++end )
                {
                    if ( history[ end ].tick <= cached.stationary.tick )
                    {
                        continue;
                    }
                    const auto missed{ air_inference::identify_stationary( history, end + 1, physical, cached.stationary, false ) };
                    allowance.spent += missed.cost.steps;
                    if ( !missed.consistent )
                    {
                        break;
                    }
                }
            }
            const u64                           available{ FRAME_SEARCH_STEPS > allowance.spent ? FRAME_SEARCH_STEPS - allowance.spent : 0 };
            const bool                          search{ available >= air_inference::stationary_search_steps };
            air_inference::stationary_challenge challenge{ };
            stationary              = air_inference::identify_stationary( history, history_count, physical, cached.stationary, search,
                                                                          goal_turns_trusted( cached ), &challenge );
            stationary.cost.ceiling = available;
            if ( challenge.valid && cached.trial_count < GOAL_TRIALS )
            {
                const vec2 held{ goal_trial_endpoint( challenge.held, physical ) };
                const vec2 rival{ goal_trial_endpoint( challenge.rival, physical ) };
                if ( held.dist_to( rival ) > GOAL_TRIAL_SEPARATION )
                {
                    cached.trials[ cached.trial_count++ ] = { held, rival, challenge.held.terminal.tick };
                }
            }
            if ( !stationary.consistent &&
                 !air_inference::identify_change( history, history_count, physical, cached.stationary, stationary ) && search )
            {
                recover_air_window( samples, count, air_begin, history, history_count, environment, physical, cached.stationary,
                                    stationary );
            }
            allowance.spent += stationary.cost.steps;
            if ( stationary.consistent )
            {
                const auto &fit{ stationary.chosen };
                f64         wish{ };
                air_inference::wish_direction( fit.terminal, fit.input, physical, wish );
                auto &chosen{ cached.result.chosen };
                chosen.input = { static_cast< strafe_prediction::model >( fit.input.kind ),
                                 static_cast< f32 >( fit.input.yaw ),
                                 static_cast< f32 >( fit.input.rate ),
                                 static_cast< f32 >( fit.input.budget ),
                                 static_cast< f32 >( wish ),
                                 fit.input.epoch };
                air_inference::state terminal{ fit.terminal };
                if ( chosen.input.kind == strafe_prediction::model::phase_left ||
                     chosen.input.kind == strafe_prediction::model::phase_right )
                {
                    air_inference::stationary_cost parity{ };
                    air_inference::stationary_fit  other{ };
                    chosen.input.phase_known = air_inference::phase_observed( history, history_count, fit, physical, parity, other );
                    allowance.spent += parity.steps;
                    if ( !chosen.input.phase_known )
                    {
                        chosen.input.phase_spread_x = static_cast< f32 >( other.terminal.vx - terminal.vx );
                        chosen.input.phase_spread_y = static_cast< f32 >( other.terminal.vy - terminal.vy );
                        terminal.x                  = ( terminal.x + other.terminal.x ) * 0.5;
                        terminal.y                  = ( terminal.y + other.terminal.y ) * 0.5;
                        terminal.vx                 = ( terminal.vx + other.terminal.vx ) * 0.5;
                        terminal.vy                 = ( terminal.vy + other.terminal.vy ) * 0.5;
                    }
                }
                chosen.initial = air_inference::narrow( fit.initial );
                chosen.current = air_inference::narrow( terminal );
                chosen.error   = environment.quantum * static_cast< f32 >( math::sqrt( fit.square / ( 24.0 * stationary.count ) ) );
            }
        } else
        {
            cached.stationary = air_inference::stationary_cache{ };
        }
        bool continued{ };
        if ( !stationary.consistent && continuation_eligible && air_count >= 4 )
        {
            const auto fit{ fit_air_state( samples + air_begin, air_count, previous_control, environment, false ) };
            if ( fit.error <= environment.quantum * 0.5f )
            {
                cached.result.chosen = fit;
                continued            = true;
            }
        }
        cached.solved = stationary.consistent || continued ||
                        ( !stationary_eligible && !ctx->m_charging && air_count >= 4 && environment.maxspeed > 0.f &&
                          solve_air( samples + air_begin, air_count, environment, cached.result ) );
        if ( cached.solved && !stationary.consistent && !continued )
        {
            refine_air_magnitude( samples + air_begin, air_count, environment, cached.result );
        }

        cached.joint_terminal = false;
        if ( cached.solved && !stationary.consistent && !continued && cached.height_samples >= 4 )
        {
            cached.joint_terminal = refine_air_joint( samples + air_begin, air_count, heights + count - cached.height_samples,
                                                      cached.height_samples, environment, cached.result );
            if ( cached.joint_terminal )
            {
                cached.height_velocity = { cached.result.chosen.current.origin.z, cached.result.chosen.current.velocity.z };
            }
        }

        cached.sparse_terminalized =
            !cached.solved && !ctx->m_charging && air_count >= 2 && air_count <= 3 &&
            terminalize_sparse_air_state( samples + air_begin, air_count, environment, cached.sparse_terminal, cached.sparse_error );
        if ( cached.solved && !stationary.consistent && !continued && !ctx->m_charging && !ctx->m_pred_lost_footing &&
             !ctx->m_pred_air_current && newest && newest->m_water_level == 0 )
        {
            i32 inference_count{ count };
            if ( cached.ballistic_samples < count )
            {
                inference_count = cached.ballistic_samples - 1;
            }
            const i32 continuity_side{ inference_count >= 4 ? air_continuity_side( samples + count - inference_count, inference_count,
                                                                                   previous_phase_tick, previous_phase_singleton )
                                                            : 0 };
            bool      ordinary_history{ ordinary_air_control };
            for ( i32 age{ }; age < inference_count; ++age )
            {
                ordinary_history = ordinary_history && g_player_data.velocity_sample( index, age )->m_water_level == 0;
            }
            strafe_prediction::state baseline{ cached.result.chosen.current };
            const velocity_sample_t *previous{ g_player_data.velocity_sample( index, 1 ) };
            constrain_public_air_terminal_xy( newest, previous, environment, baseline.tick, baseline.velocity );
            if ( cached.height_samples )
            {
                baseline.origin.z   = cached.height_velocity.x;
                baseline.velocity.z = cached.height_velocity.y;
            }
            baseline.friction = cached.joint_terminal ? cached.result.chosen.current.friction
                                                      : surface( baseline.velocity.z + environment.gravity * environment.dt * 0.5f,
                                                                 environment.air_friction );
            if ( ordinary_history && inference_count >= 4 &&
                 enhance_public_air( samples + count - inference_count, inference_count, environment, baseline, cached.result.chosen.input,
                                     continuity_side, &next_phase_singleton, &cached.result.chosen.initial ) )
            {
                cached.result.chosen.current.origin.x   = baseline.origin.x;
                cached.result.chosen.current.origin.y   = baseline.origin.y;
                cached.result.chosen.current.velocity.x = baseline.velocity.x;
                cached.result.chosen.current.velocity.y = baseline.velocity.y;
            }
        }
        cached.public_phase_singleton = next_phase_singleton;
        cached.physics_fingerprint    = physics_fingerprint;
        cached.serial                 = serial;
        cached.tick                   = ctx->m_air_seed_tick;
        cached.fingerprint            = fingerprint;
        cached.ready                  = true;
    }
    air_begin = cached.controller_begin;
    air_count = count - air_begin;
    if ( cached.ballistic_samples > 0 && cached.ballistic_samples < count )
    {
        const i32 age{ samples[ count - 1 ].tick - samples[ count - cached.ballistic_samples ].tick };
        ctx->m_wish_blast_age = ctx->m_wish_blast_age < 0 ? age : math::smaller( ctx->m_wish_blast_age, age );
    }
    const bool early_impulse{ ctx->m_wish_blast_age >= 0 && ctx->m_wish_blast_age < POST_IMPULSE_WINDOW };
    const f32  early_turn{ early_impulse && goal_turns_trusted( cached ) ? saturated_turn_across_impulse( index, environment ) : 0.0f };
    if ( cached.solved )
    {
        const strafe_prediction::fit &selected{ cached.result.chosen };
        vec3                          terminal_velocity{ selected.current.velocity };
        const velocity_sample_t      *previous{ g_player_data.velocity_sample( index, 1 ) };
        constrain_public_air_terminal_xy( newest, previous, environment, selected.current.tick, terminal_velocity );
        ctx->m_air_control     = selected.input;
        ctx->m_air_seed_tick   = selected.current.tick;
        ctx->m_pred_origin.x   = selected.current.origin.x;
        ctx->m_pred_origin.y   = selected.current.origin.y;
        ctx->m_pred_velocity.x = terminal_velocity.x;
        ctx->m_pred_velocity.y = terminal_velocity.y;
        ctx->m_wishspeed       = selected.input.speed;
        ctx->m_walk_wishspeed  = ctx->m_wishspeed;
        ctx->m_wish_valid      = selected.input.kind != strafe_prediction::model::coast && ctx->m_wishspeed > 0.f;
        ctx->m_air_turn_fades  = selected.error > AIR_ROUNDING_RMS * environment.quantum;
        ctx->m_air_turn_kept   = 1.f;
        if ( selected.input.kind == strafe_prediction::model::retrack && goal_turns_trusted( cached ) &&
             ctx->m_player_to_predict->InCond( TF_COND_BLASTJUMPING ) )
        {
            const f32 approach{ wrap( selected.input.yaw - heading( selected.initial.velocity.x, selected.initial.velocity.y ) ) };
            if ( math::abs( approach ) >= GOAL_APPROACH_MIN_DEG )
            {
                ctx->m_air_goal_side = approach > 0.0f ? 1 : -1;
            }
        }
    } else if ( cached.sparse_terminalized )
    {
        ctx->m_pred_velocity.x = cached.sparse_terminal.velocity.x;
        ctx->m_pred_velocity.y = cached.sparse_terminal.velocity.y;
    }
    if ( early_turn != 0.0f && ( !cached.solved || ctx->m_air_control.kind == strafe_prediction::model::coast ) )
    {
        ctx->m_air_control       = strafe_prediction::control{ };
        ctx->m_air_control.kind  = strafe_prediction::model::locked;
        ctx->m_air_control.yaw   = early_turn > 0.0f ? 90.0f : -90.0f;
        ctx->m_air_control.speed = math::smaller( math::abs( early_turn ) / ( environment.acceleration * environment.dt ),
                                                  environment.maxspeed );
        ctx->m_air_control.epoch = ctx->m_air_seed_tick;
        ctx->m_wishspeed         = ctx->m_air_control.speed;
        ctx->m_walk_wishspeed    = ctx->m_wishspeed;
        ctx->m_wish_valid        = ctx->m_wishspeed > 0.f;
        ctx->m_air_turn_fades    = false;
    }
    const bool phase_hold{ ctx->m_air_control.kind == strafe_prediction::model::phase_left ||
                           ctx->m_air_control.kind == strafe_prediction::model::phase_right };
    if ( phase_hold && goal_turns_trusted( cached ) && ctx->m_player_to_predict->InCond( TF_COND_BLASTJUMPING ) )
    {
        const f32 carried{ recent_turn( index, environment ) };
        if ( carried != 0.0f )
        {
            ctx->m_air_control       = strafe_prediction::control{ };
            ctx->m_air_control.kind  = strafe_prediction::model::locked;
            ctx->m_air_control.yaw   = carried > 0.0f ? 90.0f : -90.0f;
            ctx->m_air_control.speed = math::smaller( math::abs( carried ) / ( environment.acceleration * environment.dt ),
                                                      environment.maxspeed );
            ctx->m_air_control.epoch = ctx->m_air_seed_tick;
            ctx->m_wishspeed         = ctx->m_air_control.speed;
            ctx->m_walk_wishspeed    = ctx->m_wishspeed;
            ctx->m_wish_valid        = ctx->m_wishspeed > 0.f;
            ctx->m_air_turn_fades    = false;
        }
    }

    if ( cached.height_samples )
    {
        ctx->m_pred_origin.z   = cached.height_velocity.x;
        ctx->m_pred_velocity.z = cached.height_velocity.y;
    }
    ctx->m_pred_velocity.x = math::clamp( ctx->m_pred_velocity.x, -environment.max_velocity, environment.max_velocity );
    ctx->m_pred_velocity.y = math::clamp( ctx->m_pred_velocity.y, -environment.max_velocity, environment.max_velocity );
    ctx->m_pred_velocity.z = math::clamp( ctx->m_pred_velocity.z, -environment.max_velocity, environment.max_velocity );
    const f32 middle{ ctx->m_pred_velocity.z + environment.gravity * environment.dt * 0.5f };
    ctx->m_pred_surface_friction = surface( middle, environment.air_friction );
    if ( cached.joint_terminal )
    {
        ctx->m_pred_surface_friction = cached.result.chosen.current.friction;
    }
    if ( cached.solved )
    {
        strafe_prediction::state first{ ctx->m_pred_origin, ctx->m_pred_velocity, ctx->m_pred_surface_friction, ctx->m_air_seed_tick };
        ctx->m_wish_yaw = accelerate_air( first, ctx->m_air_control, environment );
    }
}

struct water_seed_control_t
{
    vec3 direction{ };
    f32  speed{ };
};

struct water_seed_cache_t
{
    vec3 terminal{ };
    vec3 direction{ };
    f32  speed{ };
    u64  fingerprint{ };
    i32  serial{ };
    i32  tick{ };
    u8   water_level{ };
    bool ready{ };
    bool valid{ };
};

static ATLAS_FORCE_INLINE void water_seed_step( vec3 &velocity, const water_seed_control_t &control,
                                                f32 friction, f32 acceleration )
{
    const f32 speed{ velocity.length( ) };
    if ( speed > 0.0f )
    {
        const f32 drag{ TICK_INTERVAL * speed * friction };
        const f32 next{ speed - drag < 0.1f ? 0.0f : speed - drag };
        velocity *= next / speed;
    }

    const f32 missing{ control.speed - velocity.length( ) };
    if ( missing > 0.0f )
    {
        const f32 gained{ math::smaller( acceleration * control.speed * TICK_INTERVAL, missing ) };
        velocity += control.direction * gained;
    }
}

static ATLAS_FORCE_INLINE bool same_water_seed_regime( const velocity_sample_t *newer,
                                                       const velocity_sample_t *older )
{
    if ( !newer || !older || newer->m_water_level != 3 || older->m_water_level != 3 ||
         newer->m_on_ground || older->m_on_ground || newer->m_wall || older->m_wall ||
         newer->m_ducked != older->m_ducked || newer->m_air_current || older->m_air_current ||
         newer->m_lost_footing || older->m_lost_footing || newer->m_charging || older->m_charging ||
         newer->m_eye_pitch != older->m_eye_pitch ||
         math::abs( newer->m_maxspeed - older->m_maxspeed ) > 1.0f ||
         newer->m_mins != older->m_mins || newer->m_maxs != older->m_maxs ||
         newer->m_surface_friction != 1.0f || older->m_surface_friction != 1.0f )
    {
        return false;
    }

    const i32 gap{ newer->m_tick - older->m_tick };
    return gap > 0 && gap <= 8;
}

static ATLAS_FORCE_INLINE f32 water_seed_quantization_bound( const velocity_sample_t *oldest,
                                                             i32                      elapsed_ticks )
{
    constexpr f32 Q{ 0.125f };
    const i32     seed_gap{ math::bigger( oldest->m_gap, 1 ) };
    const f32     axis{ Q * ( 1.0f + static_cast< f32 >( elapsed_ticks ) / seed_gap ) };
    return axis * 1.7320508075688772f;
}

static f32 water_seed_candidate_bound( const velocity_sample_t *older, i32 interval_ticks,
                                       const water_seed_control_t &control )
{
    vec3      terminal{ older->m_velocity };
    const i32 seed_gap{ math::bigger( older->m_gap, 1 ) };
    for ( i32 tick{ }; tick < seed_gap; ++tick )
    {
        water_seed_step( terminal, control, g_tf2.sv_friction->GetFloat( ),
                         g_tf2.sv_accelerate->GetFloat( ) );
    }
    const f32 phase_velocity{ 0.5f * ( terminal - older->m_velocity ).length( ) };
    return water_seed_quantization_bound( older, interval_ticks ) +
           phase_velocity * TICK_INTERVAL * static_cast< f32 >( interval_ticks );
}

static f32 replay_water_seed_interval( const velocity_sample_t *older, const velocity_sample_t *newer,
                                       const water_seed_control_t &control, vec3 *terminal )
{
    vec3      origin{ older->m_origin };
    vec3      velocity{ older->m_velocity };
    const i32 gap{ newer->m_tick - older->m_tick };
    for ( i32 tick{ }; tick < gap; ++tick )
    {
        water_seed_step( velocity, control, g_tf2.sv_friction->GetFloat( ),
                         g_tf2.sv_accelerate->GetFloat( ) );
        origin += velocity * TICK_INTERVAL;
    }
    if ( terminal )
    {
        *terminal = velocity;
    }
    return ( origin - newer->m_origin ).length( );
}

static u64 water_seed_fingerprint( const velocity_sample_t *const *samples, i32 count,
                                   f32 cap, f32 friction, f32 acceleration, f32 tick_interval )
{
    u64 hash{ 14695981039346656037ULL };
    hash = hash_value( count, hash );
    hash = hash_value( cap, hash );
    hash = hash_value( friction, hash );
    hash = hash_value( acceleration, hash );
    hash = hash_value( tick_interval, hash );
    for ( i32 at{ }; at < count; ++at )
    {
        const auto &s{ *samples[ at ] };
        hash = hash_value( s.m_origin, hash );
        hash = hash_value( s.m_velocity, hash );
        hash = hash_value( s.m_mins, hash );
        hash = hash_value( s.m_maxs, hash );
        hash = hash_value( s.m_tick, hash );
        hash = hash_value( s.m_gap, hash );
        hash = hash_value( s.m_maxspeed, hash );
        hash = hash_value( s.m_surface_friction, hash );
        hash = hash_value( s.m_eye_pitch, hash );
        hash = hash_value( s.m_water_level, hash );
        hash = hash_value( s.m_on_ground, hash );
        hash = hash_value( s.m_ducked, hash );
        hash = hash_value( s.m_wall, hash );
        hash = hash_value( s.m_air_current, hash );
        hash = hash_value( s.m_lost_footing, hash );
        hash = hash_value( s.m_charging, hash );
    }
    return hash;
}

static bool seed_water_controller( move_sim_ctx_t *ctx, const velocity_sample_t *newest )
{
    constexpr i32 MAX_POINTS{ 7 };
    if ( !newest || newest->m_water_level != 3 || newest->m_on_ground || newest->m_wall ||
         newest->m_air_current || newest->m_lost_footing || newest->m_charging ||
         newest->m_surface_friction != 1.0f )
    {
        return false;
    }

    constexpr f32 PITCH_QUANTUM{ 180.0f / 255.0f };
    if ( math::abs( newest->m_eye_pitch ) <= PITCH_QUANTUM && newest->m_velocity.z > 0.0f )
    {
        return false;
    }

    const velocity_sample_t *reverse[ MAX_POINTS ]{ newest };
    i32                      count{ 1 };
    for ( i32 age{ 1 }; age < MAX_POINTS; ++age )
    {
        const velocity_sample_t *older{
            g_player_data.velocity_sample( ctx->m_player_to_predict->entindex( ), age )
        };
        if ( !same_water_seed_regime( reverse[ count - 1 ], older ) )
        {
            break;
        }
        reverse[ count++ ] = older;
    }
    if ( count < 4 )
    {
        return false;
    }

    const velocity_sample_t *samples[ MAX_POINTS ]{ };
    for ( i32 at{ }; at < count; ++at )
    {
        samples[ at ] = reverse[ count - 1 - at ];
    }

    const f32 cap{
        strafe_prediction::movement_max_speed( samples[ count - 2 ]->m_maxspeed, false ) * 0.8f
    };
    const f32 friction{ g_tf2.sv_friction->GetFloat( ) };
    const f32 acceleration{ g_tf2.sv_accelerate->GetFloat( ) };
    const i32 index{ ctx->m_player_to_predict->entindex( ) };
    if ( index <= 0 || index >= MAX_TRACKED_PLAYERS )
    {
        return false;
    }

    const u64 fingerprint{
        water_seed_fingerprint( samples, count, cap, friction, acceleration, TICK_INTERVAL )
    };
    const i32                                  serial{ ctx->m_player_to_predict->GetRefEHandle( ).GetSerialNumber( ) };
    ATLAS_NO_DESTROY static water_seed_cache_t cache[ MAX_TRACKED_PLAYERS ]{ };
    water_seed_cache_t                        &cached{ cache[ index ] };
    if ( cached.ready && cached.serial == serial && cached.tick == newest->m_tick &&
         cached.water_level == newest->m_water_level && cached.fingerprint == fingerprint )
    {
        if ( !cached.valid )
        {
            return false;
        }
        ctx->m_pred_origin             = newest->m_origin;
        ctx->m_pred_velocity           = cached.terminal;
        ctx->m_water_control_valid     = true;
        ctx->m_water_control_level     = cached.water_level;
        ctx->m_water_control_direction = cached.direction;
        ctx->m_water_control_speed     = cached.speed;
        return true;
    }

    cached             = water_seed_cache_t{ };
    cached.ready       = true;
    cached.serial      = serial;
    cached.tick        = newest->m_tick;
    cached.water_level = newest->m_water_level;
    cached.fingerprint = fingerprint;

    vec3 direction{ samples[ count - 2 ]->m_velocity };
    if ( direction.normalize( ) == 0.0f )
    {
        direction = vec3{ 0.0f, 0.0f, -1.0f };
    }
    const f32                  observed{ samples[ count - 2 ]->m_velocity.length( ) };
    const water_seed_control_t candidates[]{
        { { 0.0f, 0.0f, -1.0f }, 48.0f },
        { direction, math::clamp( observed, 0.0f, cap ) },
        { direction, cap },
    };

    constexpr i32 CANDIDATES{ static_cast< i32 >( sizeof( candidates ) / sizeof( candidates[ 0 ] ) ) };
    i32           selected_index{ -1 };
    f32           best_score{ FLT_MAX };
    for ( i32 candidate_index{ }; candidate_index < CANDIDATES; ++candidate_index )
    {
        const auto &candidate{ candidates[ candidate_index ] };
        bool        accepted{ true };
        f32         score{ };
        for ( i32 at{ 1 }; at < count - 1; ++at )
        {
            const f32 error{ replay_water_seed_interval( samples[ at - 1 ], samples[ at ],
                                                         candidate, nullptr ) };
            const i32 gap{ samples[ at ]->m_tick - samples[ at - 1 ]->m_tick };
            const f32 bound{ water_seed_candidate_bound( samples[ at - 1 ], gap, candidate ) };
            if ( error > bound )
            {
                accepted = false;
                break;
            }
            const f32 normalized{ bound > 0.0f ? error / bound : error };
            score += normalized * normalized;
        }
        if ( accepted && score < best_score )
        {
            best_score     = score;
            selected_index = candidate_index;
        }
    }
    if ( selected_index < 0 )
    {
        return false;
    }
    const water_seed_control_t selected{ candidates[ selected_index ] };

    vec3      best_terminal{ };
    const i32 holdout_gap{ samples[ count - 1 ]->m_tick - samples[ count - 2 ]->m_tick };
    const f32 holdout_error{ replay_water_seed_interval( samples[ count - 2 ], samples[ count - 1 ],
                                                         selected, &best_terminal ) };
    if ( holdout_error > water_seed_candidate_bound( samples[ count - 2 ], holdout_gap, selected ) )
    {
        return false;
    }

    cached.valid     = true;
    cached.terminal  = best_terminal;
    cached.direction = selected.direction;
    cached.speed     = selected.speed;

    ctx->m_pred_origin             = newest->m_origin;
    ctx->m_pred_velocity           = best_terminal;
    ctx->m_water_control_valid     = true;
    ctx->m_water_control_level     = newest->m_water_level;
    ctx->m_water_control_direction = selected.direction;
    ctx->m_water_control_speed     = selected.speed;
    return true;
}

constexpr i32 KEY_NONE{ 0 };
constexpr i32 KEY_HOLDER{ strafe_prediction::GROUND_KEYS };
constexpr i32 KEY_HYPOTHESES{ KEY_HOLDER + 1 };

constexpr f32 KEY_TRANSITION[ KEY_HOLDER ][ KEY_HOLDER ]{
    { 0.95776f, 0.00958f, 0.00053f, 0.01164f, 0.00011f, 0.00810f, 0.00011f, 0.01164f, 0.00053f },
    { 0.00318f, 0.97186f, 0.01151f, 0.00087f, 0.00006f, 0.00010f, 0.00006f, 0.00087f, 0.01151f },
    { 0.00039f, 0.04703f, 0.93307f, 0.01654f, 0.00003f, 0.00006f, 0.00007f, 0.00019f, 0.00262f },
    { 0.01471f, 0.00267f, 0.02576f, 0.93279f, 0.01200f, 0.00166f, 0.00010f, 0.01009f, 0.00022f },
    { 0.00100f, 0.00031f, 0.00000f, 0.04777f, 0.90723f, 0.03678f, 0.00673f, 0.00000f, 0.00017f },
    { 0.01889f, 0.00108f, 0.00028f, 0.00286f, 0.02274f, 0.92827f, 0.02274f, 0.00286f, 0.00028f },
    { 0.00100f, 0.00031f, 0.00017f, 0.00000f, 0.00673f, 0.03678f, 0.90723f, 0.04777f, 0.00000f },
    { 0.01471f, 0.00267f, 0.00022f, 0.01009f, 0.00010f, 0.00166f, 0.01200f, 0.93279f, 0.02576f },
    { 0.00039f, 0.04703f, 0.00262f, 0.00019f, 0.00007f, 0.00006f, 0.00003f, 0.01654f, 0.93307f },
};

constexpr f32 KEY_STAY[ KEY_HOLDER ][ 3 ]{
    { 0.9541f, 0.9690f, 0.9838f },
    { 0.9727f, 0.9731f, 0.9800f },
    { 0.9343f, 0.9328f, 0.9500f },
    { 0.9364f, 0.9279f, 0.9377f },
    { 0.9068f, 0.9210f, 0.9431f },
    { 0.9288f, 0.9368f, 0.9462f },
    { 0.9068f, 0.9210f, 0.9431f },
    { 0.9364f, 0.9279f, 0.9377f },
    { 0.9343f, 0.9328f, 0.9500f },
};

constexpr f32 KEY_FLIP_RATE{ 0.024f };

constexpr i32 KEY_WINDOW{ 24 };
constexpr i32 KEY_MAX_GAP{ 8 };
constexpr f32 KEY_SIGMA{ 0.06f };
constexpr f32 KEY_SWITCH{ 8.0f };
constexpr f32 KEY_MARGIN{ 4.0f };
constexpr f32 KEY_MISS{ 0.2f };
constexpr f32 KEY_GAIN{ 0.4f };

struct key_read_t
{
    vec3 m_velocity{ };
    vec3 m_runner_velocity{ };
    f32  m_margin{ };
    f32  m_miss{ };
    i32  m_key{ };
    i32  m_runner{ };
    f32  m_chance[ KEY_HOLDER ]{ };
    vec3 m_key_velocity[ KEY_HOLDER ]{ };
    i32  m_flips{ };
    i32  m_lateral{ };
    i32  m_held{ };
};

static ATLAS_FORCE_INLINE i32 key_side( i32 key )
{
    switch ( key )
    {
        case 2:
        case 3:
        case 4: return 1;
        case 6:
        case 7:
        case 8: return -1;
        default: return 0;
    }
}

static ATLAS_FORCE_INLINE f32 falloff( f32 x )
{
    if ( x > 20.0f )
    {
        return 0.0f;
    }

    const f32 y{ x / 64.0f };
    f32       value{ 1.0f - y * ( 1.0f - y * ( 0.5f - y * ( 1.0f / 6.0f - y / 24.0f ) ) ) };
    for ( i32 square{ }; square < 6; ++square )
    {
        value *= value;
    }
    return value;
}

static ATLAS_FORCE_INLINE bool confident( const key_read_t &keys )
{
    return keys.m_margin >= KEY_MARGIN && keys.m_miss <= KEY_MISS;
}

static ATLAS_FORCE_INLINE void key_step( vec3 &velocity, f32 view_yaw, i32 key, f32 wish_speed,
                                         const strafe_prediction::ground_physics &environment )
{
    if ( key == KEY_HOLDER )
    {
        return;
    }

    vec3 wish{ };
    if ( key != KEY_NONE )
    {
        const f32 radians{ DEG2RAD( view_yaw + 45.0f * static_cast< f32 >( key - 1 ) ) };
        wish = vec3{ math::cos( radians ), math::sin( radians ), 0.0f };
    }

    strafe_prediction::ground_step( velocity, wish, wish_speed, environment );
    strafe_prediction::ground_clamp_back_speed( velocity, view_yaw, environment.max_speed,
                                                g_tf2.tf_clamp_back_speed->GetFloat( ),
                                                g_tf2.tf_clamp_back_speed_min->GetFloat( ) );
}

static ATLAS_FORCE_INLINE strafe_prediction::ground_physics key_physics( const velocity_sample_t *sample )
{
    return { TICK_INTERVAL, g_tf2.sv_friction->GetFloat( ), g_tf2.sv_stopspeed->GetFloat( ), g_tf2.sv_accelerate->GetFloat( ),
             strafe_prediction::movement_max_speed( sample->m_maxspeed, false ), sample->m_surface_friction, false };
}

static bool read_keys( i32 index, const velocity_sample_t *newest, key_read_t &out )
{
    const velocity_sample_t *window[ KEY_WINDOW ]{ newest };
    const velocity_sample_t *takeoff{ };
    i32                      count{ 1 };
    for ( i32 age{ 1 }; age < g_player_data.velocity_samples( index ) && count < KEY_WINDOW; ++age )
    {
        const velocity_sample_t *newer{ window[ count - 1 ] };
        const velocity_sample_t *older{ g_player_data.velocity_sample( index, age ) };
        const i32                gap{ newer->m_tick - older->m_tick };
        if ( gap < 1 || gap > KEY_MAX_GAP || newer->m_gap != gap || older->m_swimming || older->m_charging ||
             older->m_lost_footing || older->m_transport_valid || older->m_ducked != newer->m_ducked )
        {
            break;
        }
        if ( !older->m_on_ground )
        {
            takeoff = older;
            break;
        }
        window[ count++ ] = older;
    }
    if ( count < 2 )
    {
        return false;
    }

    const velocity_sample_t *from{ takeoff ? takeoff : window[ count - 1 ] };
    const velocity_sample_t *to{ takeoff ? window[ count - 1 ] : window[ count - 2 ] };
    vec3                     start{ ( to->m_origin - from->m_origin ) / TICKS_TO_TIME( to->m_tick - from->m_tick ) };
    start.z = 0.0f;

    f32  cost[ KEY_HYPOTHESES ]{ };
    vec3 velocity[ KEY_HYPOTHESES ]{ };
    i32  side[ KEY_HYPOTHESES ]{ };
    i32  flips[ KEY_HYPOTHESES ]{ };
    i32  lateral[ KEY_HYPOTHESES ]{ };
    i32  held[ KEY_HYPOTHESES ]{ };
    f32  misses[ 4 ]{ };
    i32  intervals{ };
    i32  best{ };
    for ( i32 key{ }; key < KEY_HYPOTHESES; ++key )
    {
        velocity[ key ] = start;
        side[ key ]     = key_side( key );
    }

    for ( i32 at{ count - 2 }; at >= 0; --at )
    {
        const velocity_sample_t *older{ window[ at + 1 ] };
        const velocity_sample_t *newer{ window[ at ] };
        const i32                gap{ newer->m_tick - older->m_tick };
        const f32                turn{ math::normalize_yaw( newer->m_eye_yaw - older->m_eye_yaw ) };

        const strafe_prediction::ground_physics environment{ key_physics( newer ) };
        const f32                               wish_speed{ environment.max_speed * ( newer->m_ducked ? DUCK_SPEED_CROP : 1.0f ) };

        f32  next_cost[ KEY_HYPOTHESES ]{ };
        vec3 next_velocity[ KEY_HYPOTHESES ]{ };
        i32  next_side[ KEY_HYPOTHESES ]{ };
        i32  next_flips[ KEY_HYPOTHESES ]{ };
        i32  next_lateral[ KEY_HYPOTHESES ]{ };
        i32  next_held[ KEY_HYPOTHESES ]{ };
        f32  miss[ KEY_HYPOTHESES ]{ };
        for ( i32 key{ }; key < KEY_HYPOTHESES; ++key )
        {
            next_cost[ key ] = FLT_MAX;

            for ( i32 split{ key == best ? gap : 0 }; split <= gap; ++split )
            {
                const i32 source{ split == gap ? key : best };
                vec3      moved{ velocity[ source ] };
                vec3      position{ older->m_origin };
                for ( i32 tick{ 1 }; tick <= gap; ++tick )
                {
                    const f32 view{ older->m_eye_yaw + turn * static_cast< f32 >( tick ) / static_cast< f32 >( gap ) };
                    key_step( moved, view, tick <= split ? source : key, wish_speed, environment );
                    position += moved * TICK_INTERVAL;
                }

                const vec3 error{ newer->m_origin.x - position.x, newer->m_origin.y - position.y, 0.0f };
                const f32  total{ cost[ source ] + error.length2d_sqr( ) / ( KEY_SIGMA * KEY_SIGMA ) +
                                  ( source != key ? KEY_SWITCH : 0.0f ) };
                if ( total < next_cost[ key ] )
                {
                    const i32 lean{ key_side( key ) };
                    next_cost[ key ]     = total;
                    next_velocity[ key ] = moved + error * ( KEY_GAIN / TICKS_TO_TIME( gap ) );
                    miss[ key ]          = error.length2d( );
                    next_side[ key ]     = lean != 0 ? lean : side[ source ];
                    next_flips[ key ]    = flips[ source ] + ( lean != 0 && side[ source ] != 0 && lean != side[ source ] ? 1 : 0 );
                    next_lateral[ key ]  = lateral[ source ] + ( key_side( source ) != 0 ? split : 0 ) + ( lean != 0 ? gap - split : 0 );
                    next_held[ key ]     = source == key ? held[ key ] + gap : gap - split;
                }
            }
        }

        best = 0;
        for ( i32 key{ }; key < KEY_HYPOTHESES; ++key )
        {
            cost[ key ]     = next_cost[ key ];
            velocity[ key ] = next_velocity[ key ];
            side[ key ]     = next_side[ key ];
            flips[ key ]    = next_flips[ key ];
            lateral[ key ]  = next_lateral[ key ];
            held[ key ]     = next_held[ key ];
            if ( cost[ key ] < cost[ best ] )
            {
                best = key;
            }
        }
        misses[ intervals++ % 4 ] = miss[ best ];
    }

    i32 key{ KEY_NONE };
    for ( i32 other{ 1 }; other < KEY_HOLDER; ++other )
    {
        if ( cost[ other ] < cost[ key ] )
        {
            key = other;
        }
    }
    if ( cost[ KEY_HOLDER ] + KEY_MARGIN < cost[ key ] )
    {
        return false;
    }

    i32 runner{ key == KEY_NONE ? 1 : KEY_NONE };
    for ( i32 other{ }; other < KEY_HYPOTHESES; ++other )
    {
        if ( other != key && cost[ other ] < cost[ runner ] )
        {
            runner = other;
        }
    }

    const i32 kept{ math::smaller( intervals, 4 ) };
    f32       miss_sum{ };
    for ( i32 at{ }; at < kept; ++at )
    {
        miss_sum += misses[ at ];
    }

    out.m_velocity        = velocity[ key ];
    out.m_runner_velocity = velocity[ runner ];
    out.m_margin          = cost[ runner ] - cost[ key ];
    out.m_miss            = miss_sum / static_cast< f32 >( kept );
    out.m_key             = key;
    out.m_runner          = runner;
    out.m_flips           = flips[ key ];
    out.m_lateral         = lateral[ key ];
    out.m_held            = held[ key ];

    f32 total{ };
    for ( i32 other{ }; other < KEY_HOLDER; ++other )
    {
        out.m_chance[ other ]       = falloff( ( cost[ other ] - cost[ key ] ) * 0.5f );
        out.m_key_velocity[ other ] = velocity[ other ];
        total += out.m_chance[ other ];
    }
    for ( f32 &chance : out.m_chance )
    {
        chance /= total;
    }
    return true;
}

static i32 key_agreement( const key_read_t &keys, const velocity_sample_t *newest, f32 radius )
{
    const strafe_prediction::ground_physics environment{ key_physics( newest ) };
    const f32                               wish_speed{ environment.max_speed * ( newest->m_ducked ? DUCK_SPEED_CROP : 1.0f ) };

    vec3 read{ keys.m_velocity };
    vec3 rival{ keys.m_runner_velocity };
    vec3 apart{ };
    for ( i32 tick{ 1 }; tick < MAX_WALK_TICKS; ++tick )
    {
        key_step( read, newest->m_eye_yaw, keys.m_key, wish_speed, environment );
        key_step( rival, newest->m_eye_yaw, keys.m_runner, wish_speed, environment );
        apart += ( read - rival ) * TICK_INTERVAL;
        if ( apart.length2d_sqr( ) > radius * radius )
        {
            return tick - 1;
        }
    }

    return MAX_WALK_TICKS - 1;
}

constexpr f32 EYE_FADE{ 0.79f };

static f32 eye_turn( i32 index, const velocity_sample_t *newest )
{
    constexpr f32 MAX_EYE_RATE{ 12.0f };
    constexpr f32 EYE_KEPT{ 1.09f };

    for ( i32 age{ 1 }; age < g_player_data.velocity_samples( index ); ++age )
    {
        const velocity_sample_t *older{ g_player_data.velocity_sample( index, age ) };
        const i32                span{ newest->m_tick - older->m_tick };
        if ( span >= 2 )
        {
            const f32 rate{ math::normalize_yaw( newest->m_eye_yaw - older->m_eye_yaw ) / static_cast< f32 >( span ) };
            return math::clamp( rate, -MAX_EYE_RATE, MAX_EYE_RATE ) * EYE_KEPT;
        }
    }

    return 0.0f;
}

static ATLAS_FORCE_INLINE void hold_heading( move_sim_ctx_t *ctx )
{
    const f32 speed{ ctx->m_pred_velocity.length2d( ) };
    ctx->m_wish_valid     = speed > 0.0f;
    ctx->m_wish_yaw       = heading( ctx->m_pred_velocity.x, ctx->m_pred_velocity.y );
    ctx->m_wishspeed      = math::smaller( speed, strafe_prediction::movement_max_speed( ctx->m_max_speed, ctx->m_pred_lost_footing ) );
    ctx->m_walk_wishspeed = ctx->m_wishspeed;
}

constexpr i32 KEY_KEPT{ KEY_HOLDER };

static void walk_keys( move_sim_ctx_t *ctx )
{
    const strafe_prediction::ground_physics environment{
        TICK_INTERVAL, g_tf2.sv_friction->GetFloat( ), g_tf2.sv_stopspeed->GetFloat( ), g_tf2.sv_accelerate->GetFloat( ),
        strafe_prediction::movement_max_speed( ctx->m_max_speed, ctx->m_pred_lost_footing ), ctx->m_pred_surface_friction,
        ctx->m_pred_lost_footing
    };
    const f32 wish_speed{ environment.max_speed * ( ( ctx->m_pred_flags & FL_DUCKING ) ? DUCK_SPEED_CROP : 1.0f ) };

    if ( ctx->m_walk_aim == walk_aim::point )
    {
        i32 likely{ };
        for ( i32 key{ 1 }; key < KEY_HOLDER; ++key )
        {
            const f32 chance{ ctx->m_walk_keys[ key ] + ( key == ctx->m_walk_kept_key ? ctx->m_walk_keys[ KEY_KEPT ] : 0.0f ) };
            const f32 best{ ctx->m_walk_keys[ likely ] + ( likely == ctx->m_walk_kept_key ? ctx->m_walk_keys[ KEY_KEPT ] : 0.0f ) };
            if ( chance > best )
            {
                likely = key;
            }
        }

        ctx->m_wish_valid     = likely != KEY_NONE;
        ctx->m_wish_yaw       = ctx->m_walk_view_yaw + 45.0f * static_cast< f32 >( likely - 1 );
        ctx->m_walk_wishspeed = ctx->m_wish_valid ? wish_speed : 0.0f;
        if ( ctx->m_wish_valid )
        {
            const f32 radians{ DEG2RAD( ctx->m_wish_yaw ) };
            strafe_prediction::ground_accelerate( ctx->m_pred_velocity, vec3{ math::cos( radians ), math::sin( radians ), 0.0f },
                                                  wish_speed, environment );
        }
        return;
    }

    const vec3 moved{ ctx->m_pred_origin - ctx->m_walk_anchor };
    ctx->m_walk_anchor = ctx->m_pred_origin;
    for ( i32 walk{ }; walk < strafe_prediction::GROUND_WALKS; ++walk )
    {
        const i32 key{ walk == KEY_KEPT ? ctx->m_walk_kept_key : walk };
        key_step( ctx->m_walk_key_velocity[ walk ], ctx->m_walk_view_yaw, key, wish_speed, environment );
        ctx->m_walk_key_offset[ walk ] += ctx->m_walk_key_velocity[ walk ] * TICK_INTERVAL - moved;
        ctx->m_walk_key_offset[ walk ].z = 0.0f;
    }

    vec3 target{ };
    if ( ctx->m_walk_aim == walk_aim::blast || ctx->m_walk_aim == walk_aim::trap )
    {
        for ( i32 walk{ }; walk < strafe_prediction::GROUND_WALKS; ++walk )
        {
            target += ctx->m_walk_key_offset[ walk ] * ctx->m_walk_keys[ walk ];
        }
    } else
    {
        const f32 radius{ ( ctx->m_pred_maxs.x - ctx->m_pred_mins.x ) * 0.5f };
        f32       densest{ };
        for ( i32 walk{ }; walk < strafe_prediction::GROUND_WALKS; ++walk )
        {
            f32  mass{ };
            vec3 center{ };
            for ( i32 other{ }; other < strafe_prediction::GROUND_WALKS; ++other )
            {
                if ( ( ctx->m_walk_key_offset[ other ] - ctx->m_walk_key_offset[ walk ] ).length2d_sqr( ) <= radius * radius )
                {
                    mass += ctx->m_walk_keys[ other ];
                    center += ctx->m_walk_key_offset[ other ] * ctx->m_walk_keys[ other ];
                }
            }
            if ( mass > densest )
            {
                densest = mass;
                target  = center / mass;
            }
        }
    }

    ctx->m_pred_velocity.x = target.x / TICK_INTERVAL;
    ctx->m_pred_velocity.y = target.y / TICK_INTERVAL;
    hold_heading( ctx );
}

static void mix_keys( move_sim_ctx_t *ctx )
{
    f32  chance[ strafe_prediction::GROUND_WALKS ]{ };
    vec3 velocity[ strafe_prediction::GROUND_WALKS ]{ };
    vec3 offset[ strafe_prediction::GROUND_WALKS ]{ };
    for ( i32 from{ }; from < strafe_prediction::GROUND_WALKS; ++from )
    {
        const i32 key{ from == KEY_KEPT ? ctx->m_walk_kept_key : from };
        const i32 lean{ key_side( key ) };

        f32 across{ };
        for ( i32 to{ }; to < KEY_HOLDER; ++to )
        {
            across += lean != 0 && key_side( to ) == -lean ? KEY_TRANSITION[ key ][ to ] : 0.0f;
        }
        const f32 extra{ ( ctx->m_walk_flip_scale - 1.0f ) * across };
        const f32 hold{ math::bigger( KEY_TRANSITION[ key ][ key ] - extra, 0.0f ) };

        const i32 band{ ( ctx->m_walk_kept_held >= 8 ? 1 : 0 ) + ( ctx->m_walk_kept_held >= 24 ? 1 : 0 ) };
        const f32 stay{ from == KEY_KEPT ? math::bigger( KEY_STAY[ key ][ band ] - extra, 0.0f ) : hold };
        const f32 leave{ hold < 1.0f ? ( 1.0f - stay ) / ( 1.0f - hold ) : 0.0f };

        for ( i32 to{ }; to < KEY_HOLDER; ++to )
        {
            f32 rate{ KEY_TRANSITION[ key ][ to ] };
            if ( to == key )
            {
                rate = stay;
            } else
            {
                rate *= ( lean != 0 && key_side( to ) == -lean ? ctx->m_walk_flip_scale : 1.0f ) * leave;
            }

            const i32 into{ from == KEY_KEPT && to == key ? KEY_KEPT : to };
            const f32 flow{ ctx->m_walk_keys[ from ] * rate };
            chance[ into ] += flow;
            velocity[ into ] += ctx->m_walk_key_velocity[ from ] * flow;
            offset[ into ] += ctx->m_walk_key_offset[ from ] * flow;
        }
    }

    for ( i32 walk{ }; walk < strafe_prediction::GROUND_WALKS; ++walk )
    {
        ctx->m_walk_keys[ walk ] = chance[ walk ];
        if ( chance[ walk ] > 0.0f )
        {
            ctx->m_walk_key_velocity[ walk ] = velocity[ walk ] / chance[ walk ];
            ctx->m_walk_key_offset[ walk ]   = offset[ walk ] / chance[ walk ];
        }
    }
    ++ctx->m_walk_kept_held;
}

void strafe_prediction::initialize( move_sim_ctx_t *ctx )
{
    C_TFPlayer              *player{ ctx->m_player_to_predict };
    const velocity_sample_t *newest{ g_player_data.velocity_sample( player->entindex( ), 0 ) };
    ctx->m_charging = player->InCond( TF_COND_SHIELD_CHARGE ) && player->m_nWaterLevel( ) < 2;
    const f32 charge_speed{ g_tf2.tf_max_charge_speed->GetFloat( ) };
    f32       cap{ 30.0f };
    if ( player->InCond( TF_COND_PARACHUTE_ACTIVE ) )
    {
        cap *= g_tf2.tf_parachute_aircontrol->GetFloat( );
    }
    f32 modifier{ g_cl.attrib_hook( player, 1.0f, ATLAS_XORSTR_STATIC( "mod_air_control" ), true ) };
    ctx->m_ordinary_air_control = modifier == 1.0f;
    if ( player->InCond( TF_COND_BLASTJUMPING ) )
    {
        modifier                    = g_cl.attrib_hook( player, modifier, ATLAS_XORSTR_STATIC( "mod_air_control_blast_jump" ), true );
        ctx->m_ordinary_air_control = ctx->m_ordinary_air_control && modifier == 1.0f;
    }
    if ( player->InCond( TF_COND_ROCKETPACK ) )
    {
        cap *= 0.5f;
    }
    ctx->m_air_speed_cap = cap * modifier;
    if ( ctx->m_charging )
    {
        const f32 duration{ g_cl.attrib_hook( player, g_tf2.tf_demoman_charge_drain_time->GetFloat( ),
                                              ATLAS_XORSTR_STATIC( "mod_charge_time" ), true ) };
        Assert( duration > 0.0f );
        const f32 drain{ ( 100.0f / duration ) * TICK_INTERVAL };
        const f32 ticks{ math::bigger( player->m_flChargeMeter( ), 0.0f ) / drain };
        ctx->m_charge_ticks = static_cast< i32 >( ticks );
        if ( static_cast< f32 >( ctx->m_charge_ticks ) < ticks )
        {
            ++ctx->m_charge_ticks;
        }
        ctx->m_charge_exit_speed = player->TeamFortress_CalculateMaxSpeed( true );
        ctx->m_charge_exit_cap   = ctx->m_air_speed_cap;
        ctx->m_air_speed_cap     = charge_speed;
    }

    if ( player->m_nWaterLevel( ) == 3 && seed_water_controller( ctx, newest ) )
    {
        return;
    }

    if ( newest && newest->m_on_ground && newest->m_ground_index > 0 && !newest->m_transport_valid )
    {
        ctx->m_prediction_broken = true;
        return;
    }

    if ( !( ctx->m_pred_flags & FL_ONGROUND ) && ctx->m_player_to_predict->m_nWaterLevel( ) < 2 )
    {
        ctx->m_air_model = true;
        seed_air( ctx );
        bool full_stun{ };
        if ( !ctx->m_charging && player->InCond( TF_COND_STUNNED ) )
        {
            const i32 flags{ player->m_iStunFlags( ) };
            full_stun = ( flags & TF_STUN_CONTROLS ) || ( ( flags & TF_STUN_MOVEMENT ) && player->m_iMovementStunAmount( ) == 255 );
        }
        if ( ctx->m_charging || full_stun )
        {
            const bool moving{ ctx->m_charging && ctx->m_pred_velocity.length2d_sqr( ) > 1.0f };
            const f32  wish{ moving ? charge_speed : 0.0f };
            ctx->m_air_control    = { moving ? model::locked : model::coast, 0.0f, 0.0f, 0.0f, wish, ctx->m_air_seed_tick };
            ctx->m_wishspeed      = wish;
            ctx->m_walk_wishspeed = wish;
            ctx->m_wish_delta     = 0.0f;
            ctx->m_wish_yaw       = heading( ctx->m_pred_velocity.x, ctx->m_pred_velocity.y );
            ctx->m_wish_valid     = moving;
        }
        return;
    }
    if ( ctx->m_charging )
    {
        if ( !ctx->m_air_model )
        {
            if ( newest && newest->m_on_ground && newest->m_gap >= 1 && newest->m_gap <= 8 )
            {
                ctx->m_pred_origin   = newest->m_origin;
                ctx->m_pred_velocity = newest->m_velocity;
            }
            ctx->m_pred_velocity.z = 0.f;
        }

        ctx->m_wish_yaw = newest && newest->m_charging ? newest->m_eye_yaw : player->m_angEyeAngles( ).y;
        const f32 movement_maxspeed{ strafe_prediction::movement_max_speed(
            ctx->m_max_speed, ctx->m_pred_lost_footing, true, charge_speed ) };
        ctx->m_wishspeed      = movement_maxspeed * ( !ctx->m_air_model && player->IsDucked( ) ? DUCK_SPEED_CROP : 1.f );
        ctx->m_walk_wishspeed = ctx->m_wishspeed;
        ctx->m_wish_valid     = ctx->m_wishspeed > 0.f;
        ctx->m_walk_input     = true;
        ctx->m_walk_ground    = !ctx->m_air_model;
        ctx->m_air_control    = { model::world, ctx->m_wish_yaw, 0.f, 0.f, ctx->m_wishspeed, ctx->m_air_seed_tick };
        return;
    }

    i32 seed_span{ 1 }; // ticks the seed velocity was measured over; the de-lag scales with it
    ctx->m_pred_velocity = ctx->m_player_to_predict->m_vecVelocity( );

    if ( newest && ( newest->m_on_ground || !newest->m_velocity.is_zero( ) ) &&
         newest->m_gap >= 1 && newest->m_gap <= 8 )
    {

        // NOTE: THIS IS **HIGHLY** EXPERIMENTAL.
        // velocity fix
        ctx->m_pred_velocity = newest->m_velocity;
        ctx->m_pred_origin   = newest->m_origin;
        constexpr i32 MAX_BASELINE_SPAN{ 4 };
        if ( !newest->m_transport_valid )
        {
            const velocity_sample_t *far_end{ nullptr };

            for ( i32 age{ 2 }; age <= MAX_BASELINE_SPAN; ++age )
            {
                const velocity_sample_t *candidate{
                    g_player_data.velocity_sample( ctx->m_player_to_predict->entindex( ), age )
                };

                if ( !candidate || candidate->m_swimming ||
                     candidate->m_charging != newest->m_charging ||
                     candidate->m_on_ground != newest->m_on_ground )
                {
                    break;
                }

                if ( newest->m_on_ground )
                {
                    bool                     compatible{ true };
                    const velocity_sample_t *link_newer{ newest };
                    for ( i32 link{ 1 }; link <= age; ++link )
                    {
                        const velocity_sample_t *link_older{
                            g_player_data.velocity_sample( ctx->m_player_to_predict->entindex( ), link )
                        };
                        if ( !same_ground_regime( link_newer, link_older ) )
                        {
                            compatible = false;
                            break;
                        }
                        link_newer = link_older;
                    }
                    if ( !compatible )
                    {
                        break;
                    }
                }

                const i32 span{ newest->m_tick - candidate->m_tick };

                if ( span <= 0 || span > MAX_BASELINE_SPAN )
                {
                    break;
                }

                const f32  period{ TICKS_TO_TIME( span ) };
                const vec3 hyp{ ( newest->m_origin - candidate->m_origin ) /
                                period };

                bool consistent{ true };

                for ( i32 mid{ 1 }; mid < age && consistent; ++mid )
                {
                    const velocity_sample_t *between{
                        g_player_data.velocity_sample( ctx->m_player_to_predict->entindex( ), mid )
                    };

                    if ( !between )
                    {
                        consistent = false;
                        break;
                    }

                    const f32 tau{ TICKS_TO_TIME( newest->m_tick - between->m_tick ) };
                    const f32 flat_err{
                        vec3{ newest->m_origin.x - hyp.x * tau -
                                  between->m_origin.x,
                              newest->m_origin.y - hyp.y * tau -
                                  between->m_origin.y,
                              0.0f }
                            .length2d( )
                    };
                    const f32 z_err{ math::abs( newest->m_origin.z - hyp.z * tau -
                                                between->m_origin.z ) };

                    constexpr f32 MAX_CHORD_BIAS{ 0.21f }; // radians, about.. ~12dg
                    const f32     chord{
                        ( newest->m_origin - candidate->m_origin ).length2d( )
                    };
                    const f32 slack{ math::bigger( 0.4f, chord * MAX_CHORD_BIAS * 0.25f ) };
                    consistent = flat_err <= ( newest->m_on_ground ? 0.1767767f : slack ) && z_err <= 12.0f;
                }

                if ( !consistent )
                {
                    break;
                }

                far_end   = candidate;
                seed_span = span;
            }

            if ( far_end )
            {
                ctx->m_pred_velocity = ( newest->m_origin - far_end->m_origin ) /
                                       TICKS_TO_TIME( seed_span );
            }

            seed_span = math::bigger( seed_span, newest->m_gap );

            // g * span / 2 apart
            if ( !( ctx->m_pred_flags & FL_ONGROUND ) )
            {
                ctx->m_pred_velocity.z -= g_tf2.sv_gravity->GetFloat( ) * 0.5f *
                                          TICKS_TO_TIME( seed_span );
            }
        }
    }

    if ( ctx->m_pred_flags & FL_ONGROUND )
    {
        ctx->m_pred_velocity.z = 0.0f;
    }

    {
        const i32 index{ ctx->m_player_to_predict->entindex( ) };
        const i32 samples{ g_player_data.velocity_samples( index ) };

        const velocity_sample_t *newest_sample{ g_player_data.velocity_sample( index, 0 ) };
        const i32                newest_tick{ newest_sample ? newest_sample->m_tick : 0 };

        i32       stays[ 8 ]{ };
        i32       stay_count{ };
        i32       run{ };
        i32       run_start{ -1 };
        i32       current_stay{ -1 };
        i32       last_jump_tick{ -1 };
        const f32 entity_gravity{ player->m_flGravity( ) != 0.0f ? player->m_flGravity( ) : 1.0f };

        for ( i32 age{ 0 }; age < samples && stay_count < 8; ++age )
        {
            const velocity_sample_t *sample{ g_player_data.velocity_sample( index, age ) };

            if ( !sample || sample->m_charging || sample->m_swimming )
            {
                break;
            }

            if ( sample->m_on_ground )
            {
                if ( run == 0 )
                {
                    run_start = age;
                }

                run += math::bigger( sample->m_gap, 1 );
                continue;
            }

            if ( run > 0 )
            {
                if ( run_start == 0 )
                {
                    current_stay = run; // the seed's own stay, still open on the newer side
                } else if ( observed_ground_jump( g_player_data.velocity_sample( index, run_start ),
                                                  g_player_data.velocity_sample( index, run_start - 1 ),
                                                  entity_gravity * g_tf2.sv_gravity->GetFloat( ) ) )
                {
                    stays[ stay_count++ ] = run;
                    last_jump_tick        = math::bigger( last_jump_tick,
                                                          g_player_data.velocity_sample( index, run_start - 1 )->m_tick );
                }
            }

            run = 0;
        }

        if ( run > 0 && run_start == 0 )
        {
            current_stay = run;
        }

        if ( stay_count >= 1 )
        {
            // median of a handful; insertion is fine
            for ( i32 at{ 1 }; at < stay_count; ++at )
            {
                const i32 value{ stays[ at ] };
                i32       put{ at - 1 };

                while ( put >= 0 && stays[ put ] > value )
                {
                    stays[ put + 1 ] = stays[ put ];
                    --put;
                }

                stays[ put + 1 ] = value;
            }

            const i32     hop{ stays[ stay_count / 2 ] };
            constexpr i32 needed{ 2 }; // one departure does not establish a repeated jump intention

            if ( hop <= 12 && stay_count >= needed &&
                 ( current_stay < 0 || current_stay <= math::bigger( hop + 4, 2 * hop ) ) )
            {
                ctx->m_hopping          = true;
                ctx->m_hop_ground_ticks = hop;
                ctx->m_ground_ticks     = math::bigger( current_stay, 0 );

                g_player_data.remember_hop( index, hop, last_jump_tick ); // I KNOW WHERE YOU LIVE
            }
        }

        if ( !ctx->m_hopping )
        {
            constexpr i32 HOP_MEMORY{ 130 }; // ticks

            i32 hop{ };
            i32 measured_tick{ };

            if ( g_player_data.recall_hop( index, hop, measured_tick ) &&
                 newest_tick >= measured_tick && newest_tick - measured_tick <= HOP_MEMORY &&
                 ( current_stay < 0 || current_stay <= math::bigger( hop + 4, 2 * hop ) ) )
            {
                ctx->m_hopping          = true;
                ctx->m_hop_ground_ticks = hop;
                ctx->m_ground_ticks     = math::bigger( current_stay, 0 );
            }
        }
    }

    ctx->m_walk_ground = newest && newest->m_on_ground && !ctx->m_hopping;
    hold_heading( ctx );

    key_read_t keys{ };
    if ( !ctx->m_walk_ground || newest->m_lost_footing || newest->m_transport_valid ||
         !read_keys( player->entindex( ), newest, keys ) )
    {
        return;
    }

    ctx->m_pred_velocity          = keys.m_velocity;
    ctx->m_walk_input             = true;
    ctx->m_ground_agreement_ticks = key_agreement( keys, newest, ( ctx->m_pred_maxs.x - ctx->m_pred_mins.x ) * 0.5f );

    if ( keys.m_key == KEY_NONE && keys.m_velocity.is_zero( 0.0f ) )
    {
        hold_heading( ctx );
        return;
    }

    ctx->m_walk_keyed             = true;
    ctx->m_walk_unread            = !confident( keys );
    ctx->m_walk_turn              = eye_turn( player->entindex( ), newest );
    ctx->m_walk_view_yaw          = newest->m_eye_yaw;
    ctx->m_walk_anchor            = ctx->m_pred_origin;
    ctx->m_walk_kept_key          = keys.m_key;
    ctx->m_walk_flip_scale        = static_cast< f32 >( keys.m_flips + 1 ) / ( static_cast< f32 >( keys.m_lateral ) * KEY_FLIP_RATE + 1.0f );
    ctx->m_walk_kept_held         = keys.m_held;
    for ( i32 key{ }; key < KEY_HOLDER; ++key )
    {
        ctx->m_walk_keys[ key ]         = keys.m_chance[ key ];
        ctx->m_walk_key_velocity[ key ] = keys.m_key_velocity[ key ];
    }

    ctx->m_walk_keys[ KEY_KEPT ]         = ctx->m_walk_keys[ keys.m_key ];
    ctx->m_walk_key_velocity[ KEY_KEPT ] = keys.m_velocity;
    ctx->m_walk_keys[ keys.m_key ]       = 0.0f;
    hold_heading( ctx );
}

void strafe_prediction::begin_tick( move_sim_ctx_t *ctx )
{
    if ( ctx->m_charging && static_cast< i32 >( ctx->m_pred_ticks ) + 1 >= ctx->m_charge_ticks )
    {
        ctx->m_charging       = false;
        ctx->m_max_speed      = ctx->m_charge_exit_speed;
        ctx->m_air_speed_cap  = ctx->m_charge_exit_cap;
        ctx->m_air_control    = { model::coast, 0.0f, 0.0f, 0.0f, 0.0f, ctx->m_air_seed_tick };
        ctx->m_wishspeed      = 0.0f;
        ctx->m_walk_wishspeed = 0.0f;
        ctx->m_wish_valid     = false;
        ctx->m_walk_input     = false;
    }
    if ( ( ctx->m_pred_flags & FL_ONGROUND ) && ctx->m_walk_relative && ctx->m_wish_valid )
    {
        ctx->m_wish_yaw = heading( ctx->m_pred_velocity.x, ctx->m_pred_velocity.y ) + ctx->m_wish_delta;
    }

    if ( ctx->m_hopping && ( ctx->m_pred_flags & FL_ONGROUND ) &&
         !( ctx->m_pred_flags & FL_DUCKING ) && ctx->m_water_level <= 1 )
    {
        if ( ctx->m_ground_ticks >= ctx->m_hop_ground_ticks )
        {
            const f32 entity_gravity{ ctx->m_player_to_predict->m_flGravity( ) != 0.0f
                                          ? ctx->m_player_to_predict->m_flGravity( )
                                          : 1.0f };
            ctx->m_pred_velocity.z -= entity_gravity * g_tf2.sv_gravity->GetFloat( ) *
                                      0.5f * TICK_INTERVAL;
            const f32 max_bhop_speed{ ctx->m_max_speed * 1.2f };
            if ( max_bhop_speed > 0.0f && ctx->m_pred_velocity.length( ) > max_bhop_speed )
            {
                ctx->m_pred_velocity *= max_bhop_speed / ctx->m_pred_velocity.length( );
            }

            ctx->m_pred_velocity.z += JUMP_VELOCITY;
            ctx->m_pred_flags &= ~FL_ONGROUND;
            ctx->m_ground_ticks = 0;
            on_left_ground( ctx, false );
        } else
        {
            ++ctx->m_ground_ticks;
        }
    }
}

void strafe_prediction::on_left_ground( move_sim_ctx_t *ctx, bool after_move )
{
    Assert( ctx != nullptr );
    Assert( !ctx->m_air_model );
    if ( ctx->m_ground_agreement_ticks >= 0 )
    {
        ctx->m_ground_agreement_ticks = math::smaller( ctx->m_ground_agreement_ticks,
                                                       static_cast< i32 >( ctx->m_pred_ticks ) );
    }

    ctx->m_air_seed_tick = TIME_TO_TICKS( ctx->m_player_to_predict->m_flSimulationTime( ) );
    const i32 epoch{ ctx->m_air_seed_tick + static_cast< i32 >( ctx->m_pred_ticks ) + ( after_move ? 1 : 0 ) };
    if ( !ctx->m_wish_valid )
    {
        ctx->m_wish_yaw = heading( ctx->m_pred_velocity.x, ctx->m_pred_velocity.y );
    }
    const model kind{ ctx->m_walk_wishspeed <= 0.0f ? model::coast : ctx->m_walk_relative                        ? model::locked
                                                                 : ctx->m_walk_input && ctx->m_walk_turn != 0.0f ? model::sweep
                                                                                                                 : model::world };
    const f32   crop{ ( ctx->m_pred_flags & FL_DUCKING ) ? DUCK_SPEED_CROP : 1.0f };
    const f32   movement_maxspeed{ strafe_prediction::movement_max_speed(
        ctx->m_max_speed, ctx->m_pred_lost_footing, ctx->m_charging,
        g_tf2.tf_max_charge_speed->GetFloat( ) ) };
    ctx->m_wishspeed      = math::smaller( ctx->m_walk_wishspeed / crop, movement_maxspeed );
    ctx->m_air_control    = { kind, ctx->m_walk_relative ? ctx->m_wish_delta : ctx->m_wish_yaw,
                              kind == model::sweep ? ctx->m_walk_turn : 0.0f, 0.05f,
                              ctx->m_wishspeed, epoch };
    ctx->m_air_model      = true;
    ctx->m_air_turn_fades = false;
}

void strafe_prediction::on_landed( move_sim_ctx_t *ctx )
{
    Assert( ctx != nullptr );

    if ( !ctx->m_air_model )
    {
        return;
    }

    const bool fitted_in_air{ !ctx->m_walk_input && !ctx->m_charging && ctx->m_air_control.kind != model::locked };
    ctx->m_walk_input    = true;
    ctx->m_walk_relative = false;
    ctx->m_walk_ground   = false;
    ctx->m_walk_turn     = 0.0f;
    const f32 movement_maxspeed{ strafe_prediction::movement_max_speed(
        ctx->m_max_speed, ctx->m_pred_lost_footing, ctx->m_charging,
        g_tf2.tf_max_charge_speed->GetFloat( ) ) };
    ctx->m_wishspeed      = math::smaller( ctx->m_air_control.speed, movement_maxspeed );
    ctx->m_walk_wishspeed = ctx->m_wishspeed * ( ( ctx->m_pred_flags & FL_DUCKING ) ? DUCK_SPEED_CROP : 1.0f );

    switch ( fitted_in_air ? model::coast : ctx->m_air_control.kind )
    {
        case model::coast:
        {
            const f32 landing_speed{ ctx->m_pred_velocity.length2d( ) };
            ctx->m_wish_yaw       = RAD2DEG( math::atan2( ctx->m_pred_velocity.y, ctx->m_pred_velocity.x ) );
            ctx->m_wishspeed      = math::smaller( landing_speed, movement_maxspeed );
            ctx->m_walk_wishspeed = ctx->m_wishspeed * ( ( ctx->m_pred_flags & FL_DUCKING ) ? DUCK_SPEED_CROP : 1.0f );
            ctx->m_wish_valid     = ctx->m_wishspeed > 0.0f;
            break;
        }
        case model::locked:
        {
            ctx->m_walk_relative = true;
            ctx->m_wish_delta    = ctx->m_air_control.yaw;
            ctx->m_wish_valid    = ctx->m_wishspeed > 0.0f;
            break;
        }
        case model::world:
        case model::sweep:
        {
            ctx->m_walk_turn = ctx->m_air_control.rate;
            ctx->m_wish_yaw += ctx->m_walk_turn;
            ctx->m_wish_valid = ctx->m_wishspeed > 0.0f;
            break;
        }
        case model::phase_left:
        case model::phase_right:
        {
            ctx->m_wish_yaw   = ctx->m_air_control.yaw;
            ctx->m_wish_valid = ctx->m_wishspeed > 0.0f;
            break;
        }
        case model::feedback:
        case model::retrack:
        case model::retrack_drift:
        {
            ctx->m_wish_valid = ctx->m_wishspeed > 0.0f;
            break;
        }
        default: break;
    }

    ctx->m_air_model = false;
}

static ATLAS_FORCE_INLINE vec3 wish_direction( const move_sim_ctx_t *ctx )
{
    if ( !ctx->m_wish_valid )
    {
        vec3 heading{ ctx->m_pred_velocity.x, ctx->m_pred_velocity.y, 0.0f };

        return heading.normalize( ) > 0.0f ? heading : vec3{ };
    }

    vec3 forward{ };
    math::angle_vectors( vec3{ 0.0f, ctx->m_wish_yaw, 0.0f }, &forward );
    forward.z = 0.0f;
    return forward;
}

// @ CGameMovement::Accelerate
static void apply_ground_acceleration( move_sim_ctx_t *ctx )
{
    Assert( ctx != nullptr );

    if ( ctx->m_walk_wishspeed <= 0.0f )
    {
        return;
    }

    const f32 maximum{ strafe_prediction::movement_max_speed(
        ctx->m_max_speed, ctx->m_pred_lost_footing, ctx->m_charging,
        g_tf2.tf_max_charge_speed->GetFloat( ) ) };
    const f32 wish_cap{ maximum * ( ( ctx->m_pred_flags & FL_DUCKING ) ? DUCK_SPEED_CROP : 1.0f ) };
    const f32 wish_speed{ math::smaller( ctx->m_walk_wishspeed, wish_cap ) };
    strafe_prediction::ground_accelerate(
        ctx->m_pred_velocity, wish_direction( ctx ), wish_speed,
        { TICK_INTERVAL, g_tf2.sv_friction->GetFloat( ), g_tf2.sv_stopspeed->GetFloat( ),
          g_tf2.sv_accelerate->GetFloat( ), ctx->m_max_speed, ctx->m_pred_surface_friction,
          ctx->m_pred_lost_footing } );
}

// @ CGameMovement::AirAccelerate
void strafe_prediction::apply_air_input( move_sim_ctx_t *ctx )
{
    Assert( ctx != nullptr );

    if ( ctx->m_air_model )
    {
        const strafe_prediction::physics environment{ air_environment( ctx ) };
        strafe_prediction::state         state{ ctx->m_pred_origin, ctx->m_pred_velocity, ctx->m_pred_surface_friction,
                                                ctx->m_air_seed_tick + static_cast< i32 >( ctx->m_pred_ticks ) };
        if ( ctx->m_air_control.kind == strafe_prediction::model::feedback )
        {
            f64 wish{ };
            air_inference::wish_direction( air_inference::widen( state ), air_inference::widen( ctx->m_air_control ),
                                           air_inference::widen( environment ), wish );
            ctx->m_air_control.speed = ctx->m_wishspeed = static_cast< f32 >( wish );
        }

        if ( ctx->m_air_goal_side != 0 && ctx->m_air_control.kind == strafe_prediction::model::retrack )
        {
            const f32 error{ wrap( ctx->m_air_control.yaw - heading( state.velocity.x, state.velocity.y ) ) };
            if ( ctx->m_air_goal_held > 0 || error * static_cast< f32 >( ctx->m_air_goal_side ) <= 0.0f )
            {
                ++ctx->m_air_goal_held;
            }
            if ( ctx->m_air_goal_held > GOAL_HOLD_TICKS )
            {
                ctx->m_air_control.kind = strafe_prediction::model::locked;
                ctx->m_air_control.yaw  = 90.0f * static_cast< f32 >( ctx->m_air_goal_side );
                ctx->m_air_goal_side    = 0;
            }
        }
        const strafe_prediction::model kind{ ctx->m_air_control.kind };
        const bool                     world_goal{ kind == strafe_prediction::model::sweep || kind == strafe_prediction::model::retrack_drift };
        const bool                     fades{ kind == strafe_prediction::model::feedback ||
                                              ( ctx->m_air_turn_fades && ( world_goal || kind == strafe_prediction::model::locked ) ) };
        const bool                     turning{ fades && state.velocity.length2d_sqr( ) > environment.cap * environment.cap };
        const f32                      before{ turning ? heading( state.velocity.x, state.velocity.y ) : 0.f };
        ctx->m_wish_yaw = ctx->m_air_control.phase_known ? accelerate_air( state, ctx->m_air_control, environment )
                                                         : accelerate_phase_mean( state, ctx->m_air_control, environment );
        if ( turning )
        {
            const f32 withheld{ wrap( heading( state.velocity.x, state.velocity.y ) - before ) * ( 1.f - ctx->m_air_turn_kept ) };
            const f32 radians{ DEG2RAD( withheld ) };
            const f32 cosine{ math::cos( radians ) }, sine{ math::sin( radians ) };
            const f32 x{ state.velocity.x };
            state.velocity.x = cosine * x + sine * state.velocity.y;
            state.velocity.y = cosine * state.velocity.y - sine * x;
            if ( world_goal )
            {
                ctx->m_air_control.yaw -= withheld;
            }
            ctx->m_wish_yaw -= withheld;
        }
        if ( fades )
        {
            ctx->m_air_turn_kept *= AIR_TURN_KEPT_PER_TICK;
        }
        ctx->m_pred_velocity = state.velocity;
        return;
    }

    const f32 air_speed_cap{ ctx->m_air_speed_cap };

    if ( ctx->m_wishspeed <= 0.0f )
    {
        return;
    }

    const vec3 wish{ wish_direction( ctx ) };

    if ( wish.is_zero( ) )
    {
        return;
    }

    const f32 capped{ math::smaller( ctx->m_wishspeed, air_speed_cap ) };
    const f32 current{ ctx->m_pred_velocity.x * wish.x + ctx->m_pred_velocity.y * wish.y };
    const f32 missing{ capped - current };

    if ( missing <= 0.0f )
    {
        return;
    }

    const f32 surface{ climbing_surface_friction( ctx->m_pred_velocity.z,
                                                  ctx->m_pred_surface_friction,
                                                  air_current_friction( ctx ) ) };

    const f32 gained{ math::smaller( air_control_acceleration( ctx ) * ctx->m_wishspeed *
                                         TICK_INTERVAL * surface,
                                     missing ) };

    ctx->m_pred_velocity.x += gained * wish.x;
    ctx->m_pred_velocity.y += gained * wish.y;
}

void strafe_prediction::apply_ground_input( move_sim_ctx_t *ctx )
{
    if ( ctx->m_walk_keyed )
    {
        walk_keys( ctx );
        ctx->m_walk_view_yaw += ctx->m_walk_turn;
        ctx->m_walk_turn *= EYE_FADE;
        mix_keys( ctx );
        return;
    }

    if ( !ctx->m_walk_input && ctx->m_wish_valid && ctx->m_pred_velocity.length2d( ) > 1.0f )
    {
        ctx->m_wish_yaw = heading( ctx->m_pred_velocity.x, ctx->m_pred_velocity.y );
    }

    apply_ground_acceleration( ctx );

    constexpr f32 GROUND_TURN_DECAY{ 0.7f };
    if ( ctx->m_walk_input && !ctx->m_walk_relative )
    {
        ctx->m_wish_yaw += ctx->m_walk_turn;
        ctx->m_walk_turn *= GROUND_TURN_DECAY;
    }
}
