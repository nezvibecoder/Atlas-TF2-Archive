#pragma once

#include "math/vector.hpp"

struct move_sim_ctx_t;

namespace strafe_prediction
{
    inline constexpr f32 TF_MOVEMENT_MAX_SPEED{ 520.0f }; // @ CTFGameMovement::ProcessMovement / TF_MAX_SPEED
    inline constexpr i32 GROUND_KEYS{ 9 };
    inline constexpr i32 GROUND_WALKS{ GROUND_KEYS + 1 };

    ATLAS_FORCE_INLINE f32 movement_max_speed( f32 observed_max_speed, bool lost_footing,
                                               bool charging = false, f32 charge_max_speed = 0.0f )
    {
        const f32 precheck{ charging ? charge_max_speed : TF_MOVEMENT_MAX_SPEED };
        if ( lost_footing )
        {
            return precheck;
        }
        return observed_max_speed > 0.0f ? math::smaller( observed_max_speed, precheck ) : precheck;
    }

    enum class model : u8
    {
        coast,
        locked,
        world,
        sweep,
        retrack,
        retrack_drift,
        feedback,
        phase_left,
        phase_right,
        count
    };

    struct physics
    {
        f32 dt{ 0.015f };
        f32 gravity{ 800.f };
        f32 acceleration{ 10.f };
        f32 cap{ 30.f };
        f32 maxspeed{ 240.f };
        f32 quantum{ 0.125f };
        f32 air_friction{ 1.0f };
        f32 max_velocity{ 3500.0f };
    };

    struct observation
    {
        vec3 origin{ };
        vec3 velocity{ };
        i32  tick{ };
        i32  gap{ 1 };
        i32  recategorize{ }; // A known crouch transition before this tick's AirMove.
    };

    struct state
    {
        vec3 origin{ };
        vec3 velocity{ };
        f32  friction{ 1.f };
        i32  tick{ };
    };

    struct height_sample
    {
        f32 height{ };
        f32 shift{ };
        i32 tick{ };
    };

    struct control
    {
        model kind{ model::coast };
        f32   yaw{ };
        f32   rate{ };
        f32   beta{ 0.05f };
        f32   speed{ 240.f };
        i32   epoch{ };
        bool  phase_known{ true };
        f32   phase_spread_x{ };
        f32   phase_spread_y{ };
        bool  operator==( const control &other ) const = default;
    };

    struct fit
    {
        control input{ };
        state   initial{ };
        state   current{ };
        f32     error{ FLT_MAX };
        i32     switches{ };
    };

    struct solution
    {
        fit chosen{ };
    };

    struct ground_physics
    {
        f32  dt{ 0.015f };
        f32  friction{ 4.0f };
        f32  stop_speed{ 100.0f };
        f32  acceleration{ 10.0f };
        f32  max_speed{ 240.0f };
        f32  surface_friction{ 1.0f };
        bool lost_footing{ false };
    };

    ATLAS_FORCE_INLINE void ground_friction( vec3 &velocity, const ground_physics &environment )
    {
        const f32 speed{ velocity.length( ) };
        if ( speed < 0.1f )
        {
            return;
        }

        const f32 control{ math::bigger( speed, environment.stop_speed ) };
        const f32 drop{ control * environment.friction * environment.surface_friction * environment.dt };
        const f32 new_speed{ math::bigger( 0.0f, speed - drop ) };

        if ( new_speed != speed )
        {
            velocity *= new_speed / speed;
        }
    }

    ATLAS_FORCE_INLINE void ground_accelerate( vec3 &velocity, const vec3 &wish_direction,
                                               f32 wish_speed, const ground_physics &environment )
    {
        if ( wish_speed <= 0.0f )
        {
            return;
        }

        vec3 wish{ wish_direction };
        if ( wish.normalize( ) <= 0.0f )
        {
            return;
        }

        const f32 current{ velocity.dot( wish ) };
        const f32 missing{ wish_speed - current };
        if ( missing <= 0.0f )
        {
            return;
        }

        f32       acceleration{ environment.acceleration };
        const f32 friction{ environment.friction * environment.surface_friction };
        if ( wish_speed < 100.0f * friction / acceleration && !environment.lost_footing )
        {
            const f32 control{ math::bigger( velocity.length( ), environment.stop_speed ) };
            acceleration = control * friction / wish_speed + 1.0f;
        }

        const f32 gained{ math::smaller( acceleration * wish_speed * environment.dt *
                                             environment.surface_friction,
                                         missing ) };
        velocity += wish * gained;
    }

    ATLAS_FORCE_INLINE void ground_clamp_speed( vec3 &velocity, f32 max_speed )
    {
        if ( max_speed <= 0.0f )
        {
            return;
        }

        const f32 speed{ velocity.length2d( ) };
        if ( speed <= max_speed )
        {
            return;
        }

        const f32 scale{ max_speed / speed };
        velocity.x *= scale;
        velocity.y *= scale;
    }

    ATLAS_FORCE_INLINE void ground_clamp_back_speed( vec3 &velocity, f32 view_yaw, f32 max_speed, f32 scale, f32 floor )
    {
        if ( scale >= 1.0f || velocity.length2d( ) <= floor )
        {
            return;
        }

        const f32 radians{ DEG2RAD( view_yaw ) };
        const f32 forward_x{ math::cos( radians ) };
        const f32 forward_y{ math::sin( radians ) };
        const f32 forward{ velocity.x * forward_x + velocity.y * forward_y };
        if ( forward >= 0.0f )
        {
            return;
        }

        const f32 right{ velocity.x * forward_y - velocity.y * forward_x };
        const f32 back{ math::bigger( forward, -max_speed * scale ) };
        velocity.x = forward_x * back + forward_y * right;
        velocity.y = forward_y * back - forward_x * right;
        ground_clamp_speed( velocity, max_speed );
    }

    ATLAS_FORCE_INLINE void ground_step( vec3 &velocity, const vec3 &wish_direction,
                                         f32 wish_speed, const ground_physics &environment )
    {
        velocity.z = 0.0f;
        ground_friction( velocity, environment );
        ground_accelerate( velocity, wish_direction, wish_speed, environment );
        ground_clamp_speed( velocity, environment.max_speed );
        if ( velocity.length( ) < 1.0f )
        {
            velocity = vec3{ };
        }
    }

    void initialize( move_sim_ctx_t *ctx );
    void begin_tick( move_sim_ctx_t *ctx );
    void on_landed( move_sim_ctx_t *ctx );
    void on_left_ground( move_sim_ctx_t *ctx, bool after_move );
    void apply_ground_input( move_sim_ctx_t *ctx );
    void apply_air_input( move_sim_ctx_t *ctx );
} // namespace strafe_prediction
