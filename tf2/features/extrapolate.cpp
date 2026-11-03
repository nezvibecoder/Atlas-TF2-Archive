#include "extrapolate.hpp"

#include "entity_cache.hpp"
#include "math/math.hpp"
#include "misc.hpp"
#include "move_sim.hpp"
#include "networking.hpp"
#include "render/render.hpp"
#include "tickbase.hpp"


ATLAS_NO_DESTROY extrapolation g_extrapolate{ };

constexpr i32   STRAGGLE{ 2 };
constexpr float DUCK_SPEED_CROP{ 0.33333333f };
constexpr float NON_GROUND_VELOCITY{ 250.0f };
constexpr float WALKABLE_NORMAL_Z{ 0.7f };

ATLAS_FORCE_INLINE float step_size( )
{
    return g_tf2.sv_stepsize->GetFloat( );
}

constexpr i32   STRAFE_PAIRS{ 16 };
constexpr float STRAFE_MIN_SPEED{ 60.0f };
constexpr float STRAFE_PAIR_LIMIT{ 45.0f };
constexpr float STRAFE_SIGN_VOTE{ 1.0f };
constexpr float STRAFE_NOISE{ 827.0f };
constexpr float AIR_SPEED_CAP{ 30.0f }; // dtheta = 2 asin(cap / v) per tick
constexpr float BUNNYJUMP_CROP{ 1.2f }; // @ CTFGameMovement::PreventBunnyJumping, BUNNYJUMP_MAX_SPEED_FACTOR
constexpr float WATER_SWIM_FACTOR{ 0.8f };
constexpr float WATER_SINK_SPEED{ 60.0f };
constexpr float DUCK_TIME_MS{ 1000.0f };
constexpr float TIME_TO_DUCK{ 0.2f };
constexpr float TIME_TO_UNDUCK{ 0.2f };
constexpr float TF_TIME_TO_DUCK{ 0.3f };
constexpr i32   TF_AIRDUCKED_COUNT{ 2 };

static void rotate_yaw( vec3 &velocity, float degrees )
{
    const float yaw{ DEG2RAD( degrees ) };
    const float sin_yaw{ math::sin( yaw ) };
    const float cos_yaw{ math::cos( yaw ) };

    const float x{ velocity.x * cos_yaw - velocity.y * sin_yaw };
    const float y{ velocity.x * sin_yaw + velocity.y * cos_yaw };

    velocity.x = x;
    velocity.y = y;
}

ATLAS_FORCE_INLINE i32 shot_server_tick( )
{
    const i32 frame{ g_tickbase.send_frame( ) };
    return frame < 0 ? -1 : frame + g_networking.send_delay( ).ticks( );
}

warp_plan_t extrapolation::plan( i32 index, i32 ahead ) const
{
    warp_plan_t       out{ };
    const warp_flow_t flow{ g_lagcomp.flow( index ) };
    const i32         shot_server{ shot_server_tick( ) };

    if ( flow.m_newest_server < 0 || shot_server < 0 )
    {
        return out;
    }

    Assert( flow.m_rhythm >= 1 && flow.m_burst >= 1 );

    const i32 elapsed{ shot_server + ahead - 1 - flow.m_newest_server };
    const i32 silence{ math::bigger( flow.m_latest_server - flow.m_newest_server, 0 ) };

    out.m_valid  = true;
    out.m_breaks = flow.m_loose;
    out.m_warp   = flow.m_charge;

    if ( elapsed <= 0 )
    {
        return out;
    }

    if ( silence > flow.m_rhythm + STRAGGLE )
    {
        out.m_frozen = true;
        out.m_warp   = math::clamp( flow.m_charge + silence - flow.m_rhythm, 0, lag_compensation::command_bank( ) );
        return out;
    }

    const f32 teleport{ g_tf2.sv_lagcompensation_teleport_dist->GetFloat( ) };

    if ( flow.m_rhythm <= 1 )
    {
        out.m_commands = elapsed;

        if ( flow.m_rate > 1.0f && flow.m_charge > 0 )
        {
            const i32 spending{ static_cast< i32 >( ( flow.m_rate - 1.0f ) * static_cast< f32 >( elapsed ) + 0.5f ) };

            out.m_commands += math::smaller( spending, flow.m_charge );
            out.m_breaks |= flow.m_rate * flow.m_speed * TICK_INTERVAL > teleport;
        }
    } else
    {
        const i32 landings{ elapsed / flow.m_rhythm };

        out.m_commands = landings * flow.m_burst;
        out.m_breaks |= landings > 0 && static_cast< f32 >( flow.m_burst ) * flow.m_speed * TICK_INTERVAL > teleport;
    }

    out.m_valid = out.m_commands <= MAX_PATH_TICKS;
    return out;
}

