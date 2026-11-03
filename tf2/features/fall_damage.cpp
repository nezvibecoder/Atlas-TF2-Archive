#include "fall_damage.hpp"

#include "gui/gui.hpp"

#include "move_sim.hpp"

ATLAS_NO_DESTROY fall_damage g_fall_damage{ };

constexpr f32 AIR_SPEED_CAP{ 30.0f };

constexpr i32 AIR_DUCK_LIMIT{ 2 };

constexpr color_t VERDICT_WORKS{ 126, 196, 107 };
constexpr color_t VERDICT_FAILS{ 239, 75, 72 };
constexpr i32     VERDICT_OFFSET{ 24 };

ATLAS_FORCE_INLINE bool bind_ready( bind_t &bind )
{
    const bool gated{ bind.m_key != 0 || bind.m_mode == bind_mode_t::always };
    return !gated || gui::bind_active( bind );
}

ATLAS_FORCE_INLINE vec3 hull_maxs( const fall_physics_t &physics, bool ducked )
{
    return ( ducked ? VEC_DUCK_HULL_MAX : VEC_HULL_MAX ) * physics.m_scale;
}

ATLAS_FORCE_INLINE vec3 clip_velocity( const vec3 &velocity, const vec3 &normal )
{
    vec3 out{ velocity - normal * velocity.dot( normal ) };

    const f32 adjust{ out.dot( normal ) };
    if ( adjust < 0.0f )
    {
        out -= normal * adjust;
    }

    return out;
}

ATLAS_FORCE_INLINE void try_player_move( const fall_physics_t &physics, fall_state_t &state )
{
    constexpr i32 MAX_BUMPS{ 4 };
    constexpr i32 MAX_CLIP_PLANES{ 5 };

    const vec3 mins{ VEC_HULL_MIN * physics.m_scale };
    const vec3 maxs{ hull_maxs( physics, state.m_ducked ) };
    const vec3 primal{ state.m_velocity };

    vec3 planes[ MAX_CLIP_PLANES ]{ };
    i32  plane_count{ };
    vec3 original{ state.m_velocity };
    f32  time_left{ TICK_INTERVAL };
    f32  all_fraction{ };

    for ( i32 bump{ }; bump < MAX_BUMPS; ++bump )
    {
        if ( state.m_velocity.length( ) == 0.0f )
        {
            break;
        }

        trace_t trace{ };
        g_move_sim.perform_hull_trace( physics.m_local, state.m_origin, state.m_origin + state.m_velocity * time_left,
                                       mins, maxs, trace );

        all_fraction += trace.fraction;

        if ( trace.allsolid )
        {
            state.m_velocity = vec3{ };
            return;
        }

        if ( trace.fraction > 0.0f )
        {
            state.m_origin = trace.endpos;
            original       = state.m_velocity;
            plane_count    = 0;
        }

        if ( trace.fraction == 1.0f )
        {
            break;
        }

        time_left -= time_left * trace.fraction;

        if ( plane_count >= MAX_CLIP_PLANES )
        {
            state.m_velocity = vec3{ };
            break;
        }

        planes[ plane_count++ ] = trace.plane.normal;

        if ( plane_count == 1 )
        {
            state.m_velocity = clip_velocity( original, planes[ 0 ] );
            original         = state.m_velocity;
            continue;
        }

        i32 slid{ };
        for ( ; slid < plane_count; ++slid )
        {
            state.m_velocity = clip_velocity( original, planes[ slid ] );

            i32 against{ };
            for ( ; against < plane_count; ++against )
            {
                if ( against != slid && state.m_velocity.dot( planes[ against ] ) < 0.0f )
                {
                    break;
                }
            }

            if ( against == plane_count )
            {
                break;
            }
        }

        if ( slid == plane_count )
        {
            if ( plane_count != 2 )
            {
                state.m_velocity = vec3{ };
                break;
            }

            const vec3 crease{ planes[ 0 ].cross( planes[ 1 ] ).normalized( ) };
            state.m_velocity = crease * crease.dot( state.m_velocity );
        }

        if ( state.m_velocity.dot( primal ) <= 0.0f )
        {
            state.m_velocity = vec3{ };
            break;
        }
    }

    if ( all_fraction == 0.0f )
    {
        state.m_velocity = vec3{ };
    }
}

