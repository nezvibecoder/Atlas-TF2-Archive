#include "move_sim.hpp"

#include "player_data.hpp"

ATLAS_NO_DESTROY move_sim g_move_sim{ };

constexpr f32 NON_GROUND_VELOCITY{ 250.0f };

static ATLAS_FORCE_INLINE f32 player_step_size( )
{
    return g_tf2.sv_stepsize->GetFloat( );
}

static ATLAS_FORCE_INLINE f32 trace_surface_friction( const trace_t &trace )
{
    if ( trace.surface.surfaceProps < 0 )
    {
        return 1.0f;
    }

    f32 friction{ 1.0f };
    g_tf2.m_physics_surface_props->GetPhysicsProperties( trace.surface.surfaceProps, nullptr, nullptr,
                                                         &friction, nullptr );
    return math::smaller( friction * 1.25f, 1.0f );
}

static ATLAS_FORCE_INLINE vec3 clip_player_velocity( const vec3 &velocity, const vec3 &normal,
                                                     f32 slide_redirect )
{
    vec3 out{ math::clip_velocity( velocity, normal ) };
    if ( slide_redirect > 0.0f )
    {
        const f32 length{ out.length( ) };
        if ( length > 0.0f )
        {
            const f32 blocked{ velocity.dot( normal ) };
            out *= ( -blocked * slide_redirect + length ) / length;
        }
    }
    return out;
}

static ATLAS_FORCE_INLINE void record_track( move_sim_ctx_t *ctx )
{
    if ( ctx->m_track_count < MAX_WALK_TICKS )
    {
        const i32 index{ ctx->m_track_count };
        ctx->m_track[ index ]     = ctx->m_pred_origin;
        ctx->m_track_air[ index ] = ctx->m_air_model;
        ++ctx->m_track_count;
    }
}

class CTraceFilterNoPlayers : public CTraceFilterSimple
{
public:
    CTraceFilterNoPlayers( C_TFPlayer *player ) : CTraceFilterSimple( player, COLLISION_GROUP_PLAYER_MOVEMENT )
    {
    }

    bool ShouldHitEntity( IHandleEntity *handle, i32 mask ) override
    {
        IClientEntity *client{ g_tf2.m_entity_list->GetClientEntityFromHandle( handle->GetRefEHandle( ) ) };
        if ( client && client->GetBaseEntity( )->GetClientClass( )->GetClassID( ) == ETFClassID::CTFPlayer )
        {
            return false;
        }

        return CTraceFilterSimple::ShouldHitEntity( handle, mask );
    }
};

move_sim_ctx_t::move_sim_ctx_t( C_TFPlayer *player_to_pred )
{
    Assert( player_to_pred != nullptr );
    atl::memset( this, 0, sizeof( move_sim_ctx_t ) );

    m_player_to_predict       = player_to_pred;
    m_has_been_predicted_once = false;
    m_prediction_broken       = false;
    m_pred_ticks              = 0;
    m_ground_agreement_ticks  = ( player_to_pred->m_fFlags( ) & FL_ONGROUND ) ? 0 : -1;
    m_pred_velocity           = m_player_to_predict->m_vecVelocity( );
    m_pred_origin             = m_player_to_predict->m_vecOrigin( );
    m_pred_mins               = m_player_to_predict->m_vecMins( );
    m_pred_maxs               = m_player_to_predict->m_vecMaxs( );
    m_pred_flags              = m_player_to_predict->m_fFlags( );
    m_pred_surface_friction   = 1.0f;
    m_pred_air_current        = m_player_to_predict->InCond( TF_COND_AIR_CURRENT );
    m_pred_lost_footing       = m_player_to_predict->InCond( TF_COND_LOST_FOOTING );

    m_max_speed = m_player_to_predict->m_flMaxspeed( );
    m_dormant   = m_player_to_predict->IsDormant( );

    if ( const velocity_sample_t *newest{
             g_player_data.velocity_sample( m_player_to_predict->entindex( ), 0 )
         }; newest && newest->m_transport_valid )
    {
        m_pred_base_velocity           = newest->m_base_velocity;
        m_pred_base_velocity.z         = 0.0f;
        m_pred_transport_ground_index  = newest->m_ground_index;
        m_pred_transport_ground_serial = newest->m_ground_serial;
        m_pred_transport_active        = true;
    }

    // a remembered target has no velocity and no input to seed from
    if ( m_dormant )
    {
        float remaining{ };
        m_prediction_broken = !g_player_data.dormant_origin( m_player_to_predict, m_pred_origin, remaining );
        m_pred_velocity     = vec3{ };
    } else
    {
        strafe_prediction::initialize( this );
    }

    m_track[ 0 ]     = m_pred_origin;
    m_track_air[ 0 ] = m_air_model;
    m_track_count    = 1;
}