extrapolation::sim_context_t extrapolation::build_context( i32                 index,
                                                           const lag_record_t *newest ) const
{
    sim_context_t context{ };

    context.m_maxspeed    = newest->m_maxspeed;
    context.m_ducked      = newest->m_ducked;
    context.m_swimming    = newest->m_swimming;
    context.m_parachuting = newest->m_parachuting;
    context.m_mins        = newest->m_mins;
    context.m_maxs        = newest->m_maxs;

    const lag_record_t *cur{ newest };
    bool                readable{ cur->m_velocity.length2d( ) > STRAFE_MIN_SPEED };
    float               cur_yaw{ readable ? RAD2DEG( math::atan2( cur->m_velocity.y,
                                                                  cur->m_velocity.x ) )
                                          : 0.0f };

    float sum_dyaw{ };
    float sum_span{ };
    i32   used{ };
    float lock{ };

    for ( i32 age{ 1 }; readable && age <= STRAFE_PAIRS && !cur->m_wall; ++age )
    {
        const lag_record_t *older{ g_lagcomp.record( index, age ) };

        if ( !older || older->m_velocity.length2d( ) <= STRAFE_MIN_SPEED )
        {
            context.m_stop = "end";
            break;
        }

        const float older_yaw{ RAD2DEG( math::atan2( older->m_velocity.y,
                                                     older->m_velocity.x ) ) };
        const float dyaw{ math::normalize_yaw( cur_yaw - older_yaw ) };
        const float span{ 0.5f * static_cast< float >( cur->m_commands + older->m_commands ) };
        const float pair_speed{ math::bigger( cur->m_velocity.length2d( ),
                                              older->m_velocity.length2d( ) ) };
        const float pair_budget{ g_tf2.sv_accelerate->GetFloat( ) * newest->m_maxspeed *
                                 TICK_INTERVAL };
        const float pair_rate{ pair_speed > pair_budget
                                   ? RAD2DEG( math::asin( pair_budget / pair_speed ) )
                                   : 90.0f };
        const float pair_limit{ math::smaller( pair_rate * span, 150.0f ) };

        if ( span <= 0.0f || math::abs( dyaw ) > pair_limit )
        {
            context.m_stop = "gate";
            break;
        }

        if ( math::abs( dyaw ) > STRAFE_SIGN_VOTE )
        {
            if ( lock != 0.0f && dyaw * lock < 0.0f )
            {
                context.m_stop = "flip";
                break; // they flipped; everything older belongs to the previous leg
            }

            if ( lock == 0.0f )
            {
                lock = dyaw > 0.0f ? 1.0f : -1.0f;
            }
        }

        sum_dyaw += dyaw;
        sum_span += span;
        ++used;

        cur     = older;
        cur_yaw = older_yaw;
    }

    context.m_pairs = used;

    if ( !context.m_stop )
    {
        context.m_stop = !readable ? "slow" : ( cur->m_wall ? "wall" : "full" );
    }

    if ( used > 0 && sum_span > 0.0f )
    {
        const float rate{ sum_dyaw / sum_span };
        const float speed{ newest->m_velocity.length2d( ) };
        const float mean_span{ sum_span / static_cast< float >( used ) };
        const float deadband{ math::bigger(
            1.0f, STRAFE_NOISE /
                      ( static_cast< float >( used ) * mean_span * mean_span * speed ) ) };

        if ( math::abs( rate ) > deadband )
        {
            context.m_turn_rate = rate;
            context.m_turn_age  = sum_span;
        } else
        {
            context.m_stop = "band";
        }
    }

    context.m_wishspeed = context.m_ducked ? context.m_maxspeed * DUCK_SPEED_CROP
                                           : context.m_maxspeed;
    const lag_record_t *prev{ g_lagcomp.record( index, 1 ) };

    if ( context.m_turn_rate == 0.0f && !context.m_swimming && newest->m_on_ground && prev &&
         prev->m_on_ground && !newest->m_wall && newest->m_commands >= 1 &&
         newest->m_commands <= STRAFE_PAIRS )
    {
        const float friction{ g_tf2.sv_friction->GetFloat( ) };
        const float stop{ g_tf2.sv_stopspeed->GetFloat( ) };
        const float accel{ g_tf2.sv_accelerate->GetFloat( ) };

        const lag_record_t *base{ prev };
        if ( prev->m_velocity.length2d( ) > 0.88f * context.m_maxspeed )
        {
            const lag_record_t *older{ g_lagcomp.record( index, 2 ) };

            if ( older && older->m_on_ground && !prev->m_wall &&
                 newest->m_commands + prev->m_commands <= STRAFE_PAIRS * 2 )
            {
                base = older;
            }
        }

        const float v_old{ base->m_velocity.length2d( ) };
        const float v_new{ newest->m_velocity.length2d( ) };
        const i32   span{ base == prev ? newest->m_commands : newest->m_commands + prev->m_commands };

        if ( v_old > 1.0f && span >= 1 && friction > 0.0f )
        {
            const float decay{ 1.0f - friction * TICK_INTERVAL };

            float kept{ 1.0f };
            for ( i32 tick{ }; tick < span; ++tick )
            {
                kept *= decay;
            }

            const float released{ v_old >= stop
                                      ? v_old * kept - v_old
                                      : -math::smaller( stop * friction * TICK_INTERVAL *
                                                            static_cast< float >( span ),
                                                        v_old ) };

            const float fixed_point{ context.m_maxspeed * accel / friction };
            const float held{ math::smaller( ( fixed_point - v_old ) * ( 1.0f - kept ),
                                             context.m_maxspeed - v_old ) };

            if ( const float dv{ v_new - v_old }; dv <= ( held + released ) * 0.5f )
            {
                context.m_wishspeed = dv > released + 10.0f ? v_new : 0.0f;
            }
        }
    }

    return context;
}

predicted_state_t extrapolation::seed( i32 index, const lag_record_t *newest ) const
{
    predicted_state_t state{ };

    const loose_state_t *loose{ g_lagcomp.loose( index ) };
    const i32            commands{ loose ? loose->m_commands : newest->m_commands };
    const i32            server_tick{ loose ? loose->m_server_tick : newest->m_server_tick };

    state.m_origin    = loose ? loose->m_origin : newest->m_pose.m_origin;
    state.m_velocity  = loose ? loose->m_velocity : newest->m_velocity;
    state.m_on_ground = loose ? loose->m_on_ground : newest->m_on_ground;

    if ( !state.m_on_ground && !newest->m_swimming && commands >= 1 && !state.m_velocity.is_zero( ) )
    {
        state.m_velocity.z -= g_tf2.sv_gravity->GetFloat( ) * 0.5f * static_cast< float >( commands ) * TICK_INTERVAL;
    }

    seed_origin_t &cached{ this->m_seed_origins[ index ] };
    if ( cached.m_server_tick != server_tick )
    {
        cached.m_server_tick = server_tick;
        cached.m_origin      = state.m_origin;
        cached.m_valid       = true;
        trace_t occupied{ };
        g_cl.trace_hull( cached.m_origin, cached.m_origin, newest->m_mins, newest->m_maxs, occupied );
        if ( occupied.startsolid || occupied.allsolid )
        {
            constexpr float quantum{ 0.125f };
            cached.m_valid = false;
            for ( i32 axes{ 1 }; axes <= 3 && !cached.m_valid; ++axes )
            {
                for ( i32 x{ -1 }; x <= 1 && !cached.m_valid; ++x )
                {
                    for ( i32 y{ -1 }; y <= 1 && !cached.m_valid; ++y )
                    {
                        for ( i32 z{ -1 }; z <= 1 && !cached.m_valid; ++z )
                        {
                            if ( ( x != 0 ) + ( y != 0 ) + ( z != 0 ) != axes )
                            {
                                continue;
                            }
                            const vec3 offset{ x * quantum, y * quantum, z * quantum };
                            const vec3 candidate{ state.m_origin + offset };
                            trace_t    clear{ };
                            g_cl.trace_hull( candidate, candidate, newest->m_mins, newest->m_maxs, clear );
                            if ( !clear.startsolid && !clear.allsolid )
                            {
                                cached.m_origin = candidate;
                                cached.m_valid  = true;
                            }
                        }
                    }
                }
            }
            if ( !cached.m_valid )
            {
                DBGOUT( "extrapolate[%d]: solid seed at tick %d origin %.3f %.3f %.3f; prediction skipped",
                        index, newest->m_tick, state.m_origin.x, state.m_origin.y, state.m_origin.z );
            }
        }
    }
    state.m_origin = cached.m_origin;
    state.m_valid  = cached.m_valid;
    return state;
}