ATLAS_FORCE_INLINE bool touches_ground( const fall_physics_t &physics, const fall_state_t &state, f32 &ground )
{
    const vec3 mins{ VEC_HULL_MIN * physics.m_scale };
    const vec3 maxs{ hull_maxs( physics, state.m_ducked ) };
    const vec3 end{ state.m_origin.x, state.m_origin.y, state.m_origin.z - 2.0f };

    trace_t trace{ };
    g_move_sim.perform_hull_trace( physics.m_local, state.m_origin, end, mins, maxs, trace );

    if ( trace.fraction == 1.0f )
    {
        return false;
    }

    ground = trace.endpos.z;

    if ( trace.plane.normal.z >= 0.7f )
    {
        return true;
    }

    const vec3 quadrant_mins[ 4 ]{ mins, vec3{ 0.0f, 0.0f, mins.z }, vec3{ mins.x, 0.0f, mins.z }, vec3{ 0.0f, mins.y, mins.z } };
    const vec3 quadrant_maxs[ 4 ]{ vec3{ 0.0f, 0.0f, maxs.z }, maxs, vec3{ 0.0f, maxs.y, maxs.z }, vec3{ maxs.x, 0.0f, maxs.z } };

    for ( usize quadrant{ }; quadrant < 4; ++quadrant )
    {
        g_move_sim.perform_hull_trace( physics.m_local, state.m_origin, end, quadrant_mins[ quadrant ],
                                       quadrant_maxs[ quadrant ], trace );

        if ( trace.m_pEnt && trace.plane.normal.z >= 0.7f )
        {
            return true;
        }
    }

    return false;
}

ATLAS_FORCE_INLINE bool can_unduck( const fall_physics_t &physics, const fall_state_t &state )
{
    const vec3 unducked{ state.m_origin.x, state.m_origin.y, state.m_origin.z - physics.m_lift };

    trace_t trace{ };
    g_move_sim.perform_hull_trace( physics.m_local, state.m_origin, unducked, VEC_HULL_MIN * physics.m_scale,
                                   hull_maxs( physics, false ), trace );

    return !trace.startsolid && trace.fraction == 1.0f;
}

ATLAS_FORCE_INLINE bool unduck_lands( const fall_physics_t &physics, const fall_state_t &state )
{
    if ( !state.m_ducked || !can_unduck( physics, state ) )
    {
        return false;
    }

    fall_state_t unducked{ state };
    unducked.m_origin.z -= physics.m_lift;
    unducked.m_ducked = false;

    f32 ground{ };
    return touches_ground( physics, unducked, ground );
}

ATLAS_FORCE_INLINE fall_outcome_t landing( f32 fall )
{
    return fall > TF_PLAYER_MAX_SAFE_FALL_SPEED ? fall_outcome_t::hurt : fall_outcome_t::landed;
}

ATLAS_FORCE_INLINE bool spares( fall_outcome_t outcome )
{
    return outcome == fall_outcome_t::saved || outcome == fall_outcome_t::landed;
}

ATLAS_FORCE_INLINE fall_outcome_t step( const fall_physics_t &physics, fall_state_t &state, bool duck, const vec3 &wish,
                                        f32 &ground )
{
    const f32  fall{ -state.m_velocity.z };
    const bool pressed{ duck && state.m_air_ducks < AIR_DUCK_LIMIT };

    if ( pressed && !state.m_ducked )
    {
        state.m_origin.z += physics.m_lift;
        state.m_ducked = true;
    } else if ( !pressed && state.m_ducked )
    {
        if ( state.m_held )
        {
            ++state.m_air_ducks;
        }

        if ( can_unduck( physics, state ) )
        {
            state.m_origin.z -= physics.m_lift;
            state.m_ducked = false;

            if ( touches_ground( physics, state, ground ) )
            {
                return landing( fall );
            }
        }
    }

    state.m_held = pressed;
    state.m_velocity.z -= physics.m_half_gravity;

    vec3      wishdir{ wish };
    const f32 wishspeed{ math::smaller( wishdir.normalize( ), physics.m_max_speed ) };
    const f32 add{ math::smaller( wishspeed, AIR_SPEED_CAP ) - state.m_velocity.dot( wishdir ) };
    if ( add > 0.0f )
    {
        state.m_velocity += wishdir * math::smaller( g_tf2.sv_airaccelerate->GetFloat( ) * wishspeed * TICK_INTERVAL, add );
    }

    try_player_move( physics, state );

    if ( state.m_velocity.z <= 250.0f && touches_ground( physics, state, ground ) )
    {
        return landing( fall );
    }

    state.m_velocity.z -= physics.m_half_gravity;

    if ( -state.m_velocity.z < fall && -state.m_velocity.z <= TF_PLAYER_MAX_SAFE_FALL_SPEED )
    {
        return fall_outcome_t::saved;
    }

    return fall_outcome_t::open;
}