void move_sim::observe_air( )
{
    if ( !g_cl.m_local )
    {
        return;
    }

    const i32 highest{ math::smaller( g_tf2.m_globals->maxClients, MAX_TRACKED_PLAYERS - 1 ) };
    const i32 team{ g_cl.m_local->m_iTeamNum( ) };
    for ( i32 index{ 1 }; index <= highest; ++index )
    {
        IClientEntity *entity{ g_tf2.m_entity_list->GetClientEntity( index ) };
        C_BaseEntity  *base{ entity ? entity->GetBaseEntity( ) : nullptr };
        if ( !base || base->IsDormant( ) || !g_player_data.velocity_sample( index, 0 ) )
        {
            continue;
        }

        C_TFPlayer *player{ base->As< C_TFPlayer * >( ) };
        if ( player->m_iTeamNum( ) == team || ( player->m_fFlags( ) & FL_ONGROUND ) || player->m_nWaterLevel( ) >= 2 )
        {
            continue;
        }

        const move_sim_ctx_t seed{ player };
    }
}

bool move_sim::run_tick( move_sim_ctx_t *ctx )
{
    Assert( ctx != nullptr );

    if ( ctx->m_prediction_broken )
    {
        return false;
    }

    // nothing to step: a remembered target has no velocity and no input
    if ( ctx->m_dormant )
    {
        ctx->m_has_been_predicted_once = true;
        ++ctx->m_pred_ticks;
        record_track( ctx );
        return true;
    }

    if ( !ctx->m_has_been_predicted_once )
    {
        if ( !check_stuck_player( ctx ) )
        {
            ctx->m_has_been_predicted_once = true;
            return false;
        }
    }

    if ( ctx->m_pred_base_velocity_impulse )
    {
        ctx->m_pred_velocity += ctx->m_pred_base_velocity * ( 1.0f + 0.5f * TICK_INTERVAL );
        ctx->m_pred_base_velocity = vec3{ };
        ctx->m_pred_base_velocity_impulse = false;
    }

    ctx->m_water_level = check_water( ctx );
    if ( ctx->m_water_control_valid && ctx->m_water_level != ctx->m_water_control_level )
    {
        ctx->m_water_control_valid = false;
        ctx->m_water_control_speed = 0.0f;
        ctx->m_water_control_direction = vec3{ };
    }
    const bool began_swimming{ ctx->m_water_level > 1 };

    if ( !ctx->m_has_been_predicted_once && ( ctx->m_pred_flags & FL_ONGROUND ) )
    {
        refresh_ground_surface( ctx );
    }

    strafe_prediction::begin_tick( ctx );

    apply_parachute_reductions( ctx );

    apply_gravity( ctx );

    check_velocity( ctx );

    if ( ctx->m_water_level <= 1 && ( ctx->m_pred_flags & FL_ONGROUND ) )
    {
        ctx->m_pred_velocity.z = 0.0f; // we are on ground, we can't possibly have .z;
        apply_friction( ctx );
    }

    if ( ctx->m_water_level > 1 )
    {
        water_move( ctx );
    } else if ( ctx->m_pred_flags & FL_ONGROUND )
    {
        walk_move( ctx );
    } else
    {
        air_move( ctx );
    }

    const bool airborne_before{ ( ctx->m_pred_flags & FL_ONGROUND ) == 0 };

    categorize_position( ctx );

    // the landing tick is a grounded tick, so the stay starts at one here, not at zero after
    if ( airborne_before && ( ctx->m_pred_flags & FL_ONGROUND ) )
    {
        strafe_prediction::on_landed( ctx );
        ctx->m_ground_ticks = 1;
    } else if ( !airborne_before && !( ctx->m_pred_flags & FL_ONGROUND ) )
    {
        strafe_prediction::on_left_ground( ctx, true );
    }

    if ( !began_swimming )
    {
        apply_gravity( ctx );
    }

    if ( ctx->m_pred_flags & FL_ONGROUND )
    {
        ctx->m_pred_velocity.z = 0.0f; // sanity check
    }

    check_velocity( ctx );

    Assert( !( ctx->m_pred_flags & FL_ONGROUND ) || ctx->m_pred_velocity.z == 0.0f );

    ctx->m_has_been_predicted_once = true;
    ++ctx->m_pred_ticks;
    record_track( ctx );

    return true;
}

void move_sim::apply_parachute_reductions( move_sim_ctx_t *ctx )
{
    Assert( ctx != nullptr );
    if ( ctx->m_pred_flags & FL_ONGROUND || ctx->m_water_level > 1 )
    {
        return;
    }

    if ( !ctx->m_player_to_predict->InCond( TF_COND_PARACHUTE_ACTIVE ) || ctx->m_pred_velocity.z >= 0.0f )
    {
        return;
    }

    ctx->m_pred_velocity.z =
        math::bigger( ctx->m_pred_velocity.z, g_tf2.tf_parachute_maxspeed_z->GetFloat( ) );

    const f32 drag{ g_tf2.tf_parachute_maxspeed_xy->GetFloat( ) };

    const f32 speed_x{ math::abs( ctx->m_pred_velocity.x ) };
    const f32 speed_y{ math::abs( ctx->m_pred_velocity.y ) };
    const f32 reduction_x{ speed_x > drag ? ( speed_x - drag ) / 3.f - 10.f : 0.f };
    const f32 reduction_y{ speed_y > drag ? ( speed_y - drag ) / 3.f - 10.f : 0.f };

    ctx->m_pred_velocity.x = math::clamp( ctx->m_pred_velocity.x, -drag - reduction_x, drag + reduction_x );
    ctx->m_pred_velocity.y = math::clamp( ctx->m_pred_velocity.y, -drag - reduction_y, drag + reduction_y );
}