// fuck movement simulation, real gangstas do their own prediction engine
void extrapolation::step( predicted_state_t &state, const sim_context_t &context ) const
{
    const float dt{ TICK_INTERVAL };

    ++state.m_step;

    const float carve{ context.m_turn_age > 0.0f
                           ? context.m_turn_rate *
                                 ( context.m_turn_age /
                                   ( context.m_turn_age + static_cast< float >( state.m_step ) ) )
                           : 0.0f };

    if ( context.m_swimming )
    {
        const float speed{ state.m_velocity.length( ) };
        float       newspeed{ 0.0f };

        if ( speed > 0.0f )
        {
            newspeed = speed - dt * speed * g_tf2.sv_friction->GetFloat( );
            if ( newspeed < 0.1f )
            {
                newspeed = 0.0f;
            }

            state.m_velocity *= newspeed / speed;
        }

        vec3  wish{ state.m_velocity };
        float wishspeed{ context.m_maxspeed * WATER_SWIM_FACTOR };

        if ( wish.normalize( ) <= 0.0f )
        {
            wish      = vec3{ 0.0f, 0.0f, -1.0f };
            wishspeed = WATER_SINK_SPEED * WATER_SWIM_FACTOR;
        }

        if ( const float add{ wishspeed - newspeed }; add > 0.0f )
        {
            const float accel{ math::smaller( g_tf2.sv_accelerate->GetFloat( ) * wishspeed * dt,
                                              add ) };

            state.m_velocity += wish * accel;
        }

        trace_t trace{ };
        g_cl.trace_hull( state.m_origin, state.m_origin + state.m_velocity * dt, context.m_mins,
                         context.m_maxs, trace );

        if ( trace.allsolid )
        {
            state.m_velocity = vec3{ };
            state.m_valid    = false;
            return;
        }
        state.m_origin = trace.endpos;

        if ( trace.fraction < 1.0f )
        {
            state.m_velocity = math::clip_velocity( state.m_velocity, trace.plane.normal );
        }

        return;
    }

    if ( !state.m_on_ground )
    {
        if ( carve != 0.0f )
        {
            const float cap{ AIR_SPEED_CAP * ( context.m_parachuting
                                                   ? g_tf2.tf_parachute_aircontrol->GetFloat( )
                                                   : 1.0f ) };
            const float budget{ 0.5f * g_tf2.sv_airaccelerate->GetFloat( ) * context.m_maxspeed *
                                dt };
            const float window{ math::smaller( cap, budget ) };
            const float speed{ state.m_velocity.length2d( ) };
            const float limit{ speed > window ? RAD2DEG( 2.0f * math::asin( window / speed ) )
                                              : STRAFE_PAIR_LIMIT };

            rotate_yaw( state.m_velocity, math::clamp( carve, -limit, limit ) );
        }

        if ( context.m_parachuting && state.m_velocity.z < 0.0f )
        {
            state.m_velocity.z = math::bigger( state.m_velocity.z,
                                               g_tf2.tf_parachute_maxspeed_z->GetFloat( ) );

            const float drag{ g_tf2.tf_parachute_maxspeed_xy->GetFloat( ) };

            const float speed_x{ math::abs( state.m_velocity.x ) };
            const float speed_y{ math::abs( state.m_velocity.y ) };

            const float give_x{ speed_x > drag ? ( speed_x - drag ) / 3.0f - 10.0f : 0.0f };
            const float give_y{ speed_y > drag ? ( speed_y - drag ) / 3.0f - 10.0f : 0.0f };

            state.m_velocity.x = math::clamp( state.m_velocity.x, -drag - give_x, drag + give_x );
            state.m_velocity.y = math::clamp( state.m_velocity.y, -drag - give_y, drag + give_y );
        }

        const float gravity{ g_tf2.sv_gravity->GetFloat( ) };

        state.m_velocity.z -= gravity * 0.5f * dt;

        trace_t trace{ };
        g_cl.trace_hull( state.m_origin, state.m_origin + state.m_velocity * dt, context.m_mins,
                         context.m_maxs, trace );

        if ( trace.allsolid )
        {
            state.m_velocity = vec3{ };
            state.m_valid    = false;
            return;
        }
        state.m_origin = trace.endpos;

        if ( trace.fraction < 1.0f )
        {
            // the fall met the world
            if ( trace.plane.normal.z >= WALKABLE_NORMAL_Z &&
                 state.m_velocity.z <= NON_GROUND_VELOCITY )
            {
                state.m_on_ground = true;
            }

            state.m_velocity = math::clip_velocity( state.m_velocity, trace.plane.normal );
        }

        state.m_velocity.z -= gravity * 0.5f * dt;

        if ( state.m_on_ground )
        {
            state.m_velocity.z = 0.0f;
        }
    } else
    {
        state.m_velocity.z = 0.0f;
        if ( carve != 0.0f )
        {
            const float speed{ state.m_velocity.length2d( ) };
            float       limit{ STRAFE_PAIR_LIMIT }; // slow enough turns however it likes

            if ( speed > 1.0f )
            {
                const float drop{ math::bigger( speed, g_tf2.sv_stopspeed->GetFloat( ) ) *
                                  g_tf2.sv_friction->GetFloat( ) * dt };
                const float after{ math::bigger( speed - drop, 0.0f ) };
                const float budget{ g_tf2.sv_accelerate->GetFloat( ) * context.m_wishspeed * dt };

                if ( after > 0.0f && budget > 0.0f )
                {
                    const float align{ math::clamp( ( speed * speed - after * after -
                                                      budget * budget ) /
                                                        ( 2.0f * budget * after ),
                                                    -1.0f, 1.0f ) };
                    const float lateral{ math::sqrt( 1.0f - align * align ) * budget };

                    limit = RAD2DEG(
                        math::asin( math::clamp( lateral / speed, 0.0f, 1.0f ) ) );
                }
            }

            rotate_yaw( state.m_velocity, math::clamp( carve, -limit, limit ) );
        }

        if ( const float speed{ state.m_velocity.length2d( ) }; speed >= 0.1f )
        {
            const float control{ math::bigger( speed, g_tf2.sv_stopspeed->GetFloat( ) ) };
            const float drop{ control * g_tf2.sv_friction->GetFloat( ) * dt };
            const float scale{ math::bigger( speed - drop, 0.0f ) / speed };

            state.m_velocity.x *= scale;
            state.m_velocity.y *= scale;
        }

        // how hard the classifier saw them pressing
        const float wishspeed{ context.m_wishspeed };
        if ( vec3 wish{ state.m_velocity.x, state.m_velocity.y, 0.0f };
             wish.normalize( ) > 0.0f && wishspeed > 0.0f )
        {
            const float current{ state.m_velocity.x * wish.x + state.m_velocity.y * wish.y };
            const float add{ wishspeed - current };

            if ( add > 0.0f )
            {
                const float accel{ math::smaller(
                    g_tf2.sv_accelerate->GetFloat( ) * wishspeed * dt, add ) };

                state.m_velocity.x += accel * wish.x;
                state.m_velocity.y += accel * wish.y;
            }
        }

        // TF WalkMove hard-caps the walked speed every tick
        if ( const float walked{ state.m_velocity.length2d( ) }; walked > context.m_maxspeed && walked > 0.0f )
        {
            const float scale{ context.m_maxspeed / walked };

            state.m_velocity.x *= scale;
            state.m_velocity.y *= scale;
        }

        state.m_velocity.z = 0.0f;

        // the walk sweeps horizontally
        const vec3 start{ state.m_origin };
        const vec3 velocity{ state.m_velocity };
        vec3       end{ state.m_origin + state.m_velocity * dt };
        end.z = state.m_origin.z;

        bool    has_support{ };
        trace_t trace{ };
        g_cl.trace_hull( state.m_origin, end, context.m_mins, context.m_maxs, trace );

        if ( trace.allsolid )
        {
            state.m_velocity = vec3{ };
            state.m_valid    = false;
            return;
        } else
        {
            state.m_origin = trace.endpos;
            if ( trace.fraction < 1.0f )
            {
                state.m_velocity = math::clip_velocity( velocity, trace.plane.normal );
            }
        }

        if ( trace.fraction < 1.0f && !trace.startsolid && !trace.allsolid )
        {
            const float height{ step_size( ) + 0.03125f }; // SDK DIST_EPSILON
            trace_t     up{ };
            g_cl.trace_hull( start, start + vec3{ 0.0f, 0.0f, height }, context.m_mins, context.m_maxs, up );
            if ( !up.startsolid && !up.allsolid )
            {
                trace_t raised{ };
                g_cl.trace_hull( up.endpos, up.endpos + velocity * dt, context.m_mins, context.m_maxs, raised );
                if ( !raised.startsolid && !raised.allsolid )
                {
                    trace_t down{ };
                    g_cl.trace_hull( raised.endpos, raised.endpos - vec3{ 0.0f, 0.0f, height },
                                     context.m_mins, context.m_maxs, down );
                    if ( !down.startsolid && !down.allsolid && down.fraction < 1.0f &&
                         down.plane.normal.z >= WALKABLE_NORMAL_Z &&
                         ( down.endpos - start ).length2d_sqr( ) >= ( state.m_origin - start ).length2d_sqr( ) )
                    {
                        has_support      = true;
                        state.m_origin   = down.endpos;
                        state.m_velocity = raised.fraction < 1.0f ? math::clip_velocity( velocity, raised.plane.normal ) : velocity;
                    }
                }
            }
        }

        // stick to whatever floor is underfoot; down a step for ramps and stairs, and if there is
        // none within reach they walked off an edge and the next tick belongs to gravity
        if ( !has_support )
        {
            trace_t floor{ };
            g_cl.trace_hull( state.m_origin, state.m_origin - vec3{ 0.0f, 0.0f, step_size( ) * 2.0f },
                             context.m_mins, context.m_maxs, floor );

            if ( !floor.startsolid && !floor.allsolid && floor.fraction < 1.0f && floor.plane.normal.z >= WALKABLE_NORMAL_Z )
            {
                state.m_origin = floor.endpos;
            } else
            {
                state.m_on_ground = false;
            }
        }

        state.m_velocity.z = 0.0f;
    }

    // CheckVelocity
    const float cap{ g_tf2.sv_maxvelocity->GetFloat( ) };

    state.m_velocity.x = math::clamp( state.m_velocity.x, -cap, cap );
    state.m_velocity.y = math::clamp( state.m_velocity.y, -cap, cap );
    state.m_velocity.z = math::clamp( state.m_velocity.z, -cap, cap );
}