ATLAS_FORCE_INLINE jump_bug_verdict_t predict_jump_bug( const fall_physics_t &physics, fall_state_t state )
{
    const i32 limit{ TIME_TO_TICKS( MAX_SIM_TIME ) };

    for ( i32 tick{ }; tick < limit; ++tick )
    {
        if ( unduck_lands( physics, state ) )
        {
            return jump_bug_verdict_t::works;
        }

        f32                  ground{ };
        const fall_outcome_t outcome{ step( physics, state, true, physics.m_wish, ground ) };

        if ( outcome == fall_outcome_t::hurt )
        {
            return jump_bug_verdict_t::fails;
        }

        if ( outcome != fall_outcome_t::open )
        {
            return jump_bug_verdict_t::none;
        }
    }

    return jump_bug_verdict_t::none;
}

ATLAS_FORCE_INLINE vec3 brake_wish( const fall_physics_t &physics, const vec3 &velocity )
{
    vec3 wish{ -velocity.x, -velocity.y, 0.0f };
    wish.normalize( );

    return wish * physics.m_max_speed;
}

ATLAS_FORCE_INLINE fall_result_t simulate( const fall_physics_t &physics, fall_state_t state, const edge_bug_plan_t &plan,
                                           i32 horizon )
{
    fall_result_t result{ };

    for ( result.m_ticks = 1; result.m_ticks <= horizon; ++result.m_ticks )
    {
        vec3 wish{ physics.m_wish };
        if ( plan.m_steer )
        {
            wish = result.m_ticks <= plan.m_brake ? brake_wish( physics, state.m_velocity ) : vec3{ };
        }

        result.m_outcome = step( physics, state, plan.m_duck, wish, result.m_ground );
        if ( result.m_outcome != fall_outcome_t::open )
        {
            break;
        }
    }

    return result;
}

ATLAS_FORCE_INLINE bool search( const fall_physics_t &physics, const fall_state_t &state, bool duck, i32 horizon,
                                edge_bug_plan_t &plan, fall_result_t &found, fall_result_t ( &ends )[ 2 ] )
{
    plan      = edge_bug_plan_t{ duck, true, 0 };
    ends[ 0 ] = simulate( physics, state, plan, horizon );

    if ( spares( ends[ 0 ].m_outcome ) )
    {
        found = ends[ 0 ];
        return true;
    }

    plan.m_brake = horizon;
    ends[ 1 ]    = simulate( physics, state, plan, horizon );

    if ( ends[ 0 ].m_outcome == fall_outcome_t::hurt && ends[ 1 ].m_outcome == fall_outcome_t::hurt &&
         math::abs( ends[ 0 ].m_ground - ends[ 1 ].m_ground ) <= DIST_EPSILON )
    {
        return false;
    }

    for ( plan.m_brake = 1; plan.m_brake < ends[ 1 ].m_ticks; ++plan.m_brake )
    {
        found = simulate( physics, state, plan, horizon );
        if ( spares( found.m_outcome ) )
        {
            return true;
        }
    }

    plan.m_brake = horizon;
    found        = ends[ 1 ];

    return spares( found.m_outcome );
}