// @ CGameMovement::StartGravity / FinishGravity
void move_sim::apply_gravity( move_sim_ctx_t *ctx )
{
    Assert( ctx != nullptr );
    if ( ctx->m_pred_flags & FL_ONGROUND || ctx->m_water_level > 1 )
    {
        return;
    }

    const f32 entity_gravity{ ctx->m_player_to_predict->m_flGravity( ) != 0.0f
                                  ? ctx->m_player_to_predict->m_flGravity( )
                                  : 1.0f };

    ctx->m_pred_velocity.z -=
        entity_gravity * g_tf2.sv_gravity->GetFloat( ) * 0.5f * TICK_INTERVAL; // should be correct
}

void move_sim::apply_friction( move_sim_ctx_t *ctx )
{
    Assert( ctx != nullptr );
    if ( !( ctx->m_pred_flags & FL_ONGROUND ) )
    {
        return;
    }

    strafe_prediction::ground_friction(
        ctx->m_pred_velocity,
        { TICK_INTERVAL, g_tf2.sv_friction->GetFloat( ), g_tf2.sv_stopspeed->GetFloat( ),
          g_tf2.sv_accelerate->GetFloat( ), ctx->m_max_speed, ctx->m_pred_surface_friction,
          ctx->m_pred_lost_footing } );
}

void move_sim::check_velocity( move_sim_ctx_t *ctx )
{
    Assert( ctx != nullptr );

    const f32 maximum{ g_tf2.sv_maxvelocity->GetFloat( ) };

    ctx->m_pred_velocity.x = math::clamp( ctx->m_pred_velocity.x, -maximum, maximum );
    ctx->m_pred_velocity.y = math::clamp( ctx->m_pred_velocity.y, -maximum, maximum );
    ctx->m_pred_velocity.z = math::clamp( ctx->m_pred_velocity.z, -maximum, maximum );
}

void move_sim::walk_move( move_sim_ctx_t *ctx )
{
    strafe_prediction::apply_ground_input( ctx );

    // @ CTFGameMovement::WalkMove rescales x,y to maxspeed the moment Accelerate returns
    const f32 max_speed{ strafe_prediction::movement_max_speed( ctx->m_max_speed, ctx->m_pred_lost_footing, ctx->m_charging,
                                                                g_tf2.tf_max_charge_speed->GetFloat( ) ) };
    strafe_prediction::ground_clamp_speed( ctx->m_pred_velocity, max_speed );
    if ( ctx->m_walk_keyed )
    {
        strafe_prediction::ground_clamp_back_speed( ctx->m_pred_velocity, ctx->m_walk_view_yaw, max_speed,
                                                    g_tf2.tf_clamp_back_speed->GetFloat( ),
                                                    g_tf2.tf_clamp_back_speed_min->GetFloat( ) );
    }

    const vec3 base_velocity{ ctx->m_pred_transport_active ? ctx->m_pred_base_velocity : vec3{ } };
    ctx->m_pred_velocity += base_velocity;

    // WalkMove parks a walker this slow rather than tracing them anywhere
    if ( ctx->m_pred_velocity.length( ) < 1.0f )
    {
        ctx->m_pred_velocity = vec3( );
        return;
    }

    const vec3 destination{ ctx->m_pred_origin.x + ctx->m_pred_velocity.x * TICK_INTERVAL,
                            ctx->m_pred_origin.y + ctx->m_pred_velocity.y * TICK_INTERVAL,
                            ctx->m_pred_origin.z };

    trace_t trace{ };
    perform_player_move_trace( ctx, ctx->m_pred_origin, destination, trace );

    if ( trace.fraction == 1.0f )
    {
        ctx->m_pred_origin = trace.endpos;
        ctx->m_pred_velocity -= base_velocity;
        return;
    }

    if ( ctx->m_ground_agreement_ticks >= 0 )
    {
        // The controller comparison is flat replay; it cannot certify rival collision paths.
        ctx->m_ground_agreement_ticks = math::smaller( ctx->m_ground_agreement_ticks,
                                                       static_cast< i32 >( ctx->m_pred_ticks ) );
    }
    step_move( ctx );
    ctx->m_pred_velocity -= base_velocity;
}