bool extrapolation::predict( i32 index, i32 commands, predicted_state_t &out ) const
{
    const lag_record_t *newest{ g_lagcomp.record( index, 0 ) };

    if ( !newest || commands < 0 || commands > MAX_PATH_TICKS )
    {
        return false;
    }

    const sim_context_t context{ this->build_context( index, newest ) };

    out = this->seed( index, newest );
    if ( !out.m_valid )
    {
        return false;
    }
    out.m_tick      = newest->m_tick + commands;
    out.m_turn_rate = context.m_turn_rate;
    out.m_pairs     = context.m_pairs;
    out.m_stop      = context.m_stop;

    if ( context.m_turn_rate != 0.0f && newest->m_commands > 1 && !g_lagcomp.loose( index ) )
    {
        rotate_yaw( out.m_velocity,
                    context.m_turn_rate * static_cast< float >( newest->m_commands ) * 0.5f );
    }

    for ( i32 tick{ }; tick < commands; ++tick )
    {
        this->step( out, context );
        if ( !out.m_valid )
        {
            return false;
        }
    }

    return true;
}

i32 extrapolation::predict_path( i32 index, predicted_state_t *out, i32 count ) const
{
    const lag_record_t *newest{ g_lagcomp.record( index, 0 ) };

    if ( !newest || !out || count <= 0 )
    {
        return 0;
    }

    count = math::smaller( count, MAX_PATH_TICKS );

    const sim_context_t context{ this->build_context( index, newest ) };

    predicted_state_t state{ this->seed( index, newest ) };
    if ( !state.m_valid )
    {
        return 0;
    }

    // same half-window heading advance the shot path applies
    if ( context.m_turn_rate != 0.0f && newest->m_commands > 1 && !g_lagcomp.loose( index ) )
    {
        rotate_yaw( state.m_velocity,
                    context.m_turn_rate * static_cast< float >( newest->m_commands ) * 0.5f );
    }

    for ( i32 tick{ }; tick < count; ++tick )
    {
        this->step( state, context );
        if ( !state.m_valid )
        {
            return tick;
        }

        state.m_tick = newest->m_tick + tick + 1;
        out[ tick ]  = state;
    }

    return count;
}