void fall_damage::run( )
{
#if defined( ATLAS_DEV )
    if ( this->m_expected_command >= 0 && g_cl.m_cmd->command_number > this->m_expected_command )
    {
        DBGOUT( "fall_damage::run: edge bug expected by command %d, fall is now %.1f, on ground %d",
                this->m_expected_command, -g_cl.m_local->m_vecVelocity( ).z, g_cl.m_local->IsOnGround( ) ? 1 : 0 );
        this->m_expected_command = -1;
    }
#endif

    this->m_verdict = jump_bug_verdict_t::none;

    const bool wants_jump_bug{ g_cfg.m_misc.m_jump_bug && bind_ready( g_cfg.m_misc.m_jump_bug_key ) };
    const bool wants_edge_bug{ g_cfg.m_misc.m_edge_bug && bind_ready( g_cfg.m_misc.m_edge_bug_key ) };
    const bool wants_verdict{ g_cfg.m_misc.m_jump_bug_indicator };

    if ( !wants_jump_bug && !wants_edge_bug && !wants_verdict )
    {
        return;
    }

    C_TFPlayer *local{ g_cl.m_local };

    if ( local->IsOnGround( ) ||
         local->m_MoveType( ) != MOVETYPE_WALK ||
         local->m_nWaterLevel( ) != 0 ||
         local->InCond( TF_COND_AIR_CURRENT ) ||
         local->InCond( TF_COND_SHIELD_CHARGE ) )
    {
        return;
    }

    if ( ( g_cl.m_cmd->buttons & IN_JUMP ) && !( local->m_nButtons( ) & IN_JUMP ) )
    {
        return;
    }

    const f32 gravity{ g_tf2.sv_gravity->GetFloat( ) * ( local->m_flGravity( ) != 0.0f ? local->m_flGravity( ) : 1.0f ) };
    const f32 next_fall{ gravity * TICK_INTERVAL - local->m_vecVelocity( ).z };

    if ( next_fall <= TF_PLAYER_MAX_SAFE_FALL_SPEED )
    {
        return;
    }

    fall_physics_t physics{ };
    physics.m_local        = local;
    physics.m_scale        = local->m_flModelScale( );
    physics.m_lift         = ( VEC_HULL_MAX.z - VEC_DUCK_HULL_MAX.z ) * physics.m_scale;
    physics.m_half_gravity = gravity * 0.5f * TICK_INTERVAL;
    physics.m_max_speed    = local->m_flMaxspeed( );

    math::angle_vectors( vec3{ 0.0f, g_cl.m_cmd->viewangles.y, 0.0f }, &physics.m_forward, &physics.m_right );
    physics.m_wish = physics.m_forward * g_cl.m_cmd->forwardmove + physics.m_right * g_cl.m_cmd->sidemove;

    fall_state_t state{ };
    state.m_origin    = local->GetAbsOrigin( );
    state.m_velocity  = local->m_vecVelocity( );
    state.m_ducked    = local->IsDucked( );
    state.m_held      = ( local->m_nButtons( ) & IN_DUCK ) != 0;
    state.m_air_ducks = local->m_nAirDucked( );

    if ( wants_verdict )
    {
        this->m_verdict = predict_jump_bug( physics, state );
    }

    if ( wants_jump_bug && this->jump_bug( physics, state ) )
    {
        return;
    }

    if ( wants_edge_bug )
    {
        this->edge_bug( physics, state, next_fall );
    }
}

bool fall_damage::jump_bug( const fall_physics_t &physics, const fall_state_t &state ) const
{
    if ( unduck_lands( physics, state ) )
    {
        g_cl.m_cmd->buttons &= ~IN_DUCK;
        g_cl.m_cmd->buttons |= IN_JUMP;
        return true;
    }

    fall_state_t         next{ state };
    f32                  ground{ };
    const fall_outcome_t outcome{ step( physics, next, true, physics.m_wish, ground ) };

    if ( outcome != fall_outcome_t::open || !unduck_lands( physics, next ) )
    {
        return false;
    }

    g_cl.m_cmd->buttons |= IN_DUCK;
    g_cl.m_cmd->buttons &= ~IN_JUMP;
    return true;
}