void move_sim::air_move( move_sim_ctx_t *ctx )
{
    strafe_prediction::apply_air_input( ctx );

    const vec3 base_velocity{ ctx->m_pred_transport_active ? ctx->m_pred_base_velocity : vec3{ } };
    ctx->m_pred_velocity += base_velocity;

    f32 slide_redirect{ };
    if ( ctx->m_pred_air_current )
    {
        slide_redirect = math::clamp( 1.0f - g_tf2.tf_movement_aircurrent_friction_mult->GetFloat( ),
                                      0.0f, 1.0f );
    }
    try_player_move( ctx, slide_redirect );
    ctx->m_pred_velocity -= base_velocity;
}

void move_sim::move_local( move_sim_ctx_t *ctx )
{
    const vec3 origin{ ctx->m_pred_origin };
    const vec3 velocity{ ctx->m_pred_velocity };
    try_player_move( ctx );
    if ( ctx->m_pred_flags & FL_ONGROUND )
    {
        const vec3 slid_origin{ ctx->m_pred_origin };
        const vec3 slid_velocity{ ctx->m_pred_velocity };
        if ( ( slid_origin - origin ).length2d_sqr( ) <
             ( velocity * TICK_INTERVAL ).length2d_sqr( ) )
        {
            ctx->m_pred_origin   = origin;
            ctx->m_pred_velocity = velocity;
            step_move( ctx );
            if ( ( slid_origin - origin ).length2d_sqr( ) >
                 ( ctx->m_pred_origin - origin ).length2d_sqr( ) )
            {
                ctx->m_pred_origin   = slid_origin;
                ctx->m_pred_velocity = slid_velocity;
            } else
            {
                ctx->m_pred_velocity.z = slid_velocity.z;
            }
        }
    }
    categorize_position( ctx );
}

void move_sim::water_move( move_sim_ctx_t *ctx )
{
    const vec3 entry{ ctx->m_pred_velocity };
    const f32  speed{ entry.length( ) };

    if ( speed > 0.0f )
    {
        const f32 drag{ TICK_INTERVAL * speed * g_tf2.sv_friction->GetFloat( ) *
                        ctx->m_pred_surface_friction };

        const f32 new_speed{ speed - drag < 0.1f ? 0.0f : speed - drag };

        ctx->m_pred_velocity *= new_speed / speed;
    }

    vec3 wish_dir{ 0.0f, 0.0f, -1.0f };
    f32  wish_speed{ 60.0f * 0.8f };
    if ( speed >= 20.0f )
    {
        wish_dir   = entry / speed;
        wish_speed = math::smaller( speed, ctx->m_max_speed * 0.8f );
    }
    if ( ctx->m_water_control_valid && ctx->m_water_level == ctx->m_water_control_level )
    {
        wish_dir = ctx->m_water_control_direction;
        wish_speed = ctx->m_water_control_speed;
    }

    if ( const f32 missing{ wish_speed - ctx->m_pred_velocity.length( ) }; missing > 0.0f )
    {
        const f32 gained{ math::smaller( g_tf2.sv_accelerate->GetFloat( ) * wish_speed *
                                             TICK_INTERVAL * ctx->m_pred_surface_friction,
                                         missing ) };

        ctx->m_pred_velocity += wish_dir * gained;
    }

    const vec3 base_velocity{ ctx->m_pred_transport_active ? ctx->m_pred_base_velocity : vec3{ } };
    ctx->m_pred_velocity += base_velocity;

    if ( ctx->m_pred_flags & FL_ONGROUND )
    {
        step_move( ctx );
    } else
    {
        try_player_move( ctx );
    }

    ctx->m_pred_velocity -= base_velocity;
}

// @ CTFGameMovement::StepMove
void move_sim::step_move( move_sim_ctx_t *ctx )
{
    const vec3 start_origin{ ctx->m_pred_origin };
    const vec3 start_velocity{ ctx->m_pred_velocity };

    const f32 step_size{ player_step_size( ) };

    vec3 raised{ start_origin };
    raised.z += step_size + DIST_EPSILON;

    trace_t trace{ };
    perform_player_move_trace( ctx, start_origin, raised, trace );
    if ( !trace.startsolid && !trace.allsolid )
    {
        ctx->m_pred_origin = trace.endpos;
    }

    try_player_move( ctx );

    vec3 lowered{ ctx->m_pred_origin };
    lowered.z -= step_size + DIST_EPSILON;

    perform_player_move_trace( ctx, ctx->m_pred_origin, lowered, trace );

    if ( !trace.startsolid && !trace.allsolid )
    {
        ctx->m_pred_origin = trace.endpos;
    }

    // the fraction belongs in this test; a cleared down trace hit nothing and the engine keeps it
    if ( ( trace.fraction != 1.0f && trace.plane.normal.z < 0.7f ) ||
         ctx->m_pred_origin == start_origin )
    {
        ctx->m_pred_origin   = start_origin;
        ctx->m_pred_velocity = start_velocity;

        try_player_move( ctx );
    }
}