ATLAS_FORCE_INLINE float local_input_turn( float previous, float current )
{
    if ( previous * current <= 0.0f || math::abs( previous ) > 15.0f || math::abs( current ) > 15.0f )
    {
        return 0.0f;
    }
    return ( previous + current ) * 0.5f;
}

void extrapolation::update_local_input( )
{
    const CUserCmd &cmd{ *g_cl.m_cmd };
    local_input_t  &input{ this->m_local_input };
    if ( cmd.forwardmove * cmd.forwardmove + cmd.sidemove * cmd.sidemove < 1.0f )
    {
        input = local_input_t{ };
        return;
    }
    if ( input.m_command == cmd.command_number )
    {
        return;
    }

    const float view_yaw{ cmd.viewangles.y };
    const float wish_yaw{ math::normalize_yaw( view_yaw + RAD2DEG( math::atan2( -cmd.sidemove, cmd.forwardmove ) ) ) };
    if ( cmd.command_number != input.m_command + 1 )
    {
        input = local_input_t{ cmd.command_number, view_yaw, wish_yaw };
        return;
    }

    const float view_delta{ math::normalize_yaw( view_yaw - input.m_view_yaw ) };
    const float wish_delta{ math::normalize_yaw( wish_yaw - input.m_wish_yaw ) };
    input.m_view_turn  = local_input_turn( input.m_view_delta, view_delta );
    input.m_wish_turn  = local_input_turn( input.m_wish_delta, wish_delta );
    input.m_view_delta = view_delta;
    input.m_wish_delta = wish_delta;
    input.m_view_yaw   = view_yaw;
    input.m_wish_yaw   = wish_yaw;
    input.m_command    = cmd.command_number;
}

ATLAS_FORCE_INLINE float simple_spline( float value )
{
    const float squared{ value * value };
    return 3.0f * squared - 2.0f * squared * value;
}

ATLAS_FORCE_INLINE void set_duck_view( local_duck_t &duck, float fraction )
{
    duck.m_view = duck.m_duck_view * fraction + duck.m_stand_view * ( 1.0f - fraction );
}

ATLAS_FORCE_INLINE void finish_duck( local_duck_t &duck, move_sim_ctx_t &state )
{
    if ( state.m_pred_flags & FL_DUCKING )
    {
        return;
    }

    state.m_pred_flags |= FL_DUCKING;
    state.m_pred_maxs = duck.m_duck_maxs;
    duck.m_ducked     = true;
    duck.m_ducking    = false;
    duck.m_view       = duck.m_duck_view;

    if ( !( state.m_pred_flags & FL_ONGROUND ) )
    {
        state.m_pred_origin.z += duck.m_stand_maxs.z - duck.m_duck_maxs.z;
    }
}

ATLAS_FORCE_INLINE bool can_unduck( const local_duck_t &duck, move_sim_ctx_t &state )
{
    vec3 end{ state.m_pred_origin };
    if ( !( state.m_pred_flags & FL_ONGROUND ) )
    {
        end.z -= duck.m_stand_maxs.z - duck.m_duck_maxs.z;
    }

    trace_t trace{ };
    g_move_sim.perform_hull_trace( &state, state.m_pred_origin, end, state.m_pred_mins, duck.m_stand_maxs, trace );
    return !trace.startsolid && trace.fraction == 1.0f;
}

ATLAS_FORCE_INLINE void finish_unduck( local_duck_t &duck, move_sim_ctx_t &state )
{
    state.m_pred_flags &= ~FL_DUCKING;
    state.m_pred_maxs = duck.m_stand_maxs;
    duck.m_ducked     = false;
    duck.m_ducking    = false;
    duck.m_view       = duck.m_stand_view;
    duck.m_ducktime   = 0.0f;

    if ( !( state.m_pred_flags & FL_ONGROUND ) )
    {
        state.m_pred_origin.z -= duck.m_stand_maxs.z - duck.m_duck_maxs.z;
        g_move_sim.categorize_position( &state );
    }
}