void fall_damage::edge_bug( const fall_physics_t &physics, const fall_state_t &state, f32 next_fall )
{
    if ( physics.m_max_speed < AIR_SPEED_CAP )
    {
        return;
    }

    const f32 brake{ g_tf2.sv_airaccelerate->GetFloat( ) * physics.m_max_speed * TICK_INTERVAL };
    const f32 width{ ( VEC_HULL_MAX.x - VEC_HULL_MIN.x ) * physics.m_scale };
    const i32 reach{ static_cast< i32 >( math::sqrt( 2.0f * width / ( brake * TICK_INTERVAL ) ) ) + 1 };
    const i32 horizon{ reach + static_cast< i32 >( physics.m_lift / ( next_fall * TICK_INTERVAL ) ) + 1 };

    const bool          duck{ ( g_cl.m_cmd->buttons & IN_DUCK ) != 0 };
    const fall_result_t natural{ simulate( physics, state, edge_bug_plan_t{ duck, false, 0 }, reach ) };

    if ( natural.m_outcome == fall_outcome_t::open || natural.m_outcome == fall_outcome_t::landed )
    {
        return;
    }

    if ( natural.m_outcome == fall_outcome_t::saved )
    {
#if defined( ATLAS_DEV )
        const i32 expected{ g_cl.m_cmd->command_number + natural.m_ticks - 1 };
        if ( expected != this->m_expected_command )
        {
            DBGOUT( "fall_damage::edge_bug: your input clips an edge on command %d, fall %.1f", expected, next_fall );
            this->m_expected_command = expected;
        }
#endif
        return;
    }

    edge_bug_plan_t plan{ };
    fall_result_t   found{ };
    fall_result_t   ends[ 2 ][ 2 ]{ };

    bool planned{ search( physics, state, duck, horizon, plan, found, ends[ 0 ] ) };
    if ( !planned )
    {
        planned = search( physics, state, !duck, horizon, plan, found, ends[ 1 ] );
    }

    if ( !planned )
    {
#if defined( ATLAS_DEV )
        DBGOUT( "fall_damage::edge_bug: command %d, fall %.1f, your input lands in %d at %.1f; no plan: duck kept "
                "coast %c %.1f brake %c %.1f, duck toggled coast %c %.1f brake %c %.1f",
                g_cl.m_cmd->command_number, next_fall, natural.m_ticks, natural.m_ground,
                "oslh"[ static_cast< u8 >( ends[ 0 ][ 0 ].m_outcome ) ], ends[ 0 ][ 0 ].m_ground,
                "oslh"[ static_cast< u8 >( ends[ 0 ][ 1 ].m_outcome ) ], ends[ 0 ][ 1 ].m_ground,
                "oslh"[ static_cast< u8 >( ends[ 1 ][ 0 ].m_outcome ) ], ends[ 1 ][ 0 ].m_ground,
                "oslh"[ static_cast< u8 >( ends[ 1 ][ 1 ].m_outcome ) ], ends[ 1 ][ 1 ].m_ground );
#endif
        return;
    }

    if ( plan.m_duck )
    {
        g_cl.m_cmd->buttons |= IN_DUCK;
    } else
    {
        g_cl.m_cmd->buttons &= ~IN_DUCK;
    }

    const vec3 wish{ plan.m_brake > 0 ? brake_wish( physics, state.m_velocity ) : vec3{ } };
    g_cl.m_cmd->forwardmove = physics.m_forward.dot( wish );
    g_cl.m_cmd->sidemove    = physics.m_right.dot( wish );

#if defined( ATLAS_DEV )
    const i32 expected{ g_cl.m_cmd->command_number + found.m_ticks - 1 };
    if ( expected != this->m_expected_command )
    {
        DBGOUT( "fall_damage::edge_bug: duck %d brake %d saves on command %d, your input lands in %d, fall %.1f",
                plan.m_duck ? 1 : 0, plan.m_brake, expected, natural.m_ticks, next_fall );
        this->m_expected_command = expected;
    }
#endif
}

void fall_damage::paint( ) const
{
    if ( !g_cfg.m_misc.m_jump_bug_indicator || this->m_verdict == jump_bug_verdict_t::none )
    {
        return;
    }

    if ( !g_tf2.m_engine->IsInGame( ) || !g_cl.m_local || !g_cl.m_local->IsAlive( ) || g_cl.m_local->IsOnGround( ) )
    {
        return;
    }

    const point_t at{ g_cl.m_screen_width / 2, g_cl.m_screen_height / 2 + VERDICT_OFFSET };

    if ( this->m_verdict == jump_bug_verdict_t::works )
    {
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), at, VERDICT_WORKS, ATLAS_XORSTR( "JUMP BUG" ).get( ), true,
                     render::text_align::center );
    } else
    {
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ), at, VERDICT_FAILS, ATLAS_XORSTR( "NO JUMP BUG" ).get( ), true,
                     render::text_align::center );
    }
}