void move_sim::try_player_move( move_sim_ctx_t *ctx, f32 slide_redirect )
{
    Assert( ctx != nullptr );

    constexpr i32 MAX_BUMPS{ 4 };
    constexpr i32 MAX_CLIP_PLANES{ 5 };

    vec3 planes[ MAX_CLIP_PLANES ]{ };
    i32  plane_count{ 0 };

    const vec3 primal_velocity{ ctx->m_pred_velocity };
    vec3       original_velocity{ ctx->m_pred_velocity };

    f32 time_left{ TICK_INTERVAL };
    f32 all_fraction{ 0.0f };

    for ( i32 bump{ 0 }; bump < MAX_BUMPS; ++bump )
    {
        if ( ctx->m_pred_velocity.is_zero( ) )
        {
            break;
        }

        const vec3 end{ ctx->m_pred_origin + ctx->m_pred_velocity * time_left };

        trace_t trace{ };
        perform_player_move_trace( ctx, ctx->m_pred_origin, end, trace );

        if ( trace.allsolid )
        {
            ctx->m_pred_velocity = vec3( ); // buried
            return;
        }

        all_fraction += trace.fraction;

        if ( trace.fraction > 0.0f )
        {
            if ( trace.fraction == 1.0f )
            {
                trace_t stuck{ };
                perform_player_move_trace( ctx, trace.endpos, trace.endpos, stuck );
                if ( stuck.startsolid || stuck.fraction != 1.0f )
                {
                    ctx->m_pred_velocity = vec3{ };
                    break;
                }
            }
            ctx->m_pred_origin = trace.endpos;
            original_velocity  = ctx->m_pred_velocity;
            plane_count        = 0;
        }

        if ( trace.fraction == 1.0f )
        {
            break; // moved the whole way
        }

        time_left -= time_left * trace.fraction;

        if ( plane_count >= MAX_CLIP_PLANES )
        {
            ctx->m_pred_velocity = vec3( ); // wedged
            break;
        }

        planes[ plane_count++ ] = trace.plane.normal;

        // @ CGameMovement::TryPlayerMove exempts an airborne FIRST impact from everything below
        if ( plane_count == 1 && !( ctx->m_pred_flags & FL_ONGROUND ) )
        {
            ctx->m_pred_velocity = clip_player_velocity( original_velocity, planes[ 0 ], slide_redirect );
            original_velocity    = ctx->m_pred_velocity;
            continue;
        }

        i32 slid{ 0 };
        for ( ; slid < plane_count; ++slid )
        {
            ctx->m_pred_velocity = clip_player_velocity( original_velocity, planes[ slid ], slide_redirect );

            i32 against{ 0 };
            for ( ; against < plane_count; ++against )
            {
                if ( against != slid && ctx->m_pred_velocity.dot( planes[ against ] ) < 0.0f )
                {
                    break; // this slide runs into another plane, try the next one
                }
            }

            if ( against == plane_count )
            {
                break; // clear of every other plane, take it
            }
        }

        if ( slid == plane_count )
        {
            if ( plane_count != 2 )
            {
                ctx->m_pred_velocity = vec3( );
                break;
            }

            const vec3 crease{ planes[ 0 ].cross( planes[ 1 ] ).normalized( ) };
            ctx->m_pred_velocity = crease * crease.dot( ctx->m_pred_velocity );
        }

        // a slide that turned the move back against where it started is a sloping corner;
        if ( ctx->m_pred_velocity.dot( primal_velocity ) <= 0.0f )
        {
            ctx->m_pred_velocity = vec3( );
            break;
        }
    }

    if ( all_fraction == 0.0f ) // no distance was covered at all
    {
        ctx->m_pred_velocity = vec3( );
    }
}