ATLAS_FORCE_INLINE void step_duck( local_duck_t &duck, move_sim_ctx_t &state, bool pressing, float curtime )
{
    duck.m_ducktime = math::bigger( duck.m_ducktime - 1000.0f * TICK_INTERVAL, 0.0f );

    const bool grounded{ ( state.m_pred_flags & FL_ONGROUND ) != 0 };
    const bool in_duck{ ( state.m_pred_flags & FL_DUCKING ) != 0 };
    bool       held{ pressing };

    if ( grounded )
    {
        duck.m_air_ducks = 0;
    }

    if ( g_tf2.tf_clamp_airducks->GetBool( ) &&
         ( ( grounded && curtime < duck.m_timer ) ||
           ( duck.m_ducked && duck.m_ducking ) ||
           ( !grounded && duck.m_air_ducks >= TF_AIRDUCKED_COUNT ) ) )
    {
        held = false;
    }

    const bool pressed{ held && !duck.m_held };
    const bool released{ !held && duck.m_held };
    duck.m_held = held;

    if ( held )
    {
        if ( pressed && !in_duck )
        {
            duck.m_ducktime = DUCK_TIME_MS;
            duck.m_ducking  = true;
        }

        if ( !duck.m_ducking )
        {
            return;
        }

        const float seconds{ math::bigger( DUCK_TIME_MS - duck.m_ducktime, 0.0f ) * 0.001f };
        if ( seconds > TIME_TO_DUCK || in_duck || !grounded )
        {
            finish_duck( duck, state );
        } else
        {
            set_duck_view( duck, simple_spline( seconds / TIME_TO_DUCK ) );
        }

        return;
    }

    if ( !duck.m_ducking && !in_duck )
    {
        return;
    }

    if ( released )
    {
        duck.m_timer = curtime + TF_TIME_TO_DUCK;
        if ( !grounded )
        {
            ++duck.m_air_ducks;
        }

        if ( in_duck )
        {
            duck.m_ducktime = DUCK_TIME_MS;
        } else if ( !duck.m_ducked )
        {
            duck.m_ducktime = ( DUCK_TIME_MS - duck.m_ducktime ) + ( DUCK_TIME_MS - 1000.0f * TIME_TO_UNDUCK );
        }
    }

    if ( !can_unduck( duck, state ) )
    {
        if ( duck.m_ducktime != DUCK_TIME_MS )
        {
            state.m_pred_flags |= FL_DUCKING;
            state.m_pred_maxs = duck.m_duck_maxs;
            duck.m_ducked     = true;
            duck.m_ducking    = false;
            duck.m_view       = duck.m_duck_view;
            duck.m_ducktime   = DUCK_TIME_MS;
        }

        return;
    }

    const float seconds{ math::bigger( DUCK_TIME_MS - duck.m_ducktime, 0.0f ) * 0.001f };
    if ( seconds > TIME_TO_UNDUCK || !grounded )
    {
        finish_unduck( duck, state );
        return;
    }

    set_duck_view( duck, simple_spline( 1.0f - seconds / TIME_TO_UNDUCK ) );
    duck.m_ducking = true;
}

i32 extrapolation::simulate_local( i32 ticks, vec3 *eyes, i32 capacity, vec3 *path, bool *grounded ) const
{
    C_TFPlayer     *local{ g_cl.m_local };
    const CUserCmd &cmd{ *g_cl.m_cmd };

    move_sim_ctx_t state{ };
    state.m_player_to_predict     = local;
    state.m_pred_origin           = local->GetAbsOrigin( );
    state.m_pred_velocity         = local->m_vecVelocity( );
    state.m_pred_flags            = local->m_fFlags( );
    state.m_pred_mins             = local->m_vecMins( );
    state.m_pred_maxs             = local->m_vecMaxs( );
    state.m_pred_surface_friction = local->m_surfaceFriction( );
    state.m_pred_lost_footing     = local->InCond( TF_COND_LOST_FOOTING );
    state.m_pred_air_current      = local->InCond( TF_COND_AIR_CURRENT );

    const float  scale{ local->m_flModelScale( ) };
    local_duck_t duck{ };
    duck.m_stand_maxs = VEC_HULL_MAX * scale;
    duck.m_duck_maxs  = VEC_DUCK_HULL_MAX * scale;
    duck.m_stand_view = local->GetClassEyeHeight( ).z;
    duck.m_duck_view  = TFGameRules( )->GetViewVectors( )->m_vDuckView.z * scale;
    duck.m_view       = local->m_vecViewOffset( ).z;
    duck.m_ducktime   = local->m_flDucktime( );
    duck.m_timer      = local->m_flDuckTimer( );
    duck.m_air_ducks  = local->m_nAirDucked( );
    duck.m_ducked     = local->m_bDucked( );
    duck.m_ducking    = local->m_bDucking( );
    duck.m_held       = ( local->m_nOldButtons( ) & IN_DUCK ) != 0;

    vec3 forward{ }, right{ };
    math::angle_vectors( cmd.viewangles, &forward, &right );
    forward.z = 0.0f;
    right.z   = 0.0f;
    forward.normalize( );
    right.normalize( );

    vec3        input{ forward * cmd.forwardmove + right * cmd.sidemove };
    const bool  current_input{ this->m_local_input.m_command == cmd.command_number };
    const float view_turn{ current_input ? this->m_local_input.m_view_turn : 0.0f };
    const float wish_turn{ current_input ? this->m_local_input.m_wish_turn : 0.0f };
    const float input_yaw{ RAD2DEG( math::atan2( input.y, input.x ) ) };
    const float input_speed{ input.length2d( ) };
    const float maxspeed{ local->m_flMaxspeed( ) };
    const float dt{ TICK_INTERVAL };
    const float gravity{ g_tf2.sv_gravity->GetFloat( ) *
                         ( local->m_flGravity( ) != 0.0f ? local->m_flGravity( ) : 1.0f ) };
    const bool  hop{ g_misc.bhop_held( ) };
    const bool  pressing_duck{ ( cmd.buttons & IN_DUCK ) != 0 };
    const i32   tickbase{ local->m_nTickBase( ) };
    const float jump_speed{ JUMP_SPEED * g_cl.attrib_hook( g_cl.m_weapon,
                                                           g_cl.attrib_hook( local, 1.0f, ATLAS_XORSTR( "mod_jump_height" ).get( ) ),
                                                           ATLAS_XORSTR( "mod_jump_height_from_weapon" ).get( ) ) };
    i32        written{ };
    const auto record_step{ [ & ]( )
    {
        if ( written < capacity )
        {
            if ( path )
            {
                path[ written ] = state.m_pred_origin;
            }

            if ( grounded )
            {
                grounded[ written ] = ( state.m_pred_flags & FL_ONGROUND ) != 0;
            }

            eyes[ written++ ] = state.m_pred_origin + vec3{ 0.0f, 0.0f, duck.m_view };
        }
    } };
    record_step( );

    move_sim_player_t players[ MAX_TRACKED_PLAYERS ]{ };
    obstacle_t        obstacles[ MAX_TRACKED_PLAYERS ]{ };
    const i32         obstacle_count{ this->gather_obstacles( local, ticks, players, obstacles ) };
    state.m_players      = players;
    state.m_player_count = obstacle_count;

    for ( i32 tick{ }; tick < ticks; ++tick )
    {
        this->advance_obstacles( tick + 1, players, obstacles, obstacle_count );

        const bool cropped{ ( state.m_pred_flags & FL_DUCKING ) && ( state.m_pred_flags & FL_ONGROUND ) };
        step_duck( duck, state, pressing_duck, TICKS_TO_TIME( tickbase + 1 + tick ) );

        g_move_sim.apply_parachute_reductions( &state );

        if ( view_turn != 0.0f )
        {
            math::angle_vectors( vec3{ 0.0f, cmd.viewangles.y + view_turn * ( tick + 1 ), 0.0f },
                                 &forward, &right );
        }
        if ( wish_turn != 0.0f )
        {
            math::angle_vectors( vec3{ 0.0f, input_yaw + wish_turn * ( tick + 1 ), 0.0f }, &input );
            input *= input_speed;
        }
        vec3       &velocity{ state.m_pred_velocity };
        const bool  grounded{ ( state.m_pred_flags & FL_ONGROUND ) != 0 };
        const float surface{ state.m_pred_surface_friction };
        vec3        wishdir{ input };
        const float wishspeed{ math::smaller( wishdir.normalize( ), maxspeed ) };
        if ( grounded && ( !hop || ( state.m_pred_flags & FL_DUCKING ) ) )
        {
            const strafe_prediction::ground_physics ground{ dt, g_tf2.sv_friction->GetFloat( ),
                                                            g_tf2.sv_stopspeed->GetFloat( ), g_tf2.sv_accelerate->GetFloat( ),
                                                            maxspeed, surface, state.m_pred_lost_footing };
            strafe_prediction::ground_step( velocity, wishdir, cropped ? wishspeed * DUCK_SPEED_CROP : wishspeed, ground );

            const float back_fraction{ g_tf2.tf_clamp_back_speed->GetFloat( ) };
            if ( back_fraction < 1.0f &&
                 velocity.length2d( ) > g_tf2.tf_clamp_back_speed_min->GetFloat( ) )
            {
                const float along{ forward.dot( velocity ) };
                if ( along < -maxspeed * back_fraction )
                {
                    velocity = forward * ( -maxspeed * back_fraction ) + right * right.dot( velocity );
                    strafe_prediction::ground_clamp_speed( velocity, maxspeed );
                }
            }
        } else
        {
            if ( grounded )
            {
                velocity.z = -gravity * 0.5f * dt;
                const float speed{ velocity.length( ) };
                if ( speed > BUNNYJUMP_CROP * maxspeed )
                {
                    velocity *= BUNNYJUMP_CROP * maxspeed / speed;
                }
                velocity.z = duck.m_ducking ? jump_speed : velocity.z + jump_speed;
                state.m_pred_flags &= ~FL_ONGROUND;
            }
            const float cap{ AIR_SPEED_CAP * ( local->InCond( TF_COND_PARACHUTE_ACTIVE )
                                                   ? g_tf2.tf_parachute_aircontrol->GetFloat( )
                                                   : 1.0f ) };
            const float add{ math::smaller( wishspeed, cap ) - velocity.dot( wishdir ) };
            if ( add > 0.0f )
            {
                velocity += wishdir * math::smaller(
                                          g_tf2.sv_airaccelerate->GetFloat( ) * wishspeed * surface * dt, add );
            }
            velocity.z -= gravity * 0.5f * dt;
        }

        g_move_sim.move_local( &state );
        if ( !( state.m_pred_flags & FL_ONGROUND ) )
        {
            velocity.z -= gravity * 0.5f * dt;
        }
        record_step( );
    }
    return written;
}

ATLAS_FORCE_INLINE vec3 quantization_fit( const vec3 &networked, const vec3 &mins, const vec3 &maxs, const vec3 &our_mins,
                                          const vec3 &our_maxs )
{
    constexpr float RESOLUTION{ 0.125f };
    constexpr float EPSILON{ static_cast< float >( DIST_EPSILON ) };

    vec3  shift{ };
    float least{ RESOLUTION };
    for ( usize axis{ }; axis < 3; ++axis )
    {
        if ( our_maxs[ axis ] <= mins[ axis ] || our_mins[ axis ] >= maxs[ axis ] )
        {
            return vec3{ };
        }

        const float needed{ networked[ axis ] > -RESOLUTION ? our_maxs[ axis ] + EPSILON - mins[ axis ]
                                                            : our_mins[ axis ] - EPSILON - maxs[ axis ] };
        if ( math::abs( needed ) < least )
        {
            least         = math::abs( needed );
            shift         = vec3{ };
            shift[ axis ] = needed;
        }
    }

    return shift;
}

i32 extrapolation::gather_obstacles( C_TFPlayer        *local,
                                     i32                ticks,
                                     move_sim_player_t *players,
                                     obstacle_t        *obstacles ) const
{
    CTraceFilterSimple filter{ local, COLLISION_GROUP_PLAYER_MOVEMENT };
    const u32          mask{ g_move_sim.player_solid_mask( local ) };
    const vec3         origin{ local->GetAbsOrigin( ) };
    const vec3         extent{ local->m_vecMaxs( ) };
    const float        axis_speed{ g_tf2.sv_maxvelocity->GetFloat( ) };
    i32                count{ };

    for ( const auto *cached : g_entity_cache.players( ) )
    {
        C_TFPlayer *player{ cached->m_entity->As< C_TFPlayer * >( ) };
        if ( player == local || player->IsDormant( ) || !filter.ShouldHitEntity( player, mask ) )
        {
            continue;
        }

        obstacle_t &obstacle{ obstacles[ count ] };
        obstacle = obstacle_t{ player->entindex( ) };
        const warp_plan_t last{ this->plan( obstacle.m_index, ticks ) };
        const i32         commands{ last.m_valid ? last.m_commands : 0 };
        if ( last.m_valid && this->predict( obstacle.m_index, 0, obstacle.m_state ) )
        {
            obstacle.m_context = this->build_context( obstacle.m_index, g_lagcomp.record( obstacle.m_index, 0 ) );
        } else
        {
            obstacle.m_state.m_origin = player->GetAbsOrigin( );
            obstacle.m_state.m_valid  = false;
        }
        obstacle.m_networked = obstacle.m_state.m_origin;

        const float travel{ axis_speed * TICK_INTERVAL * static_cast< float >( ticks + commands ) };
        const vec3  gap{ obstacle.m_state.m_origin - origin };
        if ( math::abs( gap.x ) > travel + extent.x + player->m_vecMaxs( ).x ||
             math::abs( gap.y ) > travel + extent.y + player->m_vecMaxs( ).y )
        {
            continue;
        }

        players[ count++ ] = move_sim_player_t{ player, obstacle.m_state.m_origin };
    }

    this->advance_obstacles( 0, players, obstacles, count );
    for ( i32 at{ }; at < count; ++at )
    {
        C_TFPlayer *player{ players[ at ].m_player };
        vec3       &placed{ obstacles[ at ].m_state.m_origin };
        placed += quantization_fit( obstacles[ at ].m_networked, placed + player->m_vecMins( ), placed + player->m_vecMaxs( ),
                                    origin + local->m_vecMins( ), origin + extent );
        players[ at ].m_origin = placed;
    }

    return count;
}