void move_sim::categorize_position( move_sim_ctx_t *ctx )
{
    Assert( ctx != nullptr );

    const bool was_grounded{ ( ctx->m_pred_flags & FL_ONGROUND ) != 0 };
    ctx->m_pred_surface_friction = 1.0f;
    ctx->m_water_level = check_water( ctx );

    if ( ctx->m_water_control_valid && ctx->m_water_level != ctx->m_water_control_level )
    {
        ctx->m_water_control_valid = false;
        ctx->m_water_control_speed = 0.0f;
        ctx->m_water_control_direction = vec3{ };
    }

    if ( ctx->m_pred_velocity.z > NON_GROUND_VELOCITY )
    {
        ctx->m_pred_flags &= ~FL_ONGROUND;
        if ( ctx->m_pred_transport_active )
        {
            ctx->m_pred_transport_active = false;
            ctx->m_pred_base_velocity_impulse = true;
        }
        return;
    }

    vec3 probe{ ctx->m_pred_origin.x, ctx->m_pred_origin.y, ctx->m_pred_origin.z - 2.0f };
    const bool retain_ground{ was_grounded && ctx->m_water_level < 3 };
    if ( retain_ground )
    {
        probe.z -= player_step_size( );
    }

    trace_t trace{ };
    trace_ground( ctx, probe, trace );
    trace_t ground_trace{ trace };
    const bool main_trace_walkable{ trace.fraction != 1.0f && trace.plane.normal.z >= 0.7f };

    bool  in_air{ false };
    bool  steep_air{ false };
    f32   ground_friction_multiplier{ 1.0f };
    if ( ctx->m_pred_lost_footing )
    {
        const f32 away{ ctx->m_pred_velocity.dot( trace.plane.normal ) };
        if ( away > 0.0f )
        {
            in_air = true;
        } else
        {
            const vec3 along{ ctx->m_pred_velocity - trace.plane.normal * away };
            const f32 restick{ g_tf2.tf_movement_lost_footing_restick->GetFloat( ) };
            if ( along.length( ) >= restick )
            {
                ground_friction_multiplier = g_tf2.tf_movement_lost_footing_friction->GetFloat( );
            } else
            {
                ctx->m_pred_lost_footing = false;
            }
        }
    }

    // slope > 45dg = not standable
    if ( !in_air && ( trace.fraction == 1.0f || trace.plane.normal.z < 0.7f ) )
    {
        if ( !touch_ground_in_quadrants( ctx, ctx->m_pred_origin, probe, ground_trace ) )
        {
            in_air = true;
            steep_air = true;
        }
    }

    if ( in_air || !ground_trace.m_pEnt )
    {
        ctx->m_pred_flags &= ~FL_ONGROUND;
        if ( ctx->m_pred_transport_active )
        {
            ctx->m_pred_transport_active = false;
            ctx->m_pred_base_velocity_impulse = true;
        }
        if ( steep_air && ctx->m_pred_velocity.z > 0.0f )
        {
            ctx->m_pred_surface_friction = 0.25f;
        }
        if ( ctx->m_pred_air_current )
        {
            ctx->m_pred_surface_friction *= g_tf2.tf_movement_aircurrent_friction_mult->GetFloat( );
        }
        return;
    }

    if ( ctx->m_pred_transport_active )
    {
        auto       *support_entity{ ground_trace.m_pEnt };
        const auto  support{ support_entity->GetRefEHandle( ) };
        auto       *support_class{ support_entity->GetClientClass( ) };
        const bool stable_conveyor{
            support_class && support_class->GetClassID( ) == ETFClassID::CFuncConveyor &&
            !support_entity->m_hMoveParent( ).IsValid( )
        };
        if ( !support.IsValid( ) || support.GetEntryIndex( ) != ctx->m_pred_transport_ground_index ||
             support.GetSerialNumber( ) != ctx->m_pred_transport_ground_serial || !stable_conveyor )
        {
            ctx->m_pred_transport_active = false;
            ctx->m_prediction_broken = true;
        }
    }

    ctx->m_pred_flags |= FL_ONGROUND;
    ctx->m_pred_velocity.z = 0.0f; // @ CGameMovement::SetGroundEntity
    ctx->m_pred_air_current = false; // ConditionThink clears TF_COND_AIR_CURRENT on ground contact.
    ctx->m_pred_surface_friction = trace_surface_friction( ground_trace ) * ground_friction_multiplier;

    if ( retain_ground && main_trace_walkable && trace.fraction > 0.0f && trace.fraction < 1.0f &&
         !trace.startsolid )
    {
        const f32 delta{ math::abs( ctx->m_pred_origin.z - trace.endpos.z ) };
        if ( delta > 0.5f * ( 1.0f / 16.0f ) )
        {
            ctx->m_pred_origin.z = trace.endpos.z;
        }
    }
}

// TODO?: use TFGameRules for hulls.
bool move_sim::touch_ground_in_quadrants( move_sim_ctx_t *ctx, const vec3 &start, const vec3 &end,
                                          trace_t &ground_trace )
{
    const vec3 mins{ ctx->m_pred_mins };
    const vec3 maxs{ ctx->m_pred_maxs };
    const f32  original_fraction{ ground_trace.fraction };
    const vec3 original_endpos{ ground_trace.endpos };

    const vec3 quadrant_mins[ 4 ]{
        mins,
        vec3( math::bigger( 0.0f, mins.x ), math::bigger( 0.0f, mins.y ), mins.z ),
        vec3( mins.x, math::bigger( 0.0f, mins.y ), mins.z ),
        vec3( math::bigger( 0.0f, mins.x ), mins.y, mins.z ),
    };

    const vec3 quadrant_maxs[ 4 ]{
        vec3( math::smaller( 0.0f, maxs.x ), math::smaller( 0.0f, maxs.y ), maxs.z ),
        maxs,
        vec3( math::smaller( 0.0f, maxs.x ), maxs.y, maxs.z ),
        vec3( maxs.x, math::smaller( 0.0f, maxs.y ), maxs.z ),
    };

    for ( i32 quadrant{ 0 }; quadrant < 4; ++quadrant )
    {
        trace_t trace{ };
        perform_hull_trace( ctx, start, end, quadrant_mins[ quadrant ],
                            quadrant_maxs[ quadrant ], trace );

        if ( trace.m_pEnt && trace.plane.normal.z >= 0.7f )
        {
            ground_trace          = trace;
            ground_trace.fraction = original_fraction;
            ground_trace.endpos   = original_endpos;
            return true;
        }
    }

    return false;
}