void extrapolation::advance_obstacles( i32 ahead, move_sim_player_t *players, obstacle_t *obstacles, i32 count ) const
{
    for ( i32 at{ }; at < count; ++at )
    {
        obstacle_t &obstacle{ obstacles[ at ] };
        const i32   commands{ this->plan( obstacle.m_index, ahead ).m_commands };
        while ( obstacle.m_state.m_valid && obstacle.m_commands < commands )
        {
            this->step( obstacle.m_state, obstacle.m_context );
            ++obstacle.m_commands;
        }

        players[ at ].m_origin = obstacle.m_state.m_origin;
    }
}

const lag_record_t *extrapolation::synthesize( i32 index, i32 commands, lag_record_t &storage )
{
    const lag_record_t *newest{ g_lagcomp.record( index, 0 ) };

    if ( !newest )
    {
        return nullptr;
    }

    // nothing to walk, the newest record is the pose, bones and all
    if ( commands == 0 && !g_lagcomp.loose( index ) )
    {
        return newest;
    }

    predicted_state_t state{ };
    if ( !this->predict( index, commands, state ) )
    {
        return nullptr;
    }

    storage                 = *newest;
    storage.m_pose.m_origin = state.m_origin;
    storage.m_tick          = state.m_tick;
    storage.m_velocity      = state.m_velocity;
    storage.m_on_ground     = state.m_on_ground;

    // the copied bones belong to where the player was, not where this record puts him, so they
    // are dropped and whoever needs bones for this pose builds them
    storage.m_bone_count = 0;

    this->remember( index, state, newest );

    return &storage;
}

void extrapolation::remember( i32 index, const predicted_state_t &state, const lag_record_t *from )
{
    if ( index < 1 || index >= MAX_TRACKED_PLAYERS )
    {
        return;
    }

    pending_t &slot{ this->m_pending[ index ] };

    slot.m_origin      = state.m_origin;
    slot.m_from_origin = from->m_pose.m_origin;
    slot.m_turn_rate   = state.m_turn_rate;
    slot.m_pairs       = state.m_pairs;
    slot.m_stop        = state.m_stop;
    slot.m_tick        = state.m_tick;
    slot.m_from_tick   = from->m_tick;
}

// a guess owed against a track that just died, respawned or lost its chain grades the distance
// between two unrelated lives, which is noise that swamps the real numbers
void extrapolation::forget( i32 index )
{
    if ( index < 1 || index >= MAX_TRACKED_PLAYERS )
    {
        return;
    }

    this->m_pending[ index ].m_tick = -1;
    this->m_seed_origins[ index ]   = seed_origin_t{ };
}

void extrapolation::on_record( i32 index, i32 tick, const vec3 &origin )
{
    if ( index < 1 || index >= MAX_TRACKED_PLAYERS )
    {
        return;
    }

    pending_t &slot{ this->m_pending[ index ] };

    if ( slot.m_tick < 0 || tick < slot.m_tick )
    {
        return; // nothing owed, or the future has not arrived yet
    }

    const vec3  miss{ origin - slot.m_origin };
    vec3        heading{ slot.m_origin - slot.m_from_origin };
    const float travelled{ heading.normalize( ) };
    const float along{ travelled > 0.0f ? heading.dot( miss ) : 0.0f };
    const float cross{ travelled > 0.0f
                           ? math::sqrt( math::bigger( miss.length_sqr( ) - along * along, 0.0f ) )
                           : miss.length( ) };

    DBGOUT( "extrapolate[%d]: predicted for tick %d from tick %d, arrived on tick %d, off by %d "
            "units (along %d cross %d over %d) turn %.2f deg/t x%d %s",
            index, slot.m_tick, slot.m_from_tick, tick, static_cast< i32 >( miss.length( ) ),
            static_cast< i32 >( along ), static_cast< i32 >( cross ),
            static_cast< i32 >( travelled ), slot.m_turn_rate, slot.m_pairs,
            slot.m_stop ? slot.m_stop : "-" );

    slot.m_tick = -1;
}

void extrapolation::paint( ) const
{
    if ( !g_cfg.m_lagcomp.m_draw_records || !g_tf2.m_engine->IsInGame( ) )
    {
        return;
    }

    for ( i32 index{ 1 }; index < MAX_TRACKED_PLAYERS; ++index )
    {
        const pending_t &slot{ this->m_pending[ index ] };

        if ( slot.m_tick < 0 )
        {
            continue;
        }

        /*vec2 screen{ };
        if ( !g_cl.world_to_screen( slot.m_origin, screen ) )
        {
            continue;
        }

        const point_t center{ static_cast< i32 >( screen.x ), static_cast< i32 >( screen.y ) };

        g_draw.line( point_t{ center.x - 4, center.y }, point_t{ center.x + 4, center.y },
                     { 255, 120, 120, 235 } );
        g_draw.line( point_t{ center.x, center.y - 4 }, point_t{ center.x, center.y + 4 },
                     { 255, 120, 120, 235 } );*/
    }
}

void extrapolation::clear( )
{
    this->m_local_input = local_input_t{ };
    for ( i32 index{ }; index < MAX_TRACKED_PLAYERS; ++index )
    {
        this->m_pending[ index ].m_tick      = -1;
        this->m_pending[ index ].m_from_tick = -1;
        this->m_seed_origins[ index ]        = seed_origin_t{ };
    }
}