void move_sim::refresh_ground_surface( move_sim_ctx_t *ctx )
{
    Assert( ctx != nullptr );

    vec3 end{ ctx->m_pred_origin.x, ctx->m_pred_origin.y,
              ctx->m_pred_origin.z - 2.0f - player_step_size( ) };
    trace_t trace{ };
    trace_ground( ctx, end, trace );
    trace_t ground_trace{ trace };
    if ( ( trace.fraction == 1.0f || trace.plane.normal.z < 0.7f ) &&
         !touch_ground_in_quadrants( ctx, ctx->m_pred_origin, end, ground_trace ) )
    {
        return;
    }

    f32 multiplier{ 1.0f };
    if ( ctx->m_pred_lost_footing )
    {
        const f32 away{ ctx->m_pred_velocity.dot( trace.plane.normal ) };
        const vec3 along{ ctx->m_pred_velocity - trace.plane.normal * away };
        const f32 restick{ g_tf2.tf_movement_lost_footing_restick->GetFloat( ) };
        if ( away <= 0.0f && along.length( ) >= restick )
        {
            multiplier = g_tf2.tf_movement_lost_footing_friction->GetFloat( );
        } else if ( away <= 0.0f )
        {
            ctx->m_pred_lost_footing = false;
        }
    }

    ctx->m_pred_surface_friction = trace_surface_friction( ground_trace ) * multiplier;
    if ( ctx->m_pred_surface_friction != 1.0f && ctx->m_ground_agreement_ticks >= 0 )
    {
        ctx->m_ground_agreement_ticks = 0; // historical replay had neutral remote surface friction
    }
}

void move_sim::trace_ground( move_sim_ctx_t *ctx, const vec3 &end, trace_t &trace )
{
    move_sim_queries_t *queries{ ctx->m_queries };
    if ( queries && queries->m_has_ground && queries->m_ground_origin == ctx->m_pred_origin &&
         queries->m_ground_end == end )
    {
        trace = queries->m_ground;
        return;
    }
    perform_player_move_trace( ctx, ctx->m_pred_origin, end, trace );
    if ( queries )
    {
        queries->m_ground_origin = ctx->m_pred_origin;
        queries->m_ground_end    = end;
        queries->m_ground        = trace;
        queries->m_has_ground    = true;
    }
}

i32 move_sim::check_water( move_sim_ctx_t *ctx )
{
    move_sim_queries_t *queries{ ctx->m_queries };
    if ( queries && queries->m_has_water && queries->m_water_origin == ctx->m_pred_origin )
    {
        return queries->m_water_level;
    }
    CTraceFilterWorldOnly filter{ };

    const auto submerged{ [ & ]( f32 height ) -> bool
    {
        const vec3 point{ ctx->m_pred_origin.x, ctx->m_pred_origin.y, height };

        trace_t trace{ };
        Ray_t   ray{ };

        ray.Init( point, point );
        g_tf2.m_trace->TraceRay( ray, MASK_WATER, &filter, &trace );

        return trace.startsolid || trace.allsolid;
    } };

    i32 level{ 0 };

    if ( submerged( ctx->m_pred_origin.z + ctx->m_pred_mins.z + 1.0f ) )
    {
        level = 1;

        if ( submerged( ctx->m_pred_origin.z + ctx->m_player_to_predict->m_vecViewOffset( ).z ) )
        {
            level = 3;
        } else if ( submerged( ctx->m_pred_origin.z +
                               ( ctx->m_pred_mins.z + ctx->m_pred_maxs.z ) * 0.5f + 12.0f ) )
        {
            level = 2;
        }
    }

    if ( queries )
    {
        queries->m_water_origin = ctx->m_pred_origin;
        queries->m_water_level  = level;
        queries->m_has_water    = true;
    }
    return level;
}

bool move_sim::check_stuck_player( move_sim_ctx_t *ctx )
{
    Assert( ctx != nullptr );
    trace_t trace{ };

    perform_player_move_trace( ctx, ctx->m_pred_origin, ctx->m_pred_origin, trace );

    if ( !trace.startsolid )
    {
        return true;
    }

    // @::CreateStuckTable
    constexpr f32         NEAR{ 0.125f };
    constexpr f32         FAR{ 2.0f };
    static constexpr vec3 STUCK_OFFSETS[]{
        { 0.f, 0.f, NEAR },
        { 0.f, 0.f, -NEAR },
        { NEAR, 0.f, 0.f },
        { -NEAR, 0.f, 0.f },
        { 0.f, NEAR, 0.f },
        { 0.f, -NEAR, 0.f },

        { NEAR, NEAR, NEAR },
        { -NEAR, NEAR, NEAR },
        { NEAR, -NEAR, NEAR },
        { -NEAR, -NEAR, NEAR },
        { NEAR, NEAR, -NEAR },
        { -NEAR, NEAR, -NEAR },
        { NEAR, -NEAR, -NEAR },
        { -NEAR, -NEAR, -NEAR },

        { 0.f, 0.f, 1.f },
        { 0.f, 0.f, 6.f },
        { FAR, 0.f, 0.f },
        { -FAR, 0.f, 0.f },
        { 0.f, FAR, 0.f },
        { 0.f, -FAR, 0.f },

        { FAR, FAR, 1.f },
        { -FAR, FAR, 1.f },
        { FAR, -FAR, 1.f },
        { -FAR, -FAR, 1.f },
        { FAR, FAR, 6.f },
        { -FAR, FAR, 6.f },
        { FAR, -FAR, 6.f },
        { -FAR, -FAR, 6.f },
    };

    for ( const vec3 &test : STUCK_OFFSETS )
    {
        perform_player_move_trace( ctx, ctx->m_pred_origin + test, ctx->m_pred_origin, trace );

        // an offset only helps when it is NOT stuck
        if ( trace.startsolid )
        {
            continue;
        }

        ctx->m_pred_origin = ctx->m_pred_origin + test;
        return true;
    }

    ctx->m_prediction_broken = true; // mission failed, we'll get'em next time
    return false;
}

void move_sim::perform_player_move_trace(
    C_TFPlayer *player,
    const vec3 &origin,
    const vec3 &destination,
    trace_t    &tr_out )
{
    perform_hull_trace( player, origin, destination, player->m_vecMins( ), player->m_vecMaxs( ),
                        tr_out );
}

void move_sim::perform_hull_trace(
    C_TFPlayer *player,
    const vec3 &origin,
    const vec3 &destination,
    const vec3 &mins,
    const vec3 &maxs,
    trace_t    &tr_out )
{
    CTraceFilterSimple filter{ player, COLLISION_GROUP_PLAYER_MOVEMENT };
    Ray_t              ray{ };

    ray.Init( origin, destination, mins, maxs );
    g_tf2.m_trace->TraceRay( ray, player_solid_mask( player ), &filter, &tr_out );
}

u32 move_sim::player_solid_mask( C_TFPlayer *player ) const
{
    u32 mask{ MASK_PLAYERSOLID };

    if ( player->IsGhost( ) )
    {
        mask = MASK_PLAYERSOLID_BRUSHONLY;
    } else
    {
        switch ( player->m_iTeamNum( ) )
        {
            case TF_TEAM_RED: mask |= CONTENTS_TEAM2; break;
            case TF_TEAM_BLUE: mask |= CONTENTS_TEAM1; break;
            default: break;
        }
    }

    return mask;
}

void move_sim::perform_player_move_trace(
    move_sim_ctx_t *ctx,
    const vec3     &origin,
    const vec3     &destination,
    trace_t        &tr_out )
{
    perform_hull_trace( ctx, origin, destination, ctx->m_pred_mins, ctx->m_pred_maxs, tr_out );
}

void move_sim::perform_hull_trace(
    move_sim_ctx_t *ctx,
    const vec3     &origin,
    const vec3     &destination,
    const vec3     &mins,
    const vec3     &maxs,
    trace_t        &tr_out )
{
    C_TFPlayer *player{ ctx->m_player_to_predict };
    if ( !ctx->m_players )
    {
        perform_hull_trace( player, origin, destination, mins, maxs, tr_out );
        return;
    }

    CTraceFilterNoPlayers filter{ player };
    Ray_t                 ray{ };
    const u32             mask{ player_solid_mask( player ) };

    ray.Init( origin, destination, mins, maxs );
    g_tf2.m_trace->TraceRay( ray, mask, &filter, &tr_out );

    for ( i32 index{ }; index < ctx->m_player_count; ++index )
    {
        const move_sim_player_t &other{ ctx->m_players[ index ] };
        const vec3               shift{ other.m_player->GetAbsOrigin( ) - other.m_origin };

        Ray_t   moved{ };
        trace_t clip{ };
        moved.Init( origin + shift, destination + shift, mins, maxs );
        g_tf2.m_trace->ClipRayToEntity( moved, mask, other.m_player, &clip );
        if ( !clip.allsolid && !clip.startsolid && clip.fraction >= tr_out.fraction )
        {
            continue;
        }

        const bool startsolid{ tr_out.startsolid };
        tr_out            = clip;
        tr_out.startpos   = clip.startpos - shift;
        tr_out.endpos     = clip.endpos - shift;
        tr_out.startsolid = startsolid || clip.startsolid;
        tr_out.m_pEnt     = other.m_player;
    }
}
