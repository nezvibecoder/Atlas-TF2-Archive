#include "ragebot.hpp"
#include "entity_cache.hpp"
#include "runtime/thread_pool.hpp"

#include "auto_vacc.hpp"
#include "crits.hpp"
#include "extrapolate.hpp"
#include "holdaim_mgr.hpp"
#include "lagcomp.hpp"
#include "move_sim.hpp"
#include "networking.hpp"
#include "nospread.hpp"
#include "player_data.hpp"
#include "proj_sim.hpp"
#include "resolver.hpp"
#include "tickbase.hpp"

#include "render/render.hpp"

namespace ballistics
{
    inline i32 arrival_tick( f32 time, f32 dt )
    {
        const i32 tick{ static_cast< i32 >( time / dt ) };
        return math::bigger( tick + ( time > tick * dt ? 1 : 0 ), 1 );
    }
    inline f64 square_root( f64 value )
    {
        return _mm_cvtsd_f64( _mm_sqrt_sd( _mm_setzero_pd( ), _mm_set_sd( value ) ) );
    }

    inline f64 absolute( f64 value )
    {
        return value < 0.0 ? -value : value;
    }

    inline bool flight_time( const vec3 &delta, f32 speed, f32 gravity, bool high, f32 &time )
    {
        if ( speed <= 0.0f )
        {
            return false;
        }

        const f64 distance{ static_cast< f64 >( delta.x ) * delta.x + static_cast< f64 >( delta.y ) * delta.y + static_cast< f64 >( delta.z ) * delta.z };
        if ( gravity <= 0.0f )
        {
            time = static_cast< f32 >( square_root( distance ) / speed );
            return !high;
        }

        const f64 velocity{ static_cast< f64 >( speed ) * speed };
        const f64 term{ velocity - static_cast< f64 >( gravity ) * delta.z };
        const f64 product{ static_cast< f64 >( gravity ) * gravity * distance };
        const f64 discriminant{ term * term - product };
        const f64 tolerance{ 16.0 * 2.2204460492503131e-16 * ( term * term + product ) };
        if ( discriminant < -tolerance || term <= 0.0 )
        {
            return false;
        }

        const f64 root{ square_root( math::bigger( discriminant, 0.0 ) ) };
        const f64 square{ high ? 2.0 * ( term + root ) / ( static_cast< f64 >( gravity ) * gravity ) : 2.0 * distance / ( term + root ) };
        time = static_cast< f32 >( square_root( square ) );
        return true;
    }

    // range per degree of view
    inline f32 reach_slope( f32 speed, f32 gravity, f32 rise, f32 distance, f32 elevation, bool high )
    {
        const f64  velocity{ static_cast< f64 >( speed ) * speed };
        const auto reach{ [ & ]( f64 angle )
        {
            f64 sine{ }, cosine{ };
            math::sincos( angle, sine, cosine );
            const f64 root{ sine * sine - 2.0 * gravity * rise / velocity };
            return root < 0.0 ? -1.0 : velocity * cosine * ( sine + square_root( root ) ) / gravity;
        } };

        constexpr f64 STEP{ 0.25 * 3.14159265358979 / 180.0 };
        const f64     above{ reach( elevation + STEP ) }, below{ reach( elevation - STEP ) };
        const f32     fallback{ high ? distance * 0.02f : -distance * 0.02f };
        if ( above < 0.0 || below < 0.0 )
        {
            return fallback;
        }
        const f32 slope{ static_cast< f32 >( ( below - above ) / 0.5 ) };
        return ( high ? slope > 0.0f : slope < 0.0f ) ? slope : fallback;
    }

    inline f32 initial_horizon( const vec3 &delta, f32 speed, f32 gravity, f32 maximum )
    {
        if ( speed <= 0.0f )
        {
            return maximum;
        }
        f32 time{ };
        if ( !flight_time( delta, speed, gravity, false, time ) )
        {
            time = delta.length( ) / speed;
        }

        return math::smaller( maximum, math::bigger( 0.25f, time * 1.25f + 0.1f ) );
    }

    inline vec3 target_at( const vec3 *path, i32 count, f32 tick )
    {
        tick = math::clamp( tick, 0.0f, static_cast< f32 >( count - 1 ) );
        const i32 at{ static_cast< i32 >( tick ) };
        return at + 1 < count ? path[ at ] + ( path[ at + 1 ] - path[ at ] ) * ( tick - at ) : path[ at ];
    }

    inline f64 evaluate( const f64 *coefficients, i32 degree, f64 value )
    {
        f64 result{ coefficients[ degree ] };
        for ( i32 at{ degree - 1 }; at >= 0; --at )
        {
            result = result * value + coefficients[ at ];
        }
        return result;
    }

    inline i32 roots( const f64 *coefficients, i32 degree, f64 *out )
    {
        while ( degree > 0 && coefficients[ degree ] == 0.0 )
        {
            --degree;
        }
        if ( degree == 0 )
        {
            return 0;
        }
        if ( degree == 1 )
        {
            const f64 root{ -coefficients[ 0 ] / coefficients[ 1 ] };
            if ( root < 0.0 || root > 1.0 )
            {
                return 0;
            }
            out[ 0 ] = root;
            return 1;
        }
        f64 derivative[ 4 ]{ }, boundaries[ 5 ]{ };
        f64 scale{ }, variation{ };
        for ( i32 at{ }; at <= degree; ++at )
        {
            scale += absolute( coefficients[ at ] );
            if ( at )
            {
                variation += absolute( coefficients[ at ] );
                derivative[ at - 1 ] = at * coefficients[ at ];
            }
        }
        const f64 tolerance{ math::bigger( scale, 1.0 ) * 1.0e-12 };
        // On [0,1], the nonconstant terms cannot move the value by more than this sum.
        if ( absolute( coefficients[ 0 ] ) > variation + tolerance )
        {
            return 0;
        }
        const i32 stationary{ roots( derivative, degree - 1, boundaries + 1 ) };
        boundaries[ stationary + 1 ] = 1.0;
        i32        count{ };
        const auto append{ [ & ]( f64 value )
        {
            if ( count < degree && ( !count || value - out[ count - 1 ] > 1.0e-7 ) )
            {
                out[ count++ ] = value;
            }
        } };
        for ( i32 at{ }; at <= stationary; ++at )
        {
            f64       low{ boundaries[ at ] }, high{ boundaries[ at + 1 ] };
            f64       left{ evaluate( coefficients, degree, low ) };
            const f64 right{ evaluate( coefficients, degree, high ) };
            if ( absolute( left ) <= tolerance )
            {
                append( low );
            }
            if ( absolute( left ) > tolerance && absolute( right ) > tolerance && ( left < 0.0 ) != ( right < 0.0 ) )
            {
                for ( i32 step{ }; step < 24; ++step )
                {
                    const f64 middle{ ( low + high ) * 0.5 };
                    const f64 value{ evaluate( coefficients, degree, middle ) };
                    if ( ( value < 0.0 ) == ( left < 0.0 ) )
                    {
                        low  = middle;
                        left = value;
                    } else
                    {
                        high = middle;
                    }
                }
                append( ( low + high ) * 0.5 );
            }
            if ( absolute( right ) <= tolerance )
            {
                append( high );
            }
        }
        return count;
    }

    inline vec3 refine_step( const vec3 &angle, const vec3 &error, const vec3 &dx, const vec3 &dy )
    {
        const f64 xx{ dx.dot( dx ) }, xy{ dx.dot( dy ) }, yy{ dy.dot( dy ) };
        const f64 ex{ error.dot( dx ) }, ey{ error.dot( dy ) };
        if ( xx + yy <= 1.0e-8 )
        {
            return angle;
        }
        const f64 damping{ ( xx + yy ) * 0.0001 };
        const f64 determinant{ ( xx + damping ) * ( yy + damping ) - xy * xy };
        vec3      next{ angle };
        next.x += math::clamp( static_cast< f32 >( ( xy * ey - ( yy + damping ) * ex ) / determinant ), -12.0f, 12.0f );
        next.y += math::clamp( static_cast< f32 >( ( xy * ex - ( xx + damping ) * ey ) / determinant ), -12.0f, 12.0f );
        return next;
    }

    // eye + 16 F + side R - 6 U; velocity = speed F + right R + up U
    inline i32 sticky_intercepts( const vec3 &eye, const vec3 *path, i32 count, const vec3 &offset,
                                  f32 dt, f32 speed, f32 up, f32 right, f32 side, f32 gravity,
                                  intercept_t *out, i32 capacity )
    {
        const f64 magnitude{ square_root( static_cast< f64 >( speed ) * speed + static_cast< f64 >( up ) * up + static_cast< f64 >( right ) * right ) };
        const f64 cap{ magnitude > 2000.0 ? 2000.0 / magnitude : 1.0 };
        const f64 forward_speed{ speed * cap }, up_speed{ up * cap }, right_speed{ right * cap };
        i32       written{ };
        for ( i32 at{ }; at + 1 < count && written < capacity; )
        {
            i32        end{ at + 1 };
            const vec3 step{ path[ end ] - path[ at ] };
            while ( end + 1 < count && path[ end + 1 ] - path[ end ] == step )
            {
                ++end;
            }
            const f64  interval{ static_cast< f64 >( end - at ) * dt };
            const f64  time{ static_cast< f64 >( at ) * dt };
            const vec3 origin{ path[ at ] + offset - eye };
            const vec3 move{ path[ end ] - path[ at ] };
            const f64  z{ origin.z + 0.5 * gravity * time * time };
            const f64  dz{ move.z + gravity * time * interval };
            const f64  ddz{ 0.5 * gravity * interval * interval };
            const f64  a{ 16.0 + forward_speed * time }, da{ forward_speed * interval };
            const f64  b{ side + right_speed * time }, db{ right_speed * interval };
            const f64  c{ -6.0 + up_speed * time }, dc{ up_speed * interval };
            const f64  coefficients[]{
                static_cast< f64 >( origin.x ) * origin.x + static_cast< f64 >( origin.y ) * origin.y + z * z - a * a - b * b - c * c,
                2.0 * ( static_cast< f64 >( origin.x ) * move.x + static_cast< f64 >( origin.y ) * move.y + z * dz - a * da - b * db - c * dc ),
                static_cast< f64 >( move.x ) * move.x + static_cast< f64 >( move.y ) * move.y + dz * dz + 2.0 * z * ddz - da * da - db * db - dc * dc,
                2.0 * dz * ddz,
                ddz * ddz
            };
            f64       times[ 4 ]{ };
            const i32 found{ roots( coefficients, 4, times ) };
            for ( i32 root{ }; root < found && written < capacity; ++root )
            {
                const f32 arrival{ static_cast< f32 >( time + times[ root ] * interval ) };
                if ( arrival <= 0.000001f || ( written && arrival - out[ written - 1 ].m_time < 0.00001f ) )
                {
                    continue;
                }
                const vec3 target{ target_at( path, count, arrival / dt ) + offset - eye };
                const f64  height{ target.z + 0.5 * gravity * arrival * arrival };
                const f64  lateral{ side + right_speed * arrival };
                const f64  horizontal_sqr{ static_cast< f64 >( target.x ) * target.x + static_cast< f64 >( target.y ) * target.y - lateral * lateral };
                if ( horizontal_sqr < -0.000001 )
                {
                    continue; // yaw cannot put the side offset inside this horizontal radius
                }
                const f32 horizontal{ static_cast< f32 >( square_root( math::bigger( horizontal_sqr, 0.0 ) ) ) };
                const f32 tilt{ math::atan2( static_cast< f32 >( -6.0 + up_speed * arrival ), static_cast< f32 >( 16.0 + forward_speed * arrival ) ) };
                for ( i32 branch{ }; branch < 2 && written < capacity; ++branch )
                {
                    const f32 along{ branch ? -horizontal : horizontal };
                    const f32 pitch{ RAD2DEG( tilt - math::atan2( static_cast< f32 >( height ), along ) ) };
                    if ( branch && ( horizontal < 0.00001f || pitch < -89.0f || pitch > 89.0f ) )
                    {
                        continue;
                    }
                    const f32 yaw{ math::atan2( target.y, target.x ) + math::atan2( static_cast< f32 >( lateral ), along ) };
                    out[ written++ ] = intercept_t{ vec3( pitch, RAD2DEG( yaw ), 0.0f ), arrival };
                }
            }
            at = end;
        }
        return written;
    }
} // namespace ballistics

ATLAS_NO_DESTROY ragebot g_ragebot{ };

inline bool is_weapon_sniper_rifle( C_TFWeaponBase *wep )
{
    switch ( wep->GetWeaponID( ) )
    {
        case TF_WEAPON_SNIPERRIFLE_CLASSIC:
        case TF_WEAPON_SNIPERRIFLE:
        case TF_WEAPON_SNIPERRIFLE_DECAP: return true;
        default: return false;
    }
}

inline bool rezoom_cycle_pending( C_TFSniperRifle *rifle )
{
    return rifle->m_flUnzoomTime( ) > 0.0f || rifle->m_flRezoomTime( ) > 0.0f;
}

inline bool is_weapon_ambassador( C_TFWeaponBase *wep )
{
    if ( wep->GetWeaponID( ) != TF_WEAPON_REVOLVER )
    {
        return false;
    }

    return wep->As< C_TFRevolver * >( )->CanHeadshot( );
}

inline bool is_blacklisted_weapon( C_TFWeaponBase *wep )
{
    // TODO: Call the attrib hook for this instead.
    switch ( wep->m_iItemDefinitionIndex( ) )
    {
        case Soldier_m_RocketJumper:
        case Demoman_s_StickyJumper: return true;
        default: break;
    }

    switch ( wep->GetWeaponID( ) )
    {
        case TF_WEAPON_MEDIGUN:
        case TF_WEAPON_GRAPPLINGHOOK:
        case TF_WEAPON_LASER_POINTER:
        case TF_WEAPON_PDA:
        case TF_WEAPON_PDA_SPY:
        case TF_WEAPON_PDA_SPY_BUILD:
        case TF_WEAPON_PDA_ENGINEER_BUILD:
        case TF_WEAPON_PDA_ENGINEER_DESTROY:
        case TF_WEAPON_PASSTIME_GUN: return true;
        default: break;
    }

    return false;
}

inline bool pre_charges_launcher( )
{
    if ( !g_cfg.m_ragebot.m_preferences[ config::PREFERENCE_AUTO_CHARGE ] )
    {
        return false;
    }

    switch ( g_cl.m_weapon->m_iItemDefinitionIndex( ) )
    {
        case Demoman_s_TheScottishResistance: return g_cfg.m_ragebot.m_auto_charge_sticky_targets[ config::CHARGE_ON_SCOTTISH ];
        case Demoman_s_TheQuickiebombLauncher: return g_cfg.m_ragebot.m_auto_charge_sticky_targets[ config::CHARGE_ON_QUICKIEBOMB ];
        default: break;
    }

    if ( g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_PIPEBOMBLAUNCHER )
    {
        return g_cfg.m_ragebot.m_auto_charge_sticky_targets[ config::CHARGE_ON_STICKYBOMB ];
    }

    return true;
}

inline bool is_vaccinator_bait( C_TFPlayer *player )
{
    if ( player->m_iClass( ) != TF_CLASS_MEDIC || !player->IsBulletResistant( ) )
    {
        return false;
    }

    C_BaseEntity *weapon_entity{ player->m_hActiveWeapon( ).Get( ) };
    if ( !weapon_entity ||
         weapon_entity->As< C_TFWeaponBase * >( )->m_iItemDefinitionIndex( ) != Medic_s_TheVaccinator )
    {
        return false;
    }

    C_BaseEntity *heal_entity{ weapon_entity->As< C_WeaponMedigun * >( )->m_hHealingTarget( ).Get( ) };
    if ( !heal_entity )
    {
        return false;
    }

    ClientClass *heal_class{ heal_entity->GetClientClass( ) };
    if ( !heal_class || heal_class->GetClassID( ) != ETFClassID::CTFPlayer )
    {
        return false;
    }

    C_TFPlayer *pocket{ heal_entity->As< C_TFPlayer * >( ) };
    if ( pocket->m_iClass( ) != TF_CLASS_HEAVYWEAPONS || !pocket->IsSlowed( ) )
    {
        return false;
    }

    C_BaseEntity *pocket_weapon{ pocket->m_hActiveWeapon( ).Get( ) };
    return pocket_weapon && pocket_weapon->As< C_TFWeaponBase * >( )->GetWeaponID( ) == TF_WEAPON_MINIGUN;
}

// a point on the predicted bounding box; the netvar bounds already carry the model scale
ATLAS_FORCE_INLINE vec3 bbox_point( const move_sim_ctx_t *ctx, i32 bbox_pos )
{
    const f32   maxs_z{ ctx->m_pred_maxs.z };
    const vec3 &origin{ ctx->m_pred_origin };
    const f32   base{ origin.z + ctx->m_pred_mins.z };

    switch ( bbox_pos )
    {
        case bbox_pos_top: return vec3( origin.x, origin.y, base + maxs_z * 0.98f );
        case bbox_pos_mid: return vec3( origin.x, origin.y, base + maxs_z * 0.55f );
        case bbox_pos_bottom: return vec3( origin.x, origin.y, base + maxs_z * 0.15f );
        default: return origin;
    }
}

ATLAS_FORCE_INLINE vec3 arrow_cell_point( const arrow_grid_t &grid, i32 cell )
{
    const f32 half{ static_cast< f32 >( ARROW_GRID_SIZE - 1 ) * 0.5f };
    const f32 across{ static_cast< f32 >( cell % ARROW_GRID_SIZE ) - half };
    const f32 down{ static_cast< f32 >( cell / ARROW_GRID_SIZE ) - half };

    return grid.m_centre + grid.m_right * ( across * grid.m_cell ) - grid.m_up * ( down * grid.m_cell );
}

ATLAS_FORCE_INLINE bool arrow_cell_entry( const arrow_grid_t &grid, i32 cell, vec3 &entry )
{
    const vec3 point{ arrow_cell_point( grid, cell ) };
    const f32  fraction{ math::segment_box_fraction( point - grid.m_reach, point + grid.m_reach, grid.m_low, grid.m_high ) };
    if ( fraction == FLT_MAX )
    {
        return false;
    }

    entry = point - grid.m_reach + grid.m_reach * ( fraction * 2.0f );
    return true;
}

ATLAS_FORCE_INLINE i32 deepest_head_cell( arrow_grid_t &grid )
{
    constexpr i32 SIZE{ ARROW_GRID_SIZE };
    constexpr i32 STRAIGHT{ 3 };
    constexpr i32 DIAGONAL{ 4 };

    for ( i32 row{ }; row < SIZE; ++row )
    {
        for ( i32 column{ }; column < SIZE; ++column )
        {
            const i32 cell{ row * SIZE + column };
            if ( grid.m_cells[ cell ] != arrow_cell_head )
            {
                grid.m_depth[ cell ] = 0;
                continue;
            }

            const i32 left{ column > 0 ? grid.m_depth[ cell - 1 ] : 0 };
            const i32 above{ row > 0 ? grid.m_depth[ cell - SIZE ] : 0 };
            const i32 above_left{ row > 0 && column > 0 ? grid.m_depth[ cell - SIZE - 1 ] : 0 };
            const i32 above_right{ row > 0 && column + 1 < SIZE ? grid.m_depth[ cell - SIZE + 1 ] : 0 };

            grid.m_depth[ cell ] = math::smaller( math::smaller( left, above ) + STRAIGHT, math::smaller( above_left, above_right ) + DIAGONAL );
        }
    }

    grid.m_deepest = 0;
    for ( i32 row{ SIZE - 1 }; row >= 0; --row )
    {
        for ( i32 column{ SIZE - 1 }; column >= 0; --column )
        {
            const i32 cell{ row * SIZE + column };
            if ( grid.m_cells[ cell ] != arrow_cell_head )
            {
                continue;
            }

            const i32 right{ column + 1 < SIZE ? grid.m_depth[ cell + 1 ] : 0 };
            const i32 below{ row + 1 < SIZE ? grid.m_depth[ cell + SIZE ] : 0 };
            const i32 below_right{ row + 1 < SIZE && column + 1 < SIZE ? grid.m_depth[ cell + SIZE + 1 ] : 0 };
            const i32 below_left{ row + 1 < SIZE && column > 0 ? grid.m_depth[ cell + SIZE - 1 ] : 0 };
            const i32 straight{ math::smaller( right, below ) + STRAIGHT };
            const i32 diagonal{ math::smaller( below_right, below_left ) + DIAGONAL };

            grid.m_depth[ cell ] = math::smaller( grid.m_depth[ cell ], math::smaller( straight, diagonal ) );
            grid.m_deepest       = math::bigger( grid.m_deepest, grid.m_depth[ cell ] );
        }
    }

    if ( grid.m_deepest == 0 )
    {
        return -1;
    }

    i32 count{ }, columns{ }, rows{ };
    for ( i32 cell{ }; cell < SIZE * SIZE; ++cell )
    {
        if ( grid.m_depth[ cell ] == grid.m_deepest )
        {
            ++count;
            columns += cell % SIZE;
            rows += cell / SIZE;
        }
    }

    i32 best{ -1 }, nearest{ };
    for ( i32 cell{ }; cell < SIZE * SIZE; ++cell )
    {
        if ( grid.m_depth[ cell ] != grid.m_deepest )
        {
            continue;
        }

        const i32 across{ ( cell % SIZE ) * count - columns };
        const i32 down{ ( cell / SIZE ) * count - rows };
        const i32 distance{ across * across + down * down };
        if ( best < 0 || distance < nearest )
        {
            best    = cell;
            nearest = distance;
        }
    }

    return best;
}

constexpr i32 MASK_HALF{ MASK_SIZE / 2 };
constexpr i32 TRACE_BUDGET{ 32 };
constexpr i32 PRIOR_TRACE_BUDGET{ 128 };
constexpr i32 SHIFT_STEPS{ 2 };
constexpr i32 POLISH_MAX_ENTRIES{ 16 };

ATLAS_FORCE_INLINE void kernel_add( salvo_kernel_t &kernel, f32 x, f32 y, f32 weight )
{
    Assert( kernel.m_count < KERNEL_CAPACITY );
    kernel.m_entries[ kernel.m_count++ ] = { x, y, weight };
}

ATLAS_FORCE_INLINE i32 to_cell( f32 value )
{
    return static_cast< i32 >( value + ( value < 0.0f ? -0.5f : 0.5f ) );
}

// a world point along the eye's line onto the plane, in world units of the plane axes
ATLAS_FORCE_INLINE bool mask_project( const salvo_mask_t &mask, const vec3 &point, f32 &u, f32 &v )
{
    const vec3 to_point{ point - mask.m_eye };
    const f32  depth{ to_point.dot( mask.m_forward ) };

    if ( depth <= epsilon )
    {
        return false;
    }

    const vec3 on_plane{ mask.m_eye + to_point * ( mask.m_distance / depth ) - mask.m_centre };

    u = on_plane.dot( mask.m_right );
    v = on_plane.dot( mask.m_up );

    return true;
}

ATLAS_FORCE_INLINE vec3 mask_point( const salvo_mask_t &mask, f32 i, f32 j )
{
    return mask.m_centre + mask.m_right * ( i * mask.m_cell ) + mask.m_up * ( j * mask.m_cell );
}

ATLAS_FORCE_INLINE bool brushes_clear( const vec3 &from, const vec3 &to )
{
    trace_t               trace{ };
    Ray_t                 ray{ };
    CTraceFilterWorldOnly filter{ };

    ray.Init( from, to );
    g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );

    return trace.fraction == 1.0f;
}

ATLAS_FORCE_INLINE bool clip_face( const scan_pose_t &pose, const vec3 &entry, vec3 &face )
{
    const vec3 axis{ pose.m_origin.x, pose.m_origin.y, entry.z };
    const f32  inside{ math::segment_box_fraction( entry, axis, pose.m_origin + pose.m_mins * pose.m_scale,
                                                   pose.m_origin + pose.m_maxs * pose.m_scale ) };

    if ( inside == FLT_MAX )
    {
        return false;
    }

    face = entry + ( axis - entry ) * inside;
    return true;
}

ATLAS_FORCE_INLINE bool mask_setup(
    salvo_mask_t             &mask,
    const mstudiohitboxset_t *set,
    const scan_pose_t        &pose,
    i32                       hitboxes,
    C_BaseEntity             *player,
    const vec3               &shoot_pos,
    f32                       spread,
    f32                       range )
{
    mask.m_eye       = shoot_pos;
    mask.m_hull_mins = pose.m_origin + VEC_HULL_MIN * pose.m_scale;
    mask.m_hull_maxs = pose.m_origin + VEC_HULL_MAX * pose.m_scale;
    mask.m_centre    = ( mask.m_hull_mins + mask.m_hull_maxs ) * 0.5f;
    mask.m_forward   = mask.m_centre - shoot_pos;
    mask.m_distance  = mask.m_forward.normalize( );

    if ( mask.m_distance <= epsilon )
    {
        return false;
    }

    vec3 angles{ };
    math::vector_angles( mask.m_forward, angles );
    math::angle_vectors( angles, nullptr, &mask.m_right, &mask.m_up );

    // four or five cells across the pattern's typical width, never finer than the pose is known
    mask.m_cell     = math::clamp( spread * mask.m_distance / 5.0f, 2.0f, 8.0f );
    mask.m_range    = range;
    mask.m_set      = set;
    mask.m_pose     = &pose;
    mask.m_player   = player;
    mask.m_hitboxes = hitboxes;

    auto build_rect{ [ & ]( i32 hitbox )
    {
        const mstudiobbox_t  *box{ set->pHitbox( hitbox ) };
        salvo_mask_t::rect_t &rect{ mask.m_rects[ hitbox ] };

        rect.m_valid = box && box->bone >= 0 && box->bone < MAX_STUDIO_BONES;
        if ( !rect.m_valid )
        {
            return;
        }

        const matrix3x4 &bone{ pose.m_bones[ box->bone ] };

        rect.m_low_u  = FLT_MAX;
        rect.m_low_v  = FLT_MAX;
        rect.m_high_u = -FLT_MAX;
        rect.m_high_v = -FLT_MAX;

        for ( i32 corner{ }; corner < 8; ++corner )
        {
            vec3 world{ };
            math::vector_transform( math::box_corner( box->bbmin, box->bbmax, corner ), bone, world );

            f32 u{ };
            f32 v{ };
            if ( !mask_project( mask, world, u, v ) )
            {
                rect.m_valid = false; // a corner behind the eye has no place on the plane
                break;
            }

            if ( u < rect.m_low_u )
            {
                rect.m_low_u        = u;
                rect.m_extreme[ 0 ] = world;
            }
            if ( u > rect.m_high_u )
            {
                rect.m_high_u       = u;
                rect.m_extreme[ 1 ] = world;
            }
            if ( v < rect.m_low_v )
            {
                rect.m_low_v        = v;
                rect.m_extreme[ 2 ] = world;
            }
            if ( v > rect.m_high_v )
            {
                rect.m_high_v       = v;
                rect.m_extreme[ 3 ] = world;
            }
        }
    } };

    if ( hitboxes < 8 || g_thread_pool.concurrency( ) < 2 )
    {
        for ( i32 hitbox{ }; hitbox < hitboxes; ++hitbox )
        {
            build_rect( hitbox );
        }
    } else
    {
        const auto run{ +[]( void *context, i32 index, i32 worker )
        {
            ( *static_cast< decltype( build_rect ) * >( context ) )( index );
        } };
        if ( !g_thread_pool.run( run, &build_rect, hitboxes ) )
        {
            for ( i32 hitbox{ }; hitbox < hitboxes; ++hitbox )
            {
                build_rect( hitbox );
            }
        }
    }

    return true;
}

ATLAS_FORCE_INLINE cell_state_t mask_ray( salvo_mask_t &mask, const vec3 &end, f32 u, f32 v )
{
    f32 nearest{ FLT_MAX };
    for ( i32 hitbox{ }; hitbox < mask.m_hitboxes; ++hitbox )
    {
        const salvo_mask_t::rect_t &rect{ mask.m_rects[ hitbox ] };
        if ( !rect.m_valid || u < rect.m_low_u || u > rect.m_high_u || v < rect.m_low_v || v > rect.m_high_v )
        {
            continue;
        }

        const mstudiobbox_t *box{ mask.m_set->pHitbox( hitbox ) };
        f32                  entry{ };
        if ( math::ray_hits_box( mask.m_eye, end, mask.m_pose->m_bones[ box->bone ], box->bbmin, box->bbmax, 0.0f, &entry ) )
        {
            nearest = math::smaller( nearest, entry );
        }
    }

    if ( nearest > 1.0f )
    {
        return cell_miss;
    }

    const vec3 delta{ end - mask.m_eye };
    const vec3 entry{ mask.m_eye + delta * nearest };
    const f32  reach{ math::segment_box_fraction( mask.m_eye, end, mask.m_hull_mins, mask.m_hull_maxs ) };
    vec3       face{ };
    const bool clips{ clip_face( *mask.m_pose, entry, face ) };

    if ( reach == FLT_MAX && !clips )
    {
        return cell_miss;
    } else if ( mask.m_traces >= mask.m_trace_limit )
    {
        return cell_blocked;
    }

    ++mask.m_traces;
    if ( !g_cl.trace_line( g_cl.m_local, mask.m_player, mask.m_eye, entry, MASK_SOLID | CONTENTS_HITBOX ) )
    {
        return cell_blocked;
    }

    if ( reach <= nearest )
    {
        return cell_hit;
    }

    if ( reach != FLT_MAX )
    {
        ++mask.m_traces;
        if ( brushes_clear( entry, mask.m_eye + delta * reach ) )
        {
            return cell_hit;
        }
    }

    if ( !clips )
    {
        return cell_blocked;
    }

    ++mask.m_traces;
    return g_cl.trace_line( g_cl.m_local, mask.m_player, entry, face, MASK_SOLID ) ? cell_hit : cell_blocked;
}

ATLAS_FORCE_INLINE cell_state_t mask_cell( salvo_mask_t &mask, i32 i, i32 j )
{
    if ( i < -MASK_HALF || i >= MASK_HALF || j < -MASK_HALF || j >= MASK_HALF )
    {
        return cell_miss;
    }

    cell_state_t &cell{ mask.m_cells[ ( j + MASK_HALF ) * MASK_SIZE + ( i + MASK_HALF ) ] };
    if ( cell == cell_unknown )
    {
        const vec3 point{ mask_point( mask, i, j ) };
        const vec3 end{ mask.m_eye + ( point - mask.m_eye ).normalized( ) * mask.m_range };
        cell = mask_ray( mask, end, static_cast< f32 >( i ) * mask.m_cell, static_cast< f32 >( j ) * mask.m_cell );
    }
    return cell;
}

// expected pellets on the target with the pattern centred on cell
ATLAS_FORCE_INLINE salvo_score_t salvo_score( salvo_mask_t &mask, const salvo_kernel_t &kernel, f32 a, f32 b )
{
    vec3 angles{ };
    math::vector_angles( mask_point( mask, a, b ) - mask.m_eye, angles );

    vec3 forward{ };
    vec3 right{ };
    vec3 up{ };
    math::angle_vectors( angles, &forward, &right, &up );

    salvo_score_t score{ };
    f32           sum_i{ };
    f32           sum_j{ };

    for ( i32 index{ }; index < kernel.m_count; ++index )
    {
        const kernel_entry_t &entry{ kernel.m_entries[ index ] };
        const vec3            direction{ forward + right * ( entry.m_x * kernel.m_spread ) + up * ( entry.m_y * kernel.m_spread ) };
        const f32             depth{ direction.dot( mask.m_forward ) };
        if ( depth <= epsilon )
        {
            continue;
        }

        const f32          scale{ mask.m_distance / ( depth * mask.m_cell ) };
        const f32          i{ direction.dot( mask.m_right ) * scale };
        const f32          j{ direction.dot( mask.m_up ) * scale };
        // A cached cell proves only its centre ray; a known pellet must hit with its actual direction.
        const cell_state_t state{ kernel.m_is_prior
                                      ? mask_cell( mask, to_cell( i ), to_cell( j ) )
                                      : mask_ray( mask, mask.m_eye + direction.normalized( ) * mask.m_range,
                                                  i * mask.m_cell, j * mask.m_cell ) };

        switch ( state )
        {
            case cell_hit:
                score.m_open_hits += entry.m_weight;
                score.m_hits += entry.m_weight;
                sum_i += entry.m_weight * ( kernel.m_is_prior ? static_cast< f32 >( to_cell( i ) ) : i );
                sum_j += entry.m_weight * ( kernel.m_is_prior ? static_cast< f32 >( to_cell( j ) ) : j );
                break;
            case cell_blocked: score.m_open_hits += entry.m_weight; break;
            default: break;
        }
    }

    if ( score.m_hits > 0.0f )
    {
        score.m_mean_i = sum_i / score.m_hits;
        score.m_mean_j = sum_j / score.m_hits;
    }

    mask.m_open_hits = math::bigger( mask.m_open_hits, score.m_open_hits );
    return score;
}

ATLAS_FORCE_INLINE salvo_score_t refine_aim( salvo_mask_t &mask, const salvo_kernel_t &kernel, f32 &a, f32 &b, salvo_score_t best )
{
    const f32 seed_a{ a };
    const f32 seed_b{ b };

    if ( !kernel.m_is_prior )
    {
        const vec3 direction{ ( mask_point( mask, seed_a, seed_b ) - mask.m_eye ).normalized( ) };
        const i32  count{ math::smaller( kernel.m_count, POLISH_MAX_ENTRIES ) };
        for ( i32 index{ }; index < count && best.m_hits < kernel.m_weight; ++index )
        {
            const kernel_entry_t &entry{ kernel.m_entries[ index ] };
            vec3                  angles{ };
            if ( !math::launch_angles( direction, 1.0f, entry.m_x * kernel.m_spread, entry.m_y * kernel.m_spread, angles ) )
            {
                continue;
            }

            vec3 forward{ };
            math::angle_vectors( angles, &forward );
            f32 u{ };
            f32 v{ };
            if ( !mask_project( mask, mask.m_eye + forward * mask.m_distance, u, v ) )
            {
                continue;
            }

            const f32           next_a{ u / mask.m_cell };
            const f32           next_b{ v / mask.m_cell };
            const salvo_score_t moved{ salvo_score( mask, kernel, next_a, next_b ) };
            if ( moved.m_hits > best.m_hits )
            {
                a    = next_a;
                b    = next_b;
                best = moved;
            }
        }
    }

    for ( i32 step{ }; step < SHIFT_STEPS && best.m_hits > 0.0f && best.m_hits < kernel.m_weight; ++step )
    {
        // Move the landing pattern towards the target seed, not the aim towards the displaced hits.
        const f32 next_a{ kernel.m_is_prior ? static_cast< f32 >( to_cell( best.m_mean_i ) ) : a + seed_a - best.m_mean_i };
        const f32 next_b{ kernel.m_is_prior ? static_cast< f32 >( to_cell( best.m_mean_j ) ) : b + seed_b - best.m_mean_j };

        if ( next_a == a && next_b == b )
        {
            break;
        }

        const salvo_score_t moved{ salvo_score( mask, kernel, next_a, next_b ) };
        if ( moved.m_hits < best.m_hits )
        {
            break;
        }

        a    = next_a;
        b    = next_b;
        best = moved;
    }

    if ( kernel.m_count > POLISH_MAX_ENTRIES || best.m_hits >= kernel.m_weight )
    {
        return best;
    }

    f32 best_a{ a };
    f32 best_b{ b };

    for ( i32 di{ -1 }; di <= 1; ++di )
    {
        for ( i32 dj{ -1 }; dj <= 1; ++dj )
        {
            if ( di == 0 && dj == 0 )
            {
                continue;
            }

            const salvo_score_t moved{ salvo_score( mask, kernel, a + di, b + dj ) };
            if ( moved.m_hits > best.m_hits )
            {
                best   = moved;
                best_a = a + di;
                best_b = b + dj;
            }
        }
    }

    a = best_a;
    b = best_b;

    return best;
}

ATLAS_FORCE_INLINE f32 salvo_roll( const salvo_mask_t &mask, const salvo_kernel_t &kernel )
{
    f32 total{ };
    f32 mean_x{ };
    f32 mean_y{ };

    for ( i32 index{ }; index < kernel.m_count; ++index )
    {
        const kernel_entry_t &entry{ kernel.m_entries[ index ] };
        total += entry.m_weight;
        mean_x += entry.m_weight * entry.m_x;
        mean_y += entry.m_weight * entry.m_y;
    }

    if ( total <= 0.0f )
    {
        return 0.0f;
    }

    mean_x /= total;
    mean_y /= total;

    f32 xx{ };
    f32 yy{ };
    f32 xy{ };

    for ( i32 index{ }; index < kernel.m_count; ++index )
    {
        const kernel_entry_t &entry{ kernel.m_entries[ index ] };
        const f32             dx{ entry.m_x - mean_x };
        const f32             dy{ entry.m_y - mean_y };
        xx += entry.m_weight * dx * dx;
        yy += entry.m_weight * dy * dy;
        xy += entry.m_weight * dx * dy;
    }

    f32 low_u{ FLT_MAX };
    f32 high_u{ -FLT_MAX };
    f32 low_v{ FLT_MAX };
    f32 high_v{ -FLT_MAX };

    for ( i32 hitbox{ }; hitbox < mask.m_hitboxes; ++hitbox )
    {
        const salvo_mask_t::rect_t &rect{ mask.m_rects[ hitbox ] };
        if ( !rect.m_valid )
        {
            continue;
        }

        low_u  = math::smaller( low_u, rect.m_low_u );
        high_u = math::bigger( high_u, rect.m_high_u );
        low_v  = math::smaller( low_v, rect.m_low_v );
        high_v = math::bigger( high_v, rect.m_high_v );
    }

    // no rect leaves nothing to lay the pattern along
    if ( low_u > high_u )
    {
        return 0.0f;
    }

    return no_spread::axis_roll( xx, yy, xy, high_v - low_v >= high_u - low_u ? PI * 0.5f : 0.0f );
}

ATLAS_FORCE_INLINE const salvo_kernel_t &roll_kernel( const salvo_kernel_t &kernel, f32 roll, salvo_kernel_t &out )
{
    const f32 c{ math::cos( DEG2RAD( roll ) ) };
    const f32 s{ math::sin( DEG2RAD( roll ) ) };

    for ( i32 index{ }; index < kernel.m_count; ++index )
    {
        const kernel_entry_t &entry{ kernel.m_entries[ index ] };
        out.m_entries[ index ] = { entry.m_x * c + entry.m_y * s, entry.m_y * c - entry.m_x * s, entry.m_weight };
    }

    out.m_count    = kernel.m_count;
    out.m_weight   = kernel.m_weight;
    out.m_spread   = kernel.m_spread;
    out.m_range    = kernel.m_range;
    out.m_is_prior = kernel.m_is_prior;
    return out;
}

bool ragebot::aimed_this_command( ) const
{
    return g_cl.m_cmd && this->m_aimed_sequence == g_networking.sequence_number( );
}

bool ragebot::evaluated_this_command( ) const
{
    return g_cl.m_cmd && this->m_evaluated_sequence == g_networking.sequence_number( );
}

void ragebot::think( )
{
    const bool live{ this->m_visual_live };
    this->m_visual_live        = false;
    this->m_aimed_sequence     = -1;
    this->m_evaluated_sequence = -1;
    if ( g_cl.weapon_throw_pending( ) || !should_run( ) )
    {
        this->m_visual_target = INVALID_EHANDLE_INDEX;
        return;
    }

    this->m_hitscan_profile    = g_cfg.m_ragebot.m_hitscan_profiles[ client::get_profile( ) ];
    this->m_projectile_profile = g_cfg.m_ragebot.m_projectile_profiles[ client::get_profile( ) ];

    this->m_projectile_solved = false;

    if ( !this->fill_weapon_data( ) )
    {
        this->m_visual_target = INVALID_EHANDLE_INDEX;
        return;
    }

    if ( m_weapon_data.m_weapon_type == weapon_type::projectile_weapon && !m_weapon_data.m_is_sticky &&
         g_cl.m_weapon->m_flNextPrimaryAttack( ) > TICKS_TO_TIME( g_cl.m_local->m_nTickBase( ) ) && !this->should_fire( ) )
    {
        if ( vec3 aimed{ }; live && this->visual_aim( aimed ) )
        {
            this->m_visual_live = true;
            this->m_visual_time = this->m_visual_target_time = g_tf2.m_engine->Plat_FloatTime( );
        }

        return;
    }

    ragebot_target_t target{ };
    if ( !this->find_target( target ) )
    {
        this->m_visual_target = INVALID_EHANDLE_INDEX;
        return;
    }

    aim_at_target( target );
}

bool ragebot::find_target( ragebot_target_t &out )
{
    if ( this->m_reload_probe )
    {
        if ( g_cl.m_cmd->weaponselect != 0 )
        {
            return false;
        }
    } else if ( !do_prefetch_logic( ) )
    {
        return false;
    }

    // no mp, grab first visible point
    if ( m_weapon_data.m_weapon_type == weapon_type::hitscan_weapon &&
         this->m_hitscan_profile.m_hitscan_multipoint && !g_holdaim_mgr.held( ) )
    {
        this->build_kernel( this->m_kernel );
    } else
    {
        m_kernel.m_count = 0;
    }

    g_tf2.m_engine->GetViewAngles( this->m_view_angles );
    if ( !this->m_reload_probe )
    {
        if ( const vec3 *held{ g_holdaim_mgr.held( ) } )
        {
            this->m_view_angles = *held;
        }
    }

    {
        fetch_initial_targets( );
    }

    if ( m_targets.empty( ) )
    {
        return false; // an idle charge is started and held by maintain_managed_charge
    }

    atl::sort( m_targets.begin( ), m_targets.end( ), [ & ]( ragebot_target_t &first, ragebot_target_t &second )
    {
        return first.m_fov_to < second.m_fov_to;
    } );

    if ( m_weapon_data.m_weapon_type == weapon_type::projectile_weapon )
    {
        while ( m_targets.size( ) > this->m_projectile_profile.m_projectile_max_targets )
        {
            m_targets.pop_back( );
        }
    }

    return get_best_target( out );
}

bool ragebot::fill_weapon_data( f32 charge_time )
{
    atl::memset( &m_weapon_data, 0, sizeof( ragebot_weapon_data_t ) );

    m_weapon_data.m_weapon_type = weapon_type::invalid_weapon;
    m_weapon_data.m_charge_time = charge_time;

    if ( !g_cl.m_weapon )
    {
        return false;
    }

    const i32 weapon_id{ g_cl.m_weapon->GetWeaponID( ) };

    bool projectile{ false };

    switch ( weapon_id )
    {
        case TF_WEAPON_ROCKETLAUNCHER:
        case TF_WEAPON_PIPEBOMBLAUNCHER:
        case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
        case TF_WEAPON_GRENADELAUNCHER:
        case TF_WEAPON_FLAREGUN:
        case TF_WEAPON_COMPOUND_BOW:
        case TF_WEAPON_CROSSBOW:
        case TF_WEAPON_PARTICLE_CANNON:
        case TF_WEAPON_DRG_POMSON:
        case TF_WEAPON_RAYGUN:
        case TF_WEAPON_FLAREGUN_REVENGE:
        case TF_WEAPON_CANNON:
        case TF_WEAPON_SYRINGEGUN_MEDIC:
        case TF_WEAPON_FLAME_BALL:
        case TF_WEAPON_FLAMETHROWER:
        case TF_WEAPON_SHOTGUN_BUILDING_RESCUE:
        case TF_WEAPON_CLEAVER:
        case TF_WEAPON_JAR:
        case TF_WEAPON_JAR_MILK:
        case TF_WEAPON_JAR_GAS: projectile = true; break;

        default: break;
    }

    if ( projectile )
    {
        vec3               view_angles{ g_cl.m_cmd->viewangles };
        projectile_setup_t setup{ };
        if ( !g_proj_sim.gather( g_cl.m_local, g_cl.m_weapon, view_angles, setup, g_cl.m_cmd->command_number, charge_time ) )
        {
            return false;
        }

        if ( !setup.m_valid )
        {
            return false;
        }

        m_weapon_data.m_weapon_type      = weapon_type::projectile_weapon;
        m_weapon_data.m_projectile_speed = setup.m_speed;
        m_weapon_data.m_up_kick          = setup.m_up_kick;

        // the flight starts this much nearer the target than the eye the solve runs from
        vec3 view_forward{ };
        math::angle_vectors( view_angles, &view_forward );

        m_weapon_data.m_muzzle_forward = ( setup.m_position - g_cl.m_local->Weapon_ShootPosition( ) ).dot( view_forward );
        m_weapon_data.m_is_vphysics    = setup.m_movetype == projectile_movetype::vphysics;
        m_weapon_data.m_is_sticky      = weapon_id == TF_WEAPON_PIPEBOMBLAUNCHER;
        m_weapon_data.m_gravity_mod    = setup.m_gravity;
        m_weapon_data.m_gravity        = g_tf2.sv_gravity->GetFloat( ) * setup.m_gravity;
        m_weapon_data.m_launch_spread  = g_cl.attrib_hook( g_cl.m_weapon, 0.0f, ATLAS_XORSTR( "projectile_spread_angle" ).get( ) );
        m_launch_setup                 = setup;

        f32 launch_speed{ math::sqrt( setup.m_speed * setup.m_speed + setup.m_up_kick * setup.m_up_kick ) };
        if ( m_weapon_data.m_is_vphysics )
        {
            launch_speed = math::smaller( launch_speed, 2000.0f );
        }

        m_weapon_data.m_launch_speed         = launch_speed;
        m_weapon_data.m_has_life_time        = setup.m_lifetime > 0.0f;
        m_weapon_data.m_projectile_life_time = setup.m_lifetime;
        m_weapon_data.m_has_hull_size        = !setup.m_hull.is_zero( );
        m_weapon_data.m_hull_mins            = setup.m_hull_centre - setup.m_hull;
        m_weapon_data.m_hull_maxs            = setup.m_hull_centre + setup.m_hull;
        m_weapon_data.m_blast_radius         = g_cl.m_weapon->GetProjectileBlastRadius( );
        m_weapon_data.m_blast_falloff        = 0.5f;

        const f32 falloff_mod{ g_cl.attrib_hook( g_cl.m_weapon, 1.0f, ATLAS_XORSTR( "mult_dmg_falloff" ).get( ) ) };
        if ( falloff_mod != 1.0f )
        {
            m_weapon_data.m_blast_falloff += falloff_mod;
        }

        // Arming constrains detonation, not physical interception.
        if ( weapon_id == TF_WEAPON_PIPEBOMBLAUNCHER )
        {
            m_weapon_data.m_arm_time = splash::arm_time( g_cl.m_weapon );
        }

        switch ( weapon_id )
        {
            case TF_WEAPON_COMPOUND_BOW:
            case TF_WEAPON_PIPEBOMBLAUNCHER:
            case TF_WEAPON_CANNON: m_weapon_data.m_fires_on_attack_release = true; break;

            default: break;
        }

        switch ( weapon_id )
        {
            case TF_WEAPON_ROCKETLAUNCHER:
            case TF_WEAPON_PARTICLE_CANNON:
            case TF_WEAPON_PIPEBOMBLAUNCHER: m_weapon_data.m_splashes = true; break;

            default: break;
        }

        return true;
    }

    if ( g_cl.m_weapon->GetSlot( ) == 2 )
    {
        return false;
    }

    const auto weapon_info{ g_cl.m_weapon->GetWeaponInfo( ) };
    if ( !weapon_info )
    {
        return false;
    }

    const auto weapon_data{ weapon_info->GetWeaponData( g_cl.m_weapon->m_iWeaponMode( ) ) };

    m_weapon_data.m_fires_on_attack_release            = weapon_id == TF_WEAPON_SNIPERRIFLE_CLASSIC;
    m_weapon_data.m_weapon_type                        = weapon_type::hitscan_weapon;
    m_weapon_data.m_should_fire_always_set_attack_bits = weapon_data.m_bUseRapidFireCrits; // NOTE: Hoku; This is a bit of a hack. But this is a hitscan catch only, so it's fine.
    m_weapon_data.m_range                              = weapon_data.m_flRange;

    return true;
}

bool ragebot::should_run( )
{
    if ( !g_cfg.m_ragebot.m_enabled )
    {
        return false;
    }

    if ( !gui::bind_active( g_cfg.m_ragebot.m_activate_key ) )
    {
        return false;
    }

    if ( !g_cfg.m_ragebot.m_auto_shoot )
    {
        if ( !( g_cl.m_cmd->buttons & IN_ATTACK ) && !g_cl.will_fire_this_tick( ) )
        {
            return false;
        }
    }

    if ( !g_cl.m_local->IsAlive( ) ||   // We're dead.
         g_cl.m_local->IsInKart( ) ||   // We're in a kart.
         g_cl.m_local->IsGhost( ) ||    // We're a ghost.
         g_cl.m_local->IsFeigned( ) ||  // We're holding a dead ringer.
         g_cl.m_local->IsCloaked( ) ||  // We're cloaked.
         g_cl.m_local->IsTaunting( ) || // We're taunting.
         g_cl.m_local->IsBonked( ) )    // We're under the effects of bonk.
    {
        return false;
    }

    if ( !g_cl.m_weapon )
    {
        return false;
    }

    if ( g_cl.m_weapon->GetSlot( ) == 2 )
    {
        return false;
    }

    if ( g_gui.open( ) )
    {
        return false;
    }

    if ( is_blacklisted_weapon( g_cl.m_weapon ) )
    {
        return false;
    }

    if ( g_tickbase.shifting( ) && g_holdaim_mgr.held( ) != nullptr )
    {
        return false;
    }

    return true;
}

// TODO: Remove this...
bool ragebot::holds_aim_key( )
{
    return gui::bind_active( g_cfg.m_ragebot.m_activate_key );
}

// false cancels the aim for this command; a pending animation event means we cannot fire yet
bool ragebot::do_prefetch_logic( )
{
    // a switch in flight means whatever this command aims or presses lands on a weapon on its way out
    if ( g_cl.m_cmd->weaponselect != 0 )
    {
        return false;
    }

    if ( !g_cl.m_local->IsZoomed( ) &&
         ( g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_SNIPERRIFLE ||
           g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_SNIPERRIFLE_DECAP ) )
    {
        if ( !rezoom_cycle_pending( g_cl.m_weapon->As< C_TFSniperRifle * >( ) ) )
        {
            g_cl.m_cmd->buttons |= IN_ATTACK2;
        }

        return false;
    }

    return true;
}

bool ragebot::is_hitbox_allowed_for_current_state( i32 hitbox ) const
{
    switch ( hitbox )
    {
        case HITBOX_HEAD: return this->m_hitscan_profile.m_hitscan_boxes[ config::HITSCAN_BOX_HEAD ] || this->m_hitscan_profile.m_aimpoint_priority == config::AIMPOINT_PRIORITY_HEAD;
        case HITBOX_PELVIS: return this->m_hitscan_profile.m_hitscan_boxes[ config::HITSCAN_BOX_PELVIS ] || this->m_hitscan_profile.m_aimpoint_priority == config::AIMPOINT_PRIORITY_PELVIS;
        case HITBOX_SPINE_0: return this->m_hitscan_profile.m_hitscan_boxes[ config::HITSCAN_BOX_STOMACH ] || this->m_hitscan_profile.m_aimpoint_priority == config::AIMPOINT_PRIORITY_STOMACH;
        case HITBOX_SPINE_1: return this->m_hitscan_profile.m_hitscan_boxes[ config::HITSCAN_BOX_LOWER_CHEST ] || this->m_hitscan_profile.m_aimpoint_priority == config::AIMPOINT_PRIORITY_LOWER_CHEST;
        case HITBOX_SPINE_2: return this->m_hitscan_profile.m_hitscan_boxes[ config::HITSCAN_BOX_CHEST ] || this->m_hitscan_profile.m_aimpoint_priority == config::AIMPOINT_PRIORITY_CHEST;
        case HITBOX_SPINE_3: return this->m_hitscan_profile.m_hitscan_boxes[ config::HITSCAN_BOX_UPPER_CHEST ] || this->m_hitscan_profile.m_aimpoint_priority == config::AIMPOINT_PRIORITY_UPPER_CHEST;
        case HITBOX_UPPERARM_L:
        case HITBOX_UPPERARM_R:
        case HITBOX_LOWERARM_L:
        case HITBOX_LOWERARM_R: return this->m_hitscan_profile.m_hitscan_boxes[ config::HITSCAN_BOX_ARMS ];
        case HITBOX_HIP_L:
        case HITBOX_HIP_R:
        case HITBOX_KNEE_L:
        case HITBOX_KNEE_R:
        case HITBOX_FOOT_L:
        case HITBOX_FOOT_R: return this->m_hitscan_profile.m_hitscan_boxes[ config::HITSCAN_BOX_LEGS ];
        default: return false;
    }
}

usize ragebot::get_best_priority_hitbox( C_TFPlayer *target ) const
{
    if ( this->m_hitscan_profile.m_aimpoint_priority != config::AIMPOINT_PRIORITY_AUTO )
    {
        return this->m_hitscan_profile.m_aimpoint_priority;
    }

    const auto target_hp{ target->m_iHealth( ) };
    if ( is_weapon_ambassador( g_cl.m_weapon ) )
    {
        const auto revolver{ g_cl.m_weapon->As< C_TFRevolver * >( ) };
        const bool should_go_for_body{ ( i32 )revolver->CalcRangedDamage( target ) > target_hp || !revolver->CanHeadshotTarget( target ) };
        return should_go_for_body ? config::AIMPOINT_PRIORITY_PELVIS : config::AIMPOINT_PRIORITY_HEAD;
    }

    if ( is_weapon_sniper_rifle( g_cl.m_weapon ) )
    {
        const auto sniper_rifle{ g_cl.m_weapon->As< C_TFSniperRifle * >( ) };
        return sniper_rifle->GetProjectileDamage( ) > target_hp ? config::AIMPOINT_PRIORITY_PELVIS : config::AIMPOINT_PRIORITY_HEAD;
    }

    const auto local_wep_id{ g_cl.m_weapon->GetWeaponID( ) };
    const auto target_class{ target->m_iClass( ) };

    switch ( target_class )
    {
        case TF_CLASS_DEMOMAN:
        case TF_CLASS_HEAVYWEAPONS:
        case TF_CLASS_SPY:
        case TF_CLASS_PYRO:
        {
            // These weapons benefit the most from shooting close to the center of the body. So prioritize that.
            switch ( local_wep_id )
            {
                case TF_WEAPON_MINIGUN:
                case TF_WEAPON_SCATTERGUN:
                case TF_WEAPON_SHOTGUN_HWG:
                case TF_WEAPON_SHOTGUN_PYRO:
                case TF_WEAPON_SHOTGUN_PRIMARY:
                case TF_WEAPON_SHOTGUN_SOLDIER:
                case TF_WEAPON_SODA_POPPER:
                case TF_WEAPON_PEP_BRAWLER_BLASTER:
                case TF_WEAPON_HANDGUN_SCOUT_PRIMARY: return config::AIMPOINT_PRIORITY_CHEST;
                default: break;
            }
        }

        default: break;
    }

    return config::AIMPOINT_PRIORITY_PELVIS;
}

// the live state gates, none of which a dormant target can be judged on: a remembered position
// carries an origin and a stale box, and nothing about what the player is doing inside it
bool ragebot::target_state_allowed( C_TFPlayer *player, bool crossbow ) const
{
    if ( player->IsGhost( ) )
    {
        return false;
    }

    if ( !crossbow )
    {
        if ( player->IsCloaked( ) && g_cfg.m_ragebot.m_ignores[ config::IGNORE_CLOAKED ] )
        {
            return false;
        }

        if ( player->IsDisguised( ) && g_cfg.m_ragebot.m_ignores[ config::IGNORE_DISGUISED ] )
        {
            return false;
        }

        if ( player->IsUbered( ) && g_cfg.m_ragebot.m_ignores[ config::IGNORE_UBERED ] )
        {
            return false;
        }

        if ( player->IsFeigned( ) && g_cfg.m_ragebot.m_ignores[ config::IGNORE_FEIGNED ] )
        {
            return false;
        }

        if ( player->IsTaunting( ) && g_cfg.m_ragebot.m_ignores[ config::IGNORE_TAUNTING ] )
        {
            return false;
        }

        player_info_t info{ };
        if ( g_tf2.m_engine->GetPlayerInfo( player->entindex( ), &info ) && g_cl.is_ignored_player( info.friendsID ) )
        {
            return false;
        }
    } else
    {
        if ( player->IsCloaked( ) || player->IsBonked( ) || player->IsUbered( ) )
        {
            return false;
        }

        return true;
    }

    // NOTE: hoku; The enforcer bypasses bullet uber, so we handle it here.
    if ( g_cfg.m_ragebot.m_ignores[ config::IGNORE_VACCINATOR ] &&
         g_cl.m_weapon->m_iItemDefinitionIndex( ) != Spy_m_TheEnforcer )
    {
        // TODO: This stuff should probably get moved to the weapon class.
        const auto resistance_type{ auto_vacc::weapon_resist_type( g_cl.m_weapon->GetWeaponID( ), g_cl.m_weapon->m_iItemDefinitionIndex( ) ) };
        if ( ( player->IsBulletUbered( ) && resistance_type == MEDIGUN_BULLET_RESIST ) ||
             ( player->IsBlastUbered( ) && resistance_type == MEDIGUN_BLAST_RESIST ) ||
             ( player->IsFireUbered( ) && resistance_type == MEDIGUN_FIRE_RESIST ) )
        {
            return false;
        }
    }

    if ( player->IsBonked( ) && g_cfg.m_ragebot.m_ignores[ config::IGNORE_BONKED ] )
    {
        // natascha slows through bonk, so it still has something to gain from the shot
        if ( g_cl.m_weapon->m_iItemDefinitionIndex( ) != Heavy_m_Natascha ||
             !g_cfg.m_ragebot.m_preferences[ config::PREFERENCE_NATASCHA_BONKED ] )
        {
            return false;
        }
    }

    return true;
}

bool ragebot::target_class_allowed( i32 player_class ) const
{
    switch ( player_class )
    {
        case TF_CLASS_SCOUT: return g_cfg.m_ragebot.m_target_classes[ config::TARGET_CLASS_SCOUT ] == true;
        case TF_CLASS_SOLDIER: return g_cfg.m_ragebot.m_target_classes[ config::TARGET_CLASS_SOLDIER ] == true;
        case TF_CLASS_PYRO: return g_cfg.m_ragebot.m_target_classes[ config::TARGET_CLASS_PYRO ] == true;
        case TF_CLASS_DEMOMAN: return g_cfg.m_ragebot.m_target_classes[ config::TARGET_CLASS_DEMOMAN ] == true;
        case TF_CLASS_HEAVYWEAPONS: return g_cfg.m_ragebot.m_target_classes[ config::TARGET_CLASS_HEAVY ] == true;
        case TF_CLASS_ENGINEER: return g_cfg.m_ragebot.m_target_classes[ config::TARGET_CLASS_ENGINEER ] == true;
        case TF_CLASS_MEDIC: return g_cfg.m_ragebot.m_target_classes[ config::TARGET_CLASS_MEDIC ] == true;
        case TF_CLASS_SNIPER: return g_cfg.m_ragebot.m_target_classes[ config::TARGET_CLASS_SNIPER ] == true;
        case TF_CLASS_SPY: return g_cfg.m_ragebot.m_target_classes[ config::TARGET_CLASS_SPY ] == true;
        default: return true;
    }
}

void ragebot::fetch_initial_targets( )
{
    const vec3 &va{ this->m_view_angles };
    const vec3  shoot_pos{ g_cl.m_local->Weapon_ShootPosition( ) };

    const bool bait_filter{ g_cfg.m_ragebot.m_prevent_bait && g_cl.m_local->m_iClass( ) == TF_CLASS_HEAVYWEAPONS };
    const bool bait_override{ gui::bind_active( g_cfg.m_ragebot.m_bait_override_key ) };

    // a remembered position is an origin and a stale box with no bones; splash forgives that error
    const bool allow_dormant{ g_cfg.m_ragebot.m_preferences[ config::PREFERENCE_TARGET_DORMANT ] &&
                              m_weapon_data.m_weapon_type == weapon_type::projectile_weapon &&
                              m_weapon_data.m_splashes };

    bool baited{ };

    m_targets.clear( );
    for ( const auto *cached : g_entity_cache.players( ) )
    {
        if ( !cached->networked( ) )
        {
            continue;
        }

        const i32 i{ cached->m_index };
        if ( i == g_tf2.m_engine->GetLocalPlayer( ) )
        {
            continue;
        }

        C_BaseEntity *base_entity{ cached->m_entity };
        const bool    dormant{ base_entity->IsDormant( ) };

        if ( dormant && !allow_dormant )
        {
            continue;
        }

        switch ( cached->m_class_id )
        {
            case ETFClassID::CTFPlayer:
            {
                const auto player{ base_entity->As< C_TFPlayer * >( ) };

                vec3 remembered{ };
                f32  remaining{ };
                if ( dormant && ( !g_player_data.connected( i ) || !g_player_data.dormant_origin( base_entity, remembered, remaining ) ) )
                {
                    continue;
                }

                const bool alive{ dormant ? g_player_data.alive( i ) : player->IsAlive( ) };
                const i32  team{ dormant ? g_player_data.team( i ) : player->m_iTeamNum( ) };
                const i32  player_class{ dormant ? g_player_data.player_class( i ) : player->m_iClass( ) };
                const bool crossbow{ g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_CROSSBOW };

                if ( !alive )
                {
                    continue;
                }

                if ( TFGameRules( ) )
                {
                    if ( TFGameRules( )->m_bTruceActive( ) && team != g_cl.m_local->m_iTeamNum( ) )
                    {
                        continue;
                    }
                }

                // friendly fire has to be both on in the server and wanted here for a teammate to count
                if ( !crossbow )
                {
                    if ( team == g_cl.m_local->m_iTeamNum( ) &&
                         ( !g_tf2.mp_friendlyfire->GetBool( ) ||
                           !g_cfg.m_ragebot.m_preferences[ config::PREFERENCE_FRIENDLY_FIRE ] ) )
                    {
                        continue;
                    }
                } else
                {
                    if ( team == g_cl.m_local->m_iTeamNum( ) )
                    {
                        const auto max_health{ g_cl.m_resource->m_iMaxHealth( )[ i ] };
                        const auto health{ player->m_iHealth( ) };

                        if ( health >= max_health )
                        {
                            continue;
                        }
                    } else
                    {
                        continue;
                    }
                }

                if ( !dormant && !this->target_state_allowed( player, crossbow ) )
                {
                    continue;
                }

                if ( !this->target_class_allowed( player_class ) )
                {
                    continue;
                }

                if ( !dormant && bait_filter && is_vaccinator_bait( player ) )
                {
                    baited = true;

                    if ( !bait_override )
                    {
                        continue;
                    }
                }

                const vec3 center{ dormant ? remembered + ( player->m_vecMins( ) + player->m_vecMaxs( ) ) * 0.5f
                                           : player->WorldSpaceCenter( ) };
                vec3       ang_to_wsc{ };
                math::vector_angles( center - shoot_pos, ang_to_wsc );

                // a target outside the cone can never produce an aim point inside it
                const f32 fov{ math::calc_fov( va, ang_to_wsc ) };
                if ( g_cfg.m_ragebot.m_fov < 180 && fov > static_cast< f32 >( g_cfg.m_ragebot.m_fov ) )
                {
                    switch ( m_weapon_data.m_weapon_type )
                    {
                        case weapon_type::projectile_weapon:
                        {
                            if ( this->m_projectile_profile.m_projectile_position_mode == config::POSITION_ORIGINAL )
                            {
                                continue;
                            }
                        }

                        case weapon_type::hitscan_weapon:
                        {
                            continue;
                        }

                        case weapon_type::invalid_weapon:
                        {
                            Assert( false );
                            break;
                        }

                        default: break;
                    }
                }

                auto tgt{ m_targets.emplace_back( ) };
                Assert( tgt != nullptr );

                tgt->m_entity         = player;
                tgt->m_fov_to         = fov;
                tgt->m_should_predict = m_weapon_data.m_weapon_type == weapon_type::projectile_weapon;
                tgt->m_aim_origin     = center;
                tgt->m_aim_angle      = ang_to_wsc;
                tgt->m_got_hitpoint   = false;
                tgt->m_class_id       = cached->m_class_id;
                break;
            }

            default: continue;
        }
    }

    if ( g_cfg.m_ragebot.m_targets[ config::TARGET_SENTRYGUNS ] ||
         g_cfg.m_ragebot.m_targets[ config::TARGET_DISPENSERS ] ||
         g_cfg.m_ragebot.m_targets[ config::TARGET_TELEPORTERS ] )
    {
        for ( const auto *cached : g_entity_cache.buildings( ) )
        {
            C_BaseEntity *entity{ cached->m_entity };
            if ( !cached->networked( ) || entity->IsDormant( ) )
            {
                continue;
            }

            C_BaseObject *building{ entity->As< C_BaseObject * >( ) };
            if ( building->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) ||
                 building->m_iHealth( ) <= 0 || building->m_bCarried( ) ||
                 building->m_bPlacing( ) )
            {
                continue;
            }

            const auto cc{ building->GetClientClass( ) };
            if ( !cc )
            {
                continue;
            }

            switch ( cc->GetClassID( ) )
            {
                case ETFClassID::CObjectSentrygun:
                {
                    if ( g_cfg.m_ragebot.m_targets[ config::TARGET_SENTRYGUNS ] == false )
                    {
                        continue;
                    }

                    break;
                }

                case ETFClassID::CObjectDispenser:
                {
                    if ( g_cfg.m_ragebot.m_targets[ config::TARGET_DISPENSERS ] == false )
                    {
                        continue;
                    }

                    break;
                }

                case ETFClassID::CObjectTeleporter:
                {
                    if ( g_cfg.m_ragebot.m_targets[ config::TARGET_TELEPORTERS ] == false )
                    {
                        continue;
                    }

                    break;
                }

                default: continue;
            }

            const vec3 center{ building->WorldSpaceCenter( ) };
            vec3       angle{ };
            math::vector_angles( center - shoot_pos, angle );

            const f32 fov{ math::calc_fov( va, angle ) };
            if ( fov > static_cast< f32 >( g_cfg.m_ragebot.m_fov ) )
            {
                continue;
            }

            auto tgt{ m_targets.emplace_back( ) };
            Assert( tgt != nullptr );

            tgt->m_entity         = building;
            tgt->m_fov_to         = fov;
            tgt->m_should_predict = false;
            tgt->m_aim_origin     = center;
            tgt->m_aim_angle      = angle;
            tgt->m_got_hitpoint   = false;
            tgt->m_class_id       = cached->m_class_id;
        }
    }

    if ( g_cfg.m_ragebot.m_targets[ config::TARGET_PROJECTILES ] && m_weapon_data.m_weapon_type == weapon_type::hitscan_weapon )
    {
        for ( const auto *cached : g_entity_cache.projectiles( ) )
        {
            C_BaseEntity *entity{ cached->m_entity };
            if ( !cached->networked( ) || entity->IsDormant( ) )
            {
                continue;
            }

            if ( cached->m_class_id != ETFClassID::CTFGrenadePipebombProjectile )
            {
                continue;
            }

            const auto pipe{ entity->As< C_TFGrenadePipebombProjectile * >( ) };
            if ( !pipe->m_bTouched( ) )
            {
                continue;
            }

            if ( pipe->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) )
            {
                continue;
            }

            if ( pipe->m_iType( ) != TF_GL_MODE_REMOTE_DETONATE )
            {
                continue;
            }

            const vec3 center{ pipe->WorldSpaceCenter( ) };
            vec3       angle{ };
            math::vector_angles( center - shoot_pos, angle );

            const f32 fov{ math::calc_fov( va, angle ) };
            if ( fov > static_cast< f32 >( g_cfg.m_ragebot.m_fov ) )
            {
                continue;
            }

            auto tgt{ m_targets.emplace_back( ) };
            Assert( tgt != nullptr );

            tgt->m_entity         = pipe;
            tgt->m_fov_to         = fov;
            tgt->m_should_predict = false;
            tgt->m_aim_origin     = center;
            tgt->m_aim_angle      = angle;
            tgt->m_got_hitpoint   = false;
            tgt->m_class_id       = cached->m_class_id;
        }
    }

    if ( g_cfg.m_ragebot.m_targets[ config::TARGET_ENTITIES ] )
    {
        for ( const auto *cached : g_entity_cache.npcs( ) )
        {
            C_BaseEntity *entity{ cached->m_entity };
            if ( !cached->networked( ) || entity->IsDormant( ) )
            {
                continue;
            }

            const auto combat_character{ entity->As< C_BaseCombatCharacter * >( ) };
            if ( combat_character->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) )
            {
                continue;
            }

            const vec3 center{ combat_character->WorldSpaceCenter( ) };
            vec3       angle{ };
            math::vector_angles( center - shoot_pos, angle );

            const f32 fov{ math::calc_fov( va, angle ) };
            if ( fov > static_cast< f32 >( g_cfg.m_ragebot.m_fov ) )
            {
                continue;
            }

            auto tgt{ m_targets.emplace_back( ) };
            Assert( tgt != nullptr );

            tgt->m_entity         = combat_character;
            tgt->m_fov_to         = fov;
            tgt->m_should_predict = false;
            tgt->m_aim_origin     = center;
            tgt->m_aim_angle      = angle;
            tgt->m_got_hitpoint   = false;
            tgt->m_class_id       = cached->m_class_id;
        }
    }

    if ( !this->m_reload_probe )
    {
        this->notify_bait( baited, bait_override );
    }
}

void ragebot::notify_bait( bool baited, bool overridden )
{
    if ( !baited )
    {
        this->m_bait_notified = false;
        return;
    }

    if ( this->m_bait_notified && this->m_bait_overridden == overridden )
    {
        return;
    }

    this->m_bait_notified   = true;
    this->m_bait_overridden = overridden;

    if ( overridden )
    {
        push_notification( notify_kind_t::warning, ICON_CROSSHAIR, ATLAS_XORSTR_STATIC( "Ragebot" ), ATLAS_XORSTR_STATIC( "Overriding bait conditions." ) );
        return;
    }
}

bool ragebot::get_best_target( ragebot_target_t &out )
{
    if ( m_weapon_data.m_weapon_type == weapon_type::projectile_weapon )
    {
        return get_projectile_target( out );
    }

    for ( auto &tgt : m_targets )
    {
        if ( tgt.m_class_id != ETFClassID::CTFPlayer )
        {
            if ( !this->get_building_aimpoint( tgt ) )
            {
                continue;
            }
        } else
        {
            Assert( !tgt.m_should_predict );

            if ( !get_hitscan_aimpoint( tgt ) )
            {
                continue;
            }
        }

        Assert( tgt.m_got_hitpoint );
        out = tgt;
        return true;
    }

    return false;
}

bool ragebot::get_building_aimpoint( ragebot_target_t &out )
{
    const vec3 shoot_pos{ g_cl.m_local->Weapon_ShootPosition( ) };
    const f32  height{ out.m_entity->m_vecMaxs( ).z - out.m_entity->m_vecMins( ).z };
    const vec3 center{ out.m_entity->WorldSpaceCenter( ) };
    const vec3 points[]{ center, center + vec3( 0.0f, 0.0f, height * 0.25f ),
                         center - vec3( 0.0f, 0.0f, height * 0.25f ) };

    for ( const vec3 &point : points )
    {
        if ( !this->within_fov( shoot_pos, point ) )
        {
            continue;
        }

        if ( m_weapon_data.m_weapon_type == weapon_type::hitscan_weapon )
        {
            trace_t             trace{ };
            Ray_t               ray{ };
            CTraceFilterHitscan filter{ };
            filter.m_pSkip = g_cl.m_local;
            ray.Init( shoot_pos, point );
            g_tf2.m_trace->TraceRay( ray, MASK_SOLID | CONTENTS_HITBOX, &filter, &trace );
            if ( trace.m_pEnt != out.m_entity )
            {
                continue;
            }

            out.m_aim_origin = point;
            math::vector_angles( point - shoot_pos, out.m_aim_angle );
            out.m_got_hitpoint = true;
            return true;
        }

        for ( i32 arc{ }; arc < ( m_weapon_data.m_gravity > 0.0f ? 2 : 1 ); ++arc )
        {
            launch_t launch{ };
            if ( !this->solve_launch( this->muzzle_towards( shoot_pos, point ), point, arc != 0, launch ) ||
                 launch.m_time > this->m_projectile_profile.m_projectile_max_sim_time )
            {
                continue;
            }

            const vec3        *held{ g_holdaim_mgr.held( ) };
            const vec3         angle{ held ? *held : launch.m_angle };
            projectile_setup_t setup{ };
            if ( !this->gather_launch( angle, nullptr, 0, setup ) )
            {
                continue;
            }

            vec3      path[ MAX_WALK_TICKS ]{ };
            impact_t  impact{ };
            const i32 ticks{ math::smaller( MAX_WALK_TICKS, TIME_TO_TICKS( this->m_projectile_profile.m_projectile_max_sim_time ) ) };
            g_proj_sim.simulate( setup, path, ticks, 1, impact );
            if ( !impact.m_hit || impact.m_trace.m_pEnt != out.m_entity )
            {
                continue;
            }

            out.m_aim_origin   = impact.m_position;
            out.m_aim_angle    = angle;
            out.m_arrival      = this->arrival_tick( impact.m_time );
            out.m_got_hitpoint = true;
            return true;
        }
    }

    return false;
}

bool ragebot::within_fov( const vec3 &shoot_pos, const vec3 &point ) const
{
    if ( g_cfg.m_ragebot.m_fov >= 180 )
    {
        return true;
    }

    if ( m_weapon_data.m_weapon_type == weapon_type::projectile_weapon &&
         this->m_projectile_profile.m_projectile_position_mode == config::POSITION_ORIGINAL )
    {
        return true;
    }

    vec3 ang_to_point{ };
    math::vector_angles( point - shoot_pos, ang_to_point );

    return math::calc_fov( this->m_view_angles, ang_to_point ) <= static_cast< f32 >( g_cfg.m_ragebot.m_fov );
}

bool ragebot::trace_hitbox(
    const mstudiohitboxset_t *set,
    const scan_pose_t        &pose,
    i32                       hitbox,
    C_BaseEntity             *player,
    const vec3               &shoot_pos,
    vec3                     &out_pos,
    vec3                     &out_ang,
    bool                     &doubt )
{
    if ( hitbox >= set->numhitboxes )
    {
        return false;
    }

    const auto box{ set->pHitbox( hitbox ) };
    if ( box->bone < 0 || box->bone >= MAX_STUDIO_BONES )
    {
        return false;
    }

    doubt = g_resolver.guessing( player->entindex( ) );
    math::vector_transform( ( box->bbmin + box->bbmax ) * 0.5f, pose.m_bones[ box->bone ], out_pos );

    if ( hitbox == HITBOX_HEAD && first_hitbox( set, pose.m_bones, shoot_pos, out_pos ) != HITBOX_HEAD )
    {
        vec3 points[ HEAD_SCAN_POINTS ]{ };
        head_points( box, pose.m_bones, points );

        const vec3 *open{ nullptr };
        for ( const vec3 &point : points )
        {
            if ( first_hitbox( set, pose.m_bones, shoot_pos, point ) == HITBOX_HEAD )
            {
                open = &point;
                break;
            }
        }

        if ( !open )
        {
            return false;
        }

        out_pos = *open;
        doubt   = true;
    }

    if ( !this->within_fov( shoot_pos, out_pos ) )
    {
        return false;
    }

    // @UTIL_PlayerBulletTrace: MASK_SOLID plus hitboxes, so debris does not stop what the bullet passes
    if ( !g_cl.trace_line( g_cl.m_local, player, shoot_pos, out_pos, MASK_SOLID | CONTENTS_HITBOX ) )
    {
        return false;
    }

    if ( !this->shot_registers( set, pose, player, shoot_pos, out_pos ) )
    {
        return false;
    }

    math::vector_angles( out_pos - shoot_pos, out_ang );

    return true;
}

bool ragebot::shot_registers( const mstudiohitboxset_t *set, const scan_pose_t &pose, C_BaseEntity *player,
                              const vec3 &shoot_pos, const vec3 &point ) const
{
    const vec3 end{ shoot_pos + ( point - shoot_pos ).normalized( ) * m_weapon_data.m_range };
    const f32  seen{ math::smaller( ( point - shoot_pos ).length( ) / m_weapon_data.m_range, 1.0f ) };
    const f32  reach{ math::segment_box_fraction( shoot_pos, end, pose.m_origin + VEC_HULL_MIN * pose.m_scale,
                                                  pose.m_origin + VEC_HULL_MAX * pose.m_scale ) };

    if ( reach <= seen )
    {
        return true;
    }

    if ( reach != FLT_MAX && brushes_clear( point, shoot_pos + ( end - shoot_pos ) * reach ) )
    {
        return true;
    }

    f32 entry{ FLT_MAX };
    for ( i32 hitbox{ }; hitbox < set->numhitboxes; ++hitbox )
    {
        const mstudiobbox_t *box{ set->pHitbox( hitbox ) };

        f32 fraction{ };
        if ( math::ray_hits_box( shoot_pos, end, pose.m_bones[ box->bone ], box->bbmin, box->bbmax, 0.0f, &fraction ) )
        {
            entry = math::smaller( entry, fraction );
        }
    }

    if ( entry > 1.0f )
    {
        return false;
    }

    const vec3 at{ shoot_pos + ( end - shoot_pos ) * entry };
    vec3       face{ };
    if ( !clip_face( pose, at, face ) )
    {
        return false;
    }

    return g_cl.trace_line( g_cl.m_local, player, at, face, MASK_SOLID );
}

void ragebot::paint( ) const
{
    if ( !g_cfg.m_ragebot.m_enabled ||
         !g_tf2.m_engine->IsInGame( ) ||
         !g_cl.m_local ||
         !g_cl.m_weapon )
    {
        return;
    }

    if ( !g_cl.m_local->IsAlive( ) ||   // We're dead.
         g_cl.m_local->IsInKart( ) ||   // We're in a kart.
         g_cl.m_local->IsGhost( ) ||    // We're a ghost.
         g_cl.m_local->IsCloaked( ) ||  // We're cloaked.
         g_cl.m_local->IsTaunting( ) || // We're taunting.
         g_cl.m_local->IsBonked( ) )    // We're under the effects of bonk.
    {
        return;
    }

    if ( g_cl.m_weapon->GetSlot( ) == 2 )
    {
        return;
    }

    if ( is_blacklisted_weapon( g_cl.m_weapon ) )
    {
        return;
    }

    this->paint_fov_circle( );
    this->paint_shot_mark( );
}

void ragebot::render( ) const
{
    if ( !g_cfg.m_ragebot.m_enabled || !g_tf2.m_engine->IsInGame( ) || g_gui.open( ) )
    {
        return;
    }

    this->paint_target_path( );
    this->paint_shot( );
}

void ragebot::remember_shot( const mstudiohitboxset_t *set, const scan_pose_t &pose, i32 hitbox_count, i32 hitbox,
                             const vec3 &shoot_pos, const vec3 &aim_point )
{
    if ( this->m_reload_probe )
    {
        return;
    }

    this->m_shot_box_count = 0;
    this->m_shot_hull_mins = pose.m_origin + VEC_HULL_MIN * pose.m_scale;
    this->m_shot_hull_maxs = pose.m_origin + VEC_HULL_MAX * pose.m_scale;
    for ( i32 index{ }; index < hitbox_count; ++index )
    {
        const mstudiobbox_t *box{ set->pHitbox( index ) };
        if ( !box || box->bone < 0 || box->bone >= MAX_STUDIO_BONES )
        {
            continue;
        }

        shot_box_t &kept{ this->m_shot_boxes[ this->m_shot_box_count++ ] };
        atl::memcpy( kept.m_bone, pose.m_bones[ box->bone ], sizeof( matrix3x4 ) );
        kept.m_mins = box->bbmin;
        kept.m_maxs = box->bbmax;
    }

    if ( !g_cfg.m_ragebot.m_hitscan_draw_hitboxes && !g_cfg.m_ragebot.m_hitscan_draw_shot )
    {
        this->m_pending_draw.m_boxes = 0;
        return;
    }

    shot_draw_t &draw{ this->m_pending_draw };

    draw.m_boxes     = hitbox_count;
    draw.m_aimed     = hitbox;
    draw.m_shoot_pos = shoot_pos;
    draw.m_aim_point = aim_point;

    for ( i32 index{ }; index < hitbox_count; ++index )
    {
        const mstudiobbox_t *box{ set->pHitbox( index ) };

        draw.m_valid[ index ] = box && box->bone >= 0 && box->bone < MAX_STUDIO_BONES;
        if ( !draw.m_valid[ index ] )
        {
            continue;
        }

        for ( i32 corner{ }; corner < 8; ++corner )
        {
            math::vector_transform( math::box_corner( box->bbmin, box->bbmax, corner ), pose.m_bones[ box->bone ],
                                    draw.m_corners[ index ][ corner ] );
        }
    }
}

f32 ragebot::shot_fade( ) const
{
    if ( this->m_shot_draw.m_time < 0.0 )
    {
        return 0.0f;
    }

    const f32 duration{ math::bigger( g_cfg.m_ragebot.m_hitscan_draw_duration, 0.1f ) };
    const f32 age{ static_cast< f32 >( g_tf2.m_engine->Plat_FloatTime( ) - this->m_shot_draw.m_time ) };

    return math::bigger( 1.0f - age / duration, 0.0f );
}

void ragebot::paint_shot_mark( ) const
{
    // WTF does this code even do?
    const f32 alpha{ this->shot_fade( ) };
    vec2      mark{ };
    if ( alpha <= 0.0f || !g_cfg.m_ragebot.m_hitscan_draw_shot || !g_cl.world_to_screen( this->m_shot_draw.m_aim_point, mark ) )
    {
        return;
    }

    color_t value{ g_cfg.m_ragebot.m_hitscan_shot_color };
    value.a = static_cast< u8 >( static_cast< f32 >( value.a ) * alpha );

    g_draw.line( vec2{ mark.x - 4.0f, mark.y }, vec2{ mark.x + 4.0f, mark.y }, value, 1.5f );
    g_draw.line( vec2{ mark.x, mark.y - 4.0f }, vec2{ mark.x, mark.y + 4.0f }, value, 1.5f );
}

void ragebot::paint_shot( ) const
{
    const shot_draw_t &shot{ this->m_shot_draw };
    const f32          alpha{ this->shot_fade( ) };
    if ( alpha <= 0.0f || ( !g_cfg.m_ragebot.m_hitscan_draw_hitboxes && !g_cfg.m_ragebot.m_hitscan_draw_shot ) )
    {
        return;
    }

    const auto faded{ [ alpha ]( color_t value )
    {
        value.a = static_cast< u8 >( static_cast< f32 >( value.a ) * alpha );
        return value;
    } };

    if ( g_cfg.m_ragebot.m_hitscan_draw_hitboxes )
    {
        const color_t box{ faded( g_cfg.m_ragebot.m_hitscan_hitbox_color ) };
        const color_t target{ faded( g_cfg.m_ragebot.m_hitscan_target_color ) };

        for ( i32 index{ }; index < shot.m_boxes; ++index )
        {
            if ( !shot.m_valid[ index ] )
            {
                continue;
            }

            const vec3    *corners{ shot.m_corners[ index ] };
            const color_t &value{ index == shot.m_aimed ? target : box };

            for ( i32 corner{ }; corner < 8; ++corner )
            {
                for ( i32 axis{ }; axis < 3; ++axis )
                {
                    const i32 neighbour{ corner ^ ( 1 << axis ) };
                    if ( neighbour > corner )
                    {
                        draw_path_segment( corners[ corner ], corners[ neighbour ], value );
                    }
                }
            }
        }
    }

    if ( g_cfg.m_ragebot.m_hitscan_draw_shot )
    {
        draw_path_segment( shot.m_shoot_pos, shot.m_aim_point, faded( g_cfg.m_ragebot.m_hitscan_shot_color ) );
    }
}

bool ragebot::visual_aim( vec3 &out ) const
{
    if ( g_tf2.m_engine->Plat_FloatTime( ) - this->m_visual_time > 0.7 )
    {
        return false;
    }

    out = this->m_visual_point;

    return true;
}

u32 ragebot::visual_target_handle( ) const
{
    if ( this->m_visual_target == INVALID_EHANDLE_INDEX )
    {
        return INVALID_EHANDLE_INDEX;
    }

    const double age{ g_tf2.m_engine->Plat_FloatTime( ) - this->m_visual_target_time };
    return age >= 0.0 && age <= 0.25 ? this->m_visual_target : INVALID_EHANDLE_INDEX;
}

bool ragebot::crosshair_position( vec2 &out ) const
{
    vec3 point{ };
    if ( !g_cfg.m_ragebot.m_draw_crosshair || !this->visual_aim( point ) )
    {
        return false;
    }

    g_cl.update_w2s( );
    return g_cl.world_to_screen( point, out );
}

void ragebot::draw_path_segment( const vec3 &start, const vec3 &end, const color_t &value, bool separator, bool outline )
{
    g_cl.draw_3d_line( start, end, value, outline );

    if ( !separator )
    {
        return;
    }

    const vec3 dir{ end - start };
    const f32  length{ dir.length2d( ) };
    if ( length <= 0.1f )
    {
        return;
    }

    const vec3 tip{ end + vec3( -dir.y, dir.x, 0.0f ) * ( 7.0f / length ) };
    g_cl.draw_3d_line( end, tip, value, outline );
}

constexpr i32 PATH_TIMELINE_MINOR{ 4 };
constexpr i32 PATH_TIMELINE_MAJOR{ 16 };
constexpr f64 PATH_COMET_SPEED{ 50.0 };
constexpr f32 PATH_COMET_TAIL{ 20.0f };
constexpr f32 PATH_COMET_FLOOR{ 0.12f };
constexpr i32 PATH_COMET_GAP{ 16 };
constexpr f32 PATH_COMET_SPARK{ 3.0f };
constexpr f64 PATH_FLOW_SPEED{ 9.0 };
constexpr i32 PATH_FLOW_SPACING{ 6 };
constexpr f32 PATH_FLOW_SIZE{ 7.0f };
constexpr f32 PATH_FLOW_SPREAD{ 4.0f };

ATLAS_FORCE_INLINE color_t path_faded( color_t value, f32 factor )
{
    value.a = static_cast< u8 >( static_cast< f32 >( value.a ) * factor );
    return value;
}

ATLAS_FORCE_INLINE vec3 path_side( const vec3 &direction )
{
    const f32 length{ direction.length2d( ) };
    return length > 0.1f ? vec3{ -direction.y, direction.x, 0.0f } * ( 1.0f / length ) : vec3{ 0.0f, 1.0f, 0.0f };
}

ATLAS_FORCE_INLINE f32 path_cycle( f64 clock, f64 speed, f64 length )
{
    const f64 walked{ clock * speed };
    return static_cast< f32 >( walked - static_cast< f64 >( static_cast< i32 >( walked / length ) ) * length );
}

static void draw_timeline_step( const ragebot::path_pen_t &pen, const vec3 &start, const vec3 &end,
                                const color_t &value, i32 at )
{
    ragebot::draw_path_segment( start, end, path_faded( value, 0.6f ), false, pen.m_outline );

    const bool major{ at % PATH_TIMELINE_MAJOR == 0 };

    if ( !major && at % PATH_TIMELINE_MINOR != 0 )
    {
        return;
    }

    const vec3 side{ path_side( end - start ) * ( major ? 7.0f : 3.5f ) };
    ragebot::draw_path_segment( end + side, end - side, path_faded( value, major ? 1.0f : 0.75f ), false, pen.m_outline );
}

static void draw_comet_step( const ragebot::path_pen_t &pen, const vec3 &start, const vec3 &end,
                             const color_t &value, i32 at, i32 count )
{
    const f32 head{ path_cycle( pen.m_clock, PATH_COMET_SPEED, static_cast< f64 >( count + PATH_COMET_GAP ) ) };
    const f32 behind{ head - static_cast< f32 >( at ) };
    const f32 fade{ behind < 0.0f ? 0.0f : math::bigger( 1.0f - behind / PATH_COMET_TAIL, 0.0f ) };

    const f32     glow{ fade * fade };
    const color_t white{ 255, 255, 255, value.a };

    ragebot::draw_path_segment( start, end,
                                path_faded( value.blend( white, glow * 0.75f ), PATH_COMET_FLOOR + ( 1.0f - PATH_COMET_FLOOR ) * glow ),
                                false, pen.m_outline );

    const f32 into{ head - static_cast< f32 >( at - 1 ) };

    if ( into < 0.0f || into >= 1.0f )
    {
        return;
    }

    const vec3 spark{ start + ( end - start ) * into };

    ragebot::draw_path_segment( spark - vec3{ PATH_COMET_SPARK, 0.0f, 0.0f }, spark + vec3{ PATH_COMET_SPARK, 0.0f, 0.0f }, white, false, pen.m_outline );
    ragebot::draw_path_segment( spark - vec3{ 0.0f, PATH_COMET_SPARK, 0.0f }, spark + vec3{ 0.0f, PATH_COMET_SPARK, 0.0f }, white, false, pen.m_outline );
    ragebot::draw_path_segment( spark - vec3{ 0.0f, 0.0f, PATH_COMET_SPARK }, spark + vec3{ 0.0f, 0.0f, PATH_COMET_SPARK }, white, false, pen.m_outline );
}

static void draw_flow_step( const ragebot::path_pen_t &pen, const vec3 &start, const vec3 &end,
                            const color_t &value, i32 at, i32 count )
{
    ragebot::draw_path_segment( start, end, path_faded( value, 0.3f ), false, pen.m_outline );

    const vec3 span{ end - start };
    const f32  length{ span.length( ) };

    if ( length <= 0.1f )
    {
        return;
    }

    const f32 first{ path_cycle( pen.m_clock, PATH_FLOW_SPEED, PATH_FLOW_SPACING ) + 1.0f };
    const f32 lap{ ( static_cast< f32 >( at - 1 ) - first ) / static_cast< f32 >( PATH_FLOW_SPACING ) };
    i32       index{ static_cast< i32 >( lap ) };

    if ( static_cast< f32 >( index ) < lap )
    {
        ++index;
    }

    const f32 spot{ first + static_cast< f32 >( math::bigger( index, 0 ) * PATH_FLOW_SPACING ) };

    if ( spot >= static_cast< f32 >( at ) || spot >= static_cast< f32 >( count - 1 ) )
    {
        return;
    }

    const vec3 direction{ span * ( 1.0f / length ) };
    const vec3 tip{ start + span * ( spot - static_cast< f32 >( at - 1 ) ) };
    const vec3 back{ tip - direction * PATH_FLOW_SIZE };
    const vec3 side{ path_side( direction ) * PATH_FLOW_SPREAD };

    ragebot::draw_path_segment( tip, back + side, value, false, pen.m_outline );
    ragebot::draw_path_segment( tip, back - side, value, false, pen.m_outline );
}

ragebot::path_pen_t ragebot::path_pen( usize style, bool outline )
{
    return path_pen_t{ style, g_tf2.m_engine->Plat_FloatTime( ), outline || style == config::PATH_STYLE_OUTLINED };
}

void ragebot::draw_path_step( const path_pen_t &pen, const vec3 *points, i32 count, i32 at, const color_t &value )
{
    const vec3 &start{ points[ at - 1 ] };
    const vec3 &end{ points[ at ] };

    switch ( pen.m_style )
    {
        case config::PATH_STYLE_SEPARATORS: draw_path_segment( start, end, value, at % 4 == 0, pen.m_outline ); break;
        case config::PATH_STYLE_TIMELINE: draw_timeline_step( pen, start, end, value, at ); break;
        case config::PATH_STYLE_COMET: draw_comet_step( pen, start, end, value, at, count ); break;
        case config::PATH_STYLE_FLOW: draw_flow_step( pen, start, end, value, at, count ); break;
        default: draw_path_segment( start, end, value, false, pen.m_outline ); break;
    }
}

f32 ragebot::path_travel_alpha( i32 segment, double elapsed )
{
    constexpr f32 FADE_TICKS{ static_cast< f32 >( PATH_TRAVEL_FADE_TICKS ) };
    const f32     traveled{ static_cast< f32 >( elapsed / TICK_INTERVAL ) };
    const f32     age{ traveled - static_cast< f32 >( segment ) };

    return age <= 0.0f ? 1.0f : age >= FADE_TICKS ? 0.0f
                                                  : 1.0f - age / FADE_TICKS;
}

void ragebot::paint_target_path( ) const
{
    if ( !g_cfg.m_ragebot.m_projectile_draw_path )
    {
        return;
    }

    const double         now{ g_tf2.m_engine->Plat_FloatTime( ) };
    const bool           live{ this->m_path_time > this->m_path_hold_until };
    const target_path_t &path{ live ? this->m_path : this->m_fired_path };
    if ( path.m_count < 2 )
    {
        return;
    }

    const bool   animate{ g_cfg.m_ragebot.m_projectile_path_animate && !live };
    f32          alpha{ 1.0f };
    const double idle{ now - ( live ? this->m_path_time : this->m_path_hold_until ) - 0.25 };
    if ( !animate && idle > 0.0 )
    {
        if ( !g_cfg.m_ragebot.m_projectile_path_fade )
        {
            return;
        }

        alpha = 1.0f - static_cast< f32 >( idle ) / math::bigger( g_cfg.m_ragebot.m_projectile_path_fade_time, 0.1f );
        if ( alpha <= 0.0f )
        {
            return;
        }
    }

    const color_t &head{ g_cfg.m_ragebot.m_projectile_path_color };
    const color_t &tail{ g_cfg.m_ragebot.m_projectile_path_color_end };
    const bool     gradient{ g_cfg.m_ragebot.m_projectile_path_gradient };
    const double   travel_time{ now - this->m_fired_path_time };

    const path_pen_t pen{ path_pen( g_cfg.m_ragebot.m_projectile_path_style, g_cfg.m_ragebot.m_projectile_path_outline ) };

    for ( i32 at{ 1 }; at < path.m_count; ++at )
    {
        color_t value{ gradient ? head.blend( tail, static_cast< f32 >( at ) / static_cast< f32 >( path.m_count - 1 ) ) : head };

        // Keep the intercept endpoint visible, but distinguish the tail where
        // plausible ground controllers no longer agree within half a player hull.
        if ( path.m_ground_agreement_ticks >= 0 && at > path.m_ground_agreement_ticks )
        {
            value = g_cfg.m_ragebot.m_projectile_path_low_confidence_color;
        }
        const f32 segment_alpha{ animate ? this->path_travel_alpha( at, travel_time ) : 1.0f };
        if ( segment_alpha <= 0.0f )
        {
            continue;
        }
        value.a = static_cast< u8 >( static_cast< f32 >( value.a ) * alpha * segment_alpha );

        draw_path_step( pen, path.m_points, path.m_count, at, value );
    }

    if ( !g_cfg.m_ragebot.m_projectile_path_bounds )
    {
        return;
    }

    const f32 bounds_alpha{ animate ? this->path_travel_alpha( path.m_count - 1, travel_time ) : 1.0f };
    if ( bounds_alpha <= 0.0f )
    {
        return;
    }

    color_t value{ g_cfg.m_ragebot.m_projectile_path_bounds_color };

    if ( path.m_ground_agreement_ticks >= 0 && path.m_count - 1 > path.m_ground_agreement_ticks )
    {
        value = g_cfg.m_ragebot.m_projectile_path_low_confidence_color;
    }
    value.a = static_cast< u8 >( static_cast< f32 >( value.a ) * alpha * bounds_alpha );

    const vec3 low{ path.m_bounds_origin + path.m_bounds_mins };
    const vec3 high{ path.m_bounds_origin + path.m_bounds_maxs };
    const bool corners_only{ g_cfg.m_ragebot.m_projectile_path_bounds_style == config::PATH_BOUNDS_CORNERS };

    for ( i32 index{ 0 }; index < 8; ++index )
    {
        const vec3 from{ math::box_corner( low, high, index ) };

        for ( i32 axis{ 0 }; axis < 3; ++axis )
        {
            const i32 neighbour{ index ^ ( 1 << axis ) };

            // every edge is shared by two corners; the full box only needs it once
            if ( !corners_only && neighbour < index )
            {
                continue;
            }

            const vec3 to{ math::box_corner( low, high, neighbour ) };

            draw_path_segment( from, corners_only ? from + ( to - from ) * 0.25f : to, value );
        }
    }
}

void ragebot::paint_fov_circle( ) const
{
    if ( !g_cfg.m_ragebot.m_draw_fov )
    {
        return;
    }

    const f32 half_cone{ static_cast< f32 >( g_cfg.m_ragebot.m_fov ) };
    const f32 half_view{ g_cl.m_view_fov * 0.5f };

    if ( half_cone <= 0.0f || half_cone >= 90.0f || half_view <= 0.0f || half_view >= 90.0f )
    {
        return;
    }

    const f32 width{ static_cast< f32 >( g_cl.m_screen_width ) };
    const f32 height{ static_cast< f32 >( g_cl.m_screen_height ) };

    const f32 radius{ math::tan( DEG2RAD( half_cone ) ) / math::tan( DEG2RAD( half_view ) ) *
                      ( width * 0.5f ) };

    const vec2 center{ width * 0.5f, height * 0.5f };

    g_draw.circle( center, g_cfg.m_ragebot.m_fov_color, radius, 1.5f );
}

i32 ragebot::extreme_seeds( const salvo_mask_t &mask, vec3 *points, i32 *hitboxes ) const
{
    f32 bounds[ 4 ]{ FLT_MAX, -FLT_MAX, FLT_MAX, -FLT_MAX };
    i32 owners[ 4 ]{ -1, -1, -1, -1 };

    for ( i32 hitbox{ }; hitbox < mask.m_hitboxes; ++hitbox )
    {
        const salvo_mask_t::rect_t &rect{ mask.m_rects[ hitbox ] };
        if ( !rect.m_valid || !this->is_hitbox_allowed_for_current_state( hitbox ) )
        {
            continue;
        }

        if ( rect.m_low_u < bounds[ 0 ] )
        {
            bounds[ 0 ] = rect.m_low_u;
            owners[ 0 ] = hitbox;
        }
        if ( rect.m_high_u > bounds[ 1 ] )
        {
            bounds[ 1 ] = rect.m_high_u;
            owners[ 1 ] = hitbox;
        }
        if ( rect.m_low_v < bounds[ 2 ] )
        {
            bounds[ 2 ] = rect.m_low_v;
            owners[ 2 ] = hitbox;
        }
        if ( rect.m_high_v > bounds[ 3 ] )
        {
            bounds[ 3 ] = rect.m_high_v;
            owners[ 3 ] = hitbox;
        }
    }

    i32 count{ };
    for ( i32 bound{ }; bound < 4; ++bound )
    {
        const i32 hitbox{ owners[ bound ] };
        if ( hitbox < 0 )
        {
            continue;
        }

        const mstudiobbox_t *box{ mask.m_set->pHitbox( hitbox ) };
        const vec3          &corner{ mask.m_rects[ hitbox ].m_extreme[ bound ] };

        vec3 centre{ };
        math::vector_transform( ( box->bbmin + box->bbmax ) * 0.5f, mask.m_pose->m_bones[ box->bone ], centre );

        // the point scale is how far out on the box the seed sits; the rest of the way pulls it in
        const f32 inset{ 1.0f - static_cast< f32 >( math::clamp( this->m_hitscan_profile.m_hitscan_multipoint_scale, 1, 100 ) ) / 100.0f };

        points[ count ]     = corner + ( centre - corner ) * inset;
        hitboxes[ count++ ] = hitbox;
    }

    return count;
}

void ragebot::build_kernel( salvo_kernel_t &kernel ) const
{
    kernel.m_count    = 0;
    kernel.m_weight   = 0.0f;
    kernel.m_spread   = 0.0f;
    kernel.m_is_prior = false;

    C_TFWeaponBase     *weapon{ g_cl.m_weapon };
    const WeaponData_t &data{ weapon->GetWeaponInfo( )->GetWeaponData( weapon->m_iWeaponMode( ) ) };

    if ( data.m_iProjectile != TF_PROJECTILE_BULLET || data.m_nBulletsPerShot < 1 )
    {
        return;
    }

    const i32 bullets{ math::clamp(
        static_cast< i32 >( g_cl.attrib_hook( weapon, static_cast< f32 >( data.m_nBulletsPerShot ),
                                              ATLAS_XORSTR( "mult_bullets_per_shot" ).get( ) ) ),
        1, MAX_BULLETS ) };

    kernel.m_spread = g_nospread.weapon_spread( data.m_flSpread );
    kernel.m_range  = data.m_flRange;

    if ( kernel.m_spread <= 0.0f )
    {
        kernel_add( kernel, 0.0f, 0.0f, static_cast< f32 >( bullets ) );
    } else if ( data.m_nBulletsPerShot > 1 && ( weapon->GetDamageType( ) & DMG_BUCKSHOT ) != 0 && g_nospread.fixed_spread( ) )
    {
        for ( i32 index{ }; index < bullets; ++index )
        {
            const vec2 &cell{ g_vecFixedWpnSpreadPellets[ index % 10 ] };
            kernel_add( kernel, cell.x * 0.5f, cell.y * 0.5f, 1.0f );
        }
    } else
    {
        const f32  since_fire{ g_tf2.m_globals->curtime - g_nospread.pre_last_fire( ) };
        const bool bonus{ since_fire > ( bullets > 1 ? 0.25f : 1.25f ) };
        const f32  first_variance{ bonus ? g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "mult_spread_scale_first_shot" ).get( ) )
                                         : 0.5f };

        const spread_table_t &table{ g_nospread.spread_table( ) };

        seed_candidate_t candidates[ MAX_CANDIDATES * 2 ]{ };
        usize            count{ };

        if ( !g_tf2.sv_usercmd_custom_random_seed->GetBool( ) || g_nospread.synced( ) )
        {
            count = g_nospread.candidates( g_networking.send_delay( ).ticks( ),
                                           candidates );
        }

        if ( count > 0 )
        {
            f32 total{ };
            for ( usize index{ }; index < count; ++index )
            {
                total += candidates[ index ].m_weight;
            }

            f32 weights[ SPREAD_SEEDS + MAX_BULLETS ]{ };
            for ( usize index{ }; index < count; ++index )
            {
                const f32 share{ candidates[ index ].m_weight / total };

                for ( i32 pellet{ }; pellet < bullets; ++pellet )
                {
                    const f32 variance{ pellet == 0 ? first_variance : 0.5f };
                    if ( variance == 0.0f )
                    {
                        kernel_add( kernel, 0.0f, 0.0f, share );
                        continue;
                    }

                    const i32 seed{ candidates[ index ].m_seed + pellet };
                    if ( variance == 0.5f )
                    {
                        weights[ seed ] += share;
                    } else
                    {
                        const vec2 &offset{ table.m_offset[ seed ] };
                        kernel_add( kernel, offset.x * variance * 2.0f, offset.y * variance * 2.0f, share );
                    }
                }
            }

            for ( i32 seed{ }; seed < SPREAD_SEEDS + bullets - 1; ++seed )
            {
                if ( weights[ seed ] > 0.0f )
                {
                    kernel_add( kernel, table.m_offset[ seed ].x, table.m_offset[ seed ].y, weights[ seed ] );
                }
            }
        } else
        {
            const i32 first{ bonus && first_variance == 0.0f ? 1 : 0 };
            if ( first )
            {
                kernel_add( kernel, 0.0f, 0.0f, 1.0f );
            }

            for ( i32 seed{ first }; seed < SPREAD_SEEDS + bullets - 1; ++seed )
            {
                const i32 low{ math::bigger( first, seed - ( SPREAD_SEEDS - 1 ) ) };
                const i32 high{ math::smaller( bullets - 1, seed ) };

                if ( high < low )
                {
                    continue; // a single perfect bullet leaves no random pellet to cover this seed
                }

                kernel_add( kernel, table.m_offset[ seed ].x, table.m_offset[ seed ].y,
                            static_cast< f32 >( high - low + 1 ) / static_cast< f32 >( SPREAD_SEEDS ) );
            }

            kernel.m_is_prior = true;
        }
    }

    atl::sort( kernel.m_entries, kernel.m_entries + kernel.m_count, []( const kernel_entry_t &first, const kernel_entry_t &second )
    {
        if ( first.m_weight != second.m_weight )
        {
            return first.m_weight > second.m_weight;
        }

        return first.m_x * first.m_x + first.m_y * first.m_y < second.m_x * second.m_x + second.m_y * second.m_y;
    } );

    for ( i32 index{ }; index < kernel.m_count; ++index )
    {
        kernel.m_weight += kernel.m_entries[ index ].m_weight;
    }
}

// fuck off
static const i32 box_iterator[] = {
    HITBOX_PELVIS,
    HITBOX_SPINE_0,
    HITBOX_SPINE_1,
    HITBOX_SPINE_2,
    HITBOX_SPINE_3,
    HITBOX_HEAD,
    HITBOX_UPPERARM_L,
    HITBOX_LOWERARM_L,
    HITBOX_HAND_L,
    HITBOX_UPPERARM_R,
    HITBOX_LOWERARM_R,
    HITBOX_HAND_R,
    HITBOX_HIP_L,
    HITBOX_KNEE_L,
    HITBOX_FOOT_L,
    HITBOX_HIP_R,
    HITBOX_KNEE_R,
    HITBOX_FOOT_R,
};

bool ragebot::scan_pose(
    const mstudiohitboxset_t *set,
    const scan_pose_t        &pose,
    i32                       hitbox_count,
    i32                       priority_box,
    C_TFPlayer               *player,
    const vec3               &shoot_pos,
    i32                       declare_tick,
    bool                      newest,
    ragebot_target_t         &out )
{
    vec3 seed{ };
    vec3 seed_angle{ };
    i32  seed_hitbox{ -1 };
    bool doubt{ };

    const auto real_priority_box{ box_iterator[ priority_box ] };

    const auto take{ [ & ]( i32 hitbox )
    {
        if ( !this->trace_hitbox( set, pose, hitbox, player, shoot_pos, seed, seed_angle, doubt ) )
        {
            return false;
        }

        out.m_hitbox = seed_hitbox = hitbox;
        return true;
    } };

    if ( !take( real_priority_box ) )
    {
        for ( const auto &box : box_iterator )
        {
            if ( box == real_priority_box )
            {
                continue;
            }

            if ( !this->is_hitbox_allowed_for_current_state( box ) )
            {
                continue;
            }

            if ( take( box ) )
            {
                break;
            }
        }
    }

    const auto take_seed{ [ & ]( )
    {
        out.m_aim_origin   = seed;
        out.m_aim_angle    = seed_angle;
        out.m_got_hitpoint = true;
        out.m_tick_count   = declare_tick;
        out.m_steered      = false;
    } };

    // the first shot selects the batch angle;
    const bool scoring{ m_kernel.m_count > 0 && ( seed_hitbox >= 0 || newest ) };
    if ( !scoring )
    {
        if ( seed_hitbox < 0 )
        {
            return false;
        }

        take_seed( );
        this->remember_shot( set, pose, hitbox_count, seed_hitbox, shoot_pos, out.m_aim_origin );
        return true;
    }

    salvo_mask_t mask{ };
    mask.m_trace_limit = m_kernel.m_is_prior ? PRIOR_TRACE_BUDGET : TRACE_BUDGET;
    const bool masked{ mask_setup( mask, set, pose, hitbox_count, player, shoot_pos, m_kernel.m_spread, m_kernel.m_range ) };

    if ( seed_hitbox < 0 && newest && masked )
    {
        vec3 points[ 4 ]{ };
        i32  owners[ 4 ]{ };

        const i32 count{ this->extreme_seeds( mask, points, owners ) };
        for ( i32 at{ }; at < count && seed_hitbox < 0; ++at )
        {
            if ( !this->within_fov( shoot_pos, points[ at ] ) ||
                 !g_cl.trace_line( g_cl.m_local, player, shoot_pos, points[ at ], MASK_SOLID | CONTENTS_HITBOX ) ||
                 !this->shot_registers( set, pose, player, shoot_pos, points[ at ] ) )
            {
                continue;
            }

            seed        = points[ at ];
            seed_hitbox = owners[ at ];
            math::vector_angles( seed - shoot_pos, seed_angle );
        }
    }

    if ( seed_hitbox < 0 )
    {
        return false;
    }

    take_seed( );

    f32 u{ };
    f32 v{ };
    if ( !masked || !mask_project( mask, seed, u, v ) )
    {
        this->remember_shot( set, pose, hitbox_count, seed_hitbox, shoot_pos, out.m_aim_origin );
        return true;
    }

    f32 a{ u / mask.m_cell };
    f32 b{ v / mask.m_cell };

    // the roll belongs to this pose: the long side it lines the pattern up with comes from these hitboxes
    const vec3            punch{ g_cl.m_local->m_vecPunchAngle( ) };
    const f32             roll{ this->m_hitscan_profile.m_hitscan_roll_angles && !m_kernel.m_is_prior
                                    ? no_spread::nearest_free_roll( salvo_roll( mask, m_kernel ) - punch.z, seed_angle - punch ) + punch.z
                                    : 0.0f };
    const salvo_kernel_t &kernel{ roll != 0.0f ? roll_kernel( m_kernel, roll, this->m_rolled_kernel ) : m_kernel };

    const f32           seed_a{ a };
    const f32           seed_b{ b };
    const salvo_score_t at_seed{ salvo_score( mask, kernel, a, b ) };
    salvo_score_t       best{ at_seed };

    if ( !doubt )
    {
        best          = refine_aim( mask, kernel, a, b, at_seed );
        out.m_steered = best.m_hits > 0.0f;
    }

    if ( a != seed_a || b != seed_b )
    {
        const vec3 point{ mask_point( mask, a, b ) };
        if ( this->within_fov( shoot_pos, point ) )
        {
            out.m_aim_origin = point;
            math::vector_angles( point - shoot_pos, out.m_aim_angle );
        } else
        {
            best          = at_seed;
            out.m_steered = false;
        }
    }
    if ( out.m_steered && roll != 0.0f )
    {
        vec3 sent{ out.m_aim_angle - punch };
        sent.z = roll - punch.z;
        if ( no_spread::rolls_freely( sent ) )
        {
            out.m_aim_angle.z = roll;
        } else
        {
            take_seed( );
            best = at_seed;
        }
    }

    this->remember_shot( set, pose, hitbox_count, seed_hitbox, shoot_pos, out.m_aim_origin );

    const f32 coverage{ mask.m_open_hits > 0.0f ? best.m_hits / mask.m_open_hits : 0.0f };
    return coverage * 100.0f >= static_cast< f32 >( this->m_hitscan_profile.m_hitscan_hitchance );
}

bool ragebot::get_hitscan_aimpoint( ragebot_target_t &out )
{
    const auto player{ out.m_entity->As< C_TFPlayer * >( ) };

    i32 priority_box{ ( i32 )this->get_best_priority_hitbox( player ) };

    const CStudioHdr *studio{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< CStudioHdr * >( player ) };
    if ( !studio || !studio->m_pStudioHdr )
    {
        return false;
    }

    const studiohdr_t *hdr{ studio->m_pStudioHdr };

    const auto set{ hdr->pHitboxSet( player->m_nHitboxSet( ) ) };
    if ( !set || set->numhitboxes <= 0 )
    {
        return false;
    }

    const i32  hitbox_count{ math::smaller( static_cast< i32 >( HITBOX_MAX ), set->numhitboxes ) };
    const i32  index{ player->entindex( ) };
    const auto shoot_pos{ g_cl.m_local->Weapon_ShootPosition( ) };

    // TODO: Hoku; Maybe use the lagcompensation record for this tick instead? Meh...
    if ( !this->m_hitscan_profile.m_use_history )
    {
        matrix3x4 matrix[ MAX_STUDIO_BONES ]{ };
        if ( !player->SetupBones( matrix, MAX_STUDIO_BONES, BONE_USED_BY_HITBOX, player->m_flSimulationTime( ) ) )
        {
            return false;
        }

        const i32         tick{ TIME_TO_TICKS( player->m_flSimulationTime( ) ) };
        const scan_pose_t pose{ matrix, player->m_vecOrigin( ), tick, player->m_flModelScale( ), player->m_vecMinsPreScaled( ), player->m_vecMaxsPreScaled( ) };

        return this->scan_pose( set, pose, hitbox_count, priority_box, player, shoot_pos, tick, true, out );
    }

    const warp_plan_t     plan{ g_extrapolate.plan( index ) };
    const rewind_window_t window{ g_lagcomp.rewind_window( ) };
    bool                  scanned_newest{ };
    const auto            scan_predicted{ [ & ]( ) -> bool
    {
        if ( !g_cfg.m_lagcomp.m_extrapolate || !plan.m_valid )
        {
            return false;
        }

        if ( plan.m_commands == 0 && scanned_newest && !g_lagcomp.loose( index ) )
        {
            return false;
        }

        if ( window.m_server_tick < 0 )
        {
            return false;
        }

        lag_record_t synthetic{ };
        const auto   predicted{ g_extrapolate.synthesize( index, plan.m_commands, synthetic ) };

        if ( !predicted || predicted->m_header != hdr )
        {
            return false;
        }

        matrix3x4 matrix[ MAX_STUDIO_BONES ]{ };
        if ( !g_lagcomp.build_bones( player, predicted, BONE_USED_BY_HITBOX, matrix, MAX_STUDIO_BONES ) )
        {
            return false;
        }

        i32 declare{ predicted->m_tick };
        while ( window.m_correct - window.age( declare ) > 0.2f )
        {
            --declare;
        }

        const scan_pose_t pose{ matrix, predicted->m_pose.m_origin, predicted->m_tick, predicted->m_pose.m_scale, predicted->m_mins, predicted->m_maxs };

        return this->scan_pose( set, pose, hitbox_count, priority_box, player, shoot_pos, declare, true, out );
    } };

    if ( plan.m_breaks && scan_predicted( ) )
    {
        return true;
    }

    bool first_reachable{ true };
    for ( i32 at{ 0 }; at < g_lagcomp.count( index ); ++at )
    {
        const auto record{ g_lagcomp.record( index, at ) };

        if ( !record || record->m_header != hdr || !g_lagcomp.continuous( index, *record ) || !window.accepts( record->m_tick ) )
        {
            continue;
        }

        scanned_newest |= at == 0;

        const scan_pose_t pose{ record->m_bones, record->m_pose.m_origin, record->m_tick, record->m_pose.m_scale, record->m_mins, record->m_maxs };
        if ( this->scan_pose( set, pose, hitbox_count, priority_box, player, shoot_pos, record->m_tick, first_reachable, out ) )
        {
            return true;
        }

        first_reachable = false;
    }

    return !plan.m_breaks && scan_predicted( );
}

i32 ragebot::network_ticks( i32 hold ) const
{
    const f32 latency{ g_tf2.m_cl->m_NetChannel->GetLatency( FLOW_INCOMING ) + g_tf2.m_cl->m_NetChannel->GetLatency( FLOW_OUTGOING ) };
    return TIME_TO_TICKS( latency ) + g_networking.send_delay( hold ).ticks( ) + TIME_TO_TICKS( lag_compensation::lerp_time( ) );
}

i32 ragebot::target_catch_up_ticks( C_TFPlayer *player, i32 hold ) const
{
    const i32                delay{ g_networking.send_delay( hold ).ticks( ) };
    const i32                frame{ g_tickbase.send_frame( ) };
    const velocity_sample_t *seed{ g_player_data.velocity_sample( player->entindex( ), 0 ) };
    if ( !seed || frame < 0 )
    {
        const i32 fallback{ math::clamp( this->network_ticks( hold ), 0, MAX_CATCH_UP_TICKS ) };
        return fallback;
    }

    const i32 seed_server_tick{ seed->m_tick - seed->m_lead };
    const i32 corrected{ math::clamp( frame + delay - seed_server_tick,
                                      0, MAX_CATCH_UP_TICKS ) };

    return corrected;
}

i32 ragebot::target_catch_up_epoch( C_TFPlayer *player, i32 server_epoch ) const
{
    const velocity_sample_t *seed{ g_player_data.velocity_sample( player->entindex( ), 0 ) };
    if ( !seed || server_epoch < 0 )
    {
        return math::clamp( this->network_ticks( 0 ), 0, MAX_CATCH_UP_TICKS );
    }

    const i32 seed_server_tick{ seed->m_tick - seed->m_lead };
    return math::clamp( server_epoch - seed_server_tick, 0, MAX_CATCH_UP_TICKS );
}

i32 ragebot::projectile_catch_up_ticks( C_TFPlayer *player ) const
{
    if ( g_cl.is_delayed_throw_weapon( g_cl.m_weapon ) )
    {
        return this->target_catch_up_epoch( player, g_cl.throw_spawn_tick( ) );
    }

    // the command that fires is choked one tick, which puts the shot on the server a frame later
    return this->target_catch_up_ticks( player, 1 );
}

bool ragebot::is_bbox_pos_allowed_for_current_state( i32 bbox_pos ) const
{
    switch ( bbox_pos )
    {
        case bbox_pos_top: return this->m_projectile_profile.m_projectile_aimpoints[ config::PROJECTILE_BBOX_TOP ] || this->m_projectile_profile.m_aimpoint_priority == config::BBOX_AIMPOINT_PRIORITY_TOP;
        case bbox_pos_mid: return this->m_projectile_profile.m_projectile_aimpoints[ config::PROJECTILE_BBOX_MIDDLE ] || this->m_projectile_profile.m_aimpoint_priority == config::BBOX_AIMPOINT_PRIORITY_MID;
        case bbox_pos_bottom: return this->m_projectile_profile.m_projectile_aimpoints[ config::PROJECTILE_BBOX_BOTTOM ] || this->m_projectile_profile.m_aimpoint_priority == config::BBOX_AIMPOINT_PRIORITY_BOTTOM;
        default: return false;
    }
}

i32 ragebot::priority_bbox_pos( const move_sim_ctx_t *ctx ) const
{
    const usize priority{ this->m_projectile_profile.m_aimpoint_priority };
    if ( priority != config::BBOX_AIMPOINT_PRIORITY_AUTO )
    {
        return static_cast< i32 >( priority );
    }

    // Need to better fine tune the distance here, but the idea is:
    // If we're too far with a vphysics weapon (like a pipe), if we shoot the feet we'll just get fucked over by spin and other quirks.
    // If we're too close with a direct hit, shooting the feet will be unreliable as there is less hitbox space to work with.
    // This bool prevents all of those quirks by being selective about bbox positions.
    // bool is_close_enough_tolerance{ vec3{ ctx->m_pred_origin - g_cl.m_local->m_vecOrigin( ) }.length2d( ) < 96.f };

    switch ( g_cl.m_weapon->GetWeaponID( ) )
    {
        case TF_WEAPON_COMPOUND_BOW:
        {
            return ( i32 )g_cl.m_weapon->As< C_TFWeaponBaseGun * >( )->GetProjectileDamage( ) > ctx->m_player_to_predict->m_iHealth( ) ? bbox_pos_mid : bbox_pos_top;
        }

        case TF_WEAPON_PIPEBOMBLAUNCHER:
        case TF_WEAPON_GRENADELAUNCHER: return bbox_pos_mid;
        case TF_WEAPON_ROCKETLAUNCHER:
        case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT: break;
        case TF_WEAPON_FLAREGUN:
        case TF_WEAPON_CROSSBOW:
        case TF_WEAPON_PARTICLE_CANNON:
        case TF_WEAPON_DRG_POMSON:
        case TF_WEAPON_RAYGUN:
        case TF_WEAPON_FLAREGUN_REVENGE:
        case TF_WEAPON_CANNON:
        case TF_WEAPON_SYRINGEGUN_MEDIC:
        case TF_WEAPON_FLAME_BALL:
        case TF_WEAPON_FLAMETHROWER:
        case TF_WEAPON_SHOTGUN_BUILDING_RESCUE:
        case TF_WEAPON_CLEAVER:
        case TF_WEAPON_JAR:
        case TF_WEAPON_JAR_MILK:
        case TF_WEAPON_JAR_GAS: return bbox_pos_mid; // Always shoot the center on these weapons.
        default: return bbox_pos_mid;
    }

    // trust me on this one, if we dont do this the auto pos getter shits itself in targets in air.
    return ctx->m_player_to_predict->IsOnGround( ) ? bbox_pos_bottom : bbox_pos_mid;
}

i32 ragebot::aim_order( const move_sim_ctx_t *ctx, i32 *order ) const
{
    static constexpr i32 FALLBACK[]{ bbox_pos_mid, bbox_pos_bottom, bbox_pos_top };

    const i32 first{ this->priority_bbox_pos( ctx ) };

    i32 count{ 0 };
    if ( this->is_bbox_pos_allowed_for_current_state( first ) )
    {
        order[ count++ ] = first;
    }

    for ( const i32 pos : FALLBACK )
    {
        if ( pos != first && this->is_bbox_pos_allowed_for_current_state( pos ) )
        {
            order[ count++ ] = pos;
        }
    }

    return count;
}

vec3 ragebot::muzzle_towards( const vec3 &shoot_pos, const vec3 &point ) const
{
    return shoot_pos + ( point - shoot_pos ).normalized( ) * m_weapon_data.m_muzzle_forward;
}

// nothing on the eye's own line to the point means no muzzle on the roll circle does better
bool ragebot::eye_sees( const vec3 &point ) const
{
    trace_t                sight{ };
    Ray_t                  ray{ };
    CTraceFilterProjectile filter{ };

    filter.m_pSkip = g_cl.m_local;

    ray.Init( g_cl.m_local->Weapon_ShootPosition( ), point );
    g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &sight );

    return sight.fraction >= 1.0f;
}

constexpr f32 LAUNCH_ROLLS[]{ 0.0f, 180.0f, 90.0f, -90.0f };

bool ragebot::rolls_allowed( const vec3 &target ) const
{
    return !g_holdaim_mgr.held( ) && this->m_projectile_profile.m_projectile_roll_angles &&
           g_cl.m_local->m_iClass( ) != TF_CLASS_DEMOMAN && ( m_weapon_data.m_is_sticky || this->eye_sees( target ) );
}

bool ragebot::quarter_roll_allowed( ) const
{
    return g_cl.m_cmd->forwardmove == 0.0f && g_cl.m_cmd->sidemove == 0.0f &&
           !( g_cl.m_local->IsOnGround( ) && g_tf2.tf_clamp_back_speed->GetFloat( ) < 1.0f &&
              g_cl.m_local->m_vecVelocity( ).length( ) > g_tf2.tf_clamp_back_speed_min->GetFloat( ) );
}

bool ragebot::straight_rocket( ) const
{
    return !m_weapon_data.m_is_vphysics && m_weapon_data.m_gravity_mod == 0.0f && m_weapon_data.m_launch_spread == 0.0f &&
           m_weapon_data.m_hull_mins.is_zero( ) && m_weapon_data.m_hull_maxs.is_zero( );
}

ATLAS_FORCE_INLINE vec3 line_offset( const projectile_setup_t &setup, const vec3 &point )
{
    vec3 direction{ };
    math::angle_vectors( setup.m_angles, &direction );
    const vec3 offset{ point - setup.m_position };
    return offset - direction * offset.dot( direction );
}

ATLAS_FORCE_INLINE f32 line_miss( const projectile_setup_t &setup, const vec3 &point )
{
    return line_offset( setup, point ).length( );
}

ATLAS_FORCE_INLINE usize face_axis( const vec3 &normal )
{
    if ( normal.x != 0.0f )
    {
        return 0;
    }
    return normal.y != 0.0f ? 1 : 2;
}

bool ragebot::aim_rocket( const vec3 &want, const move_sim_ctx_t *ctx, i32 catch_up, f32 roll, vec3 &angle ) const
{
    const vec3 eye{ g_cl.m_local->Weapon_ShootPosition( ) };
    const vec3 origin{ ctx->m_track[ catch_up ] };
    const vec3 mins{ origin + ctx->m_pred_mins }, maxs{ origin + ctx->m_pred_maxs };
    const f32  distance{ want.dist_to( eye ) };
    vec3       view{ };
    math::vector_angles( want - eye, view );
    view.z = roll;

    if ( distance > client::PROJECTILE_AIM_DISTANCE * client::PROJECTILE_AIM_FRACTION && distance <= client::PROJECTILE_AIM_DISTANCE )
    {
        if ( math::segment_box_fraction( eye, want, mins, maxs ) != FLT_MAX )
        {
            return false;
        }

        angle = view;
        return true;
    }

    const vec3 offset{ g_cl.projectile_fire_offset( g_cl.m_weapon, g_proj_sim.get_fire_offset( g_cl.m_weapon, g_cl.m_local->IsDucked( ) ) ) };
    vec3       chosen{ view };
    f32        miss{ FLT_MAX };
    for ( ;; )
    {
        vec3 forward{ }, right{ }, up{ };
        math::angle_vectors( view, &forward, &right, &up );
        const vec3 muzzle{ forward * offset.x + right * offset.y + up * offset.z };
        const vec3 line{ ( forward * client::PROJECTILE_AIM_DISTANCE - muzzle ).normalized( ) };
        const vec3 toward{ want - eye - muzzle };
        const f32  error{ ( toward - line * toward.dot( line ) ).length( ) };

        if ( !( error < miss * 0.5f ) )
        {
            break;
        }

        miss   = error;
        chosen = view;

        const vec3 through{ toward.normalized( ) };
        const f32  along{ through.dot( muzzle ) };
        const f32  reach{ -along + math::sqrt( along * along - muzzle.length_sqr( ) +
                                               client::PROJECTILE_AIM_DISTANCE * client::PROJECTILE_AIM_DISTANCE ) };
        math::vector_angles( muzzle + through * reach, view );
        view.z = roll;
    }

    if ( distance > client::PROJECTILE_AIM_DISTANCE )
    {
        vec3 forward{ };
        math::angle_vectors( chosen, &forward );
        const f32 box{ math::segment_box_fraction( eye, eye + forward * client::PROJECTILE_AIM_DISTANCE, mins, maxs ) };
        if ( box != FLT_MAX && box > client::PROJECTILE_AIM_FRACTION )
        {
            return false;
        }
    }

    angle = chosen;
    return true;
}

bool ragebot::gather_launch( const vec3 &angle, const move_sim_ctx_t *ctx, i32 catch_up, projectile_setup_t &setup ) const
{
    if ( ( m_launch_setup.m_model == projectile_model::sticky || m_launch_setup.m_model == projectile_model::pipe ||
           m_launch_setup.m_model == projectile_model::cannonball ) &&
         m_weapon_data.m_launch_spread <= 0.0f )
    {
        setup           = m_launch_setup;
        setup.m_angles  = angle;
        setup.m_blocked = !g_cl.projectile_grenade_setup( g_cl.m_local, g_cl.m_weapon, angle, setup.m_position );
        return !setup.m_blocked;
    }

    projectile_target_t at_fire{ };
    if ( ctx )
    {
        at_fire = projectile_target_t{ ctx->m_track + catch_up, ctx->m_pred_mins, ctx->m_pred_maxs, ctx->m_player_to_predict };
    }

    switch ( g_cl.m_weapon->GetWeaponID( ) )
    {
        case TF_WEAPON_ROCKETLAUNCHER:
        case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
        case TF_WEAPON_PARTICLE_CANNON:
        {
            if ( m_weapon_data.m_launch_spread != 0.0f )
            {
                break;
            }
            setup = m_launch_setup;
            g_cl.projectile_fire_setup( g_cl.m_local, g_cl.m_weapon, angle, g_proj_sim.get_fire_offset( g_cl.m_weapon, g_cl.m_local->IsDucked( ) ),
                                        false, setup.m_position, setup.m_angles, ctx ? &at_fire : nullptr );
            g_proj_sim.snap_to_eye( g_cl.m_local, setup.m_position );
            return true;
        }
        default: break;
    }
    return g_proj_sim.gather( g_cl.m_local, g_cl.m_weapon, angle, setup, g_cl.m_cmd->command_number, m_weapon_data.m_charge_time,
                              ctx ? &at_fire : nullptr );
}

ragebot::contact_t ragebot::closest_approach(
    const vec3               &angle,
    const vec3               &point,
    const move_sim_ctx_t     *ctx,
    i32                       catch_up,
    bool                      splash,
    const launch_t           &launch,
    const projectile_setup_t *gathered )
{
    contact_t contact{ };
    if ( m_weapon_data.m_is_sticky && !m_sticky_work.available( ) )
    {
        m_sticky_deferred = true;
        return contact;
    }

    projectile_setup_t setup{ };
    const auto         gather{ [ & ]( )
    {
        if ( gathered )
        {
            setup = *gathered;
            return true;
        }

        return this->gather_launch( angle, ctx, catch_up, setup );
    } };

    const i32  budget{ ctx->m_track_count - 1 - catch_up };
    const i32  interval{ math::clamp( static_cast< i32 >( math::sqrt( static_cast< f32 >( budget ) ) ), 1, 12 ) };
    const bool physics{ m_launch_setup.m_movetype == projectile_movetype::vphysics };

    if ( splash && !physics && m_launch_setup.m_gravity == 0.0f )
    {
        if ( !gather( ) )
        {
            return contact;
        }

        vec3 direction{ };
        math::angle_vectors( setup.m_angles, &direction );
        CTraceFilterProjectile filter{ };
        filter.m_pSkip         = g_cl.m_local;
        filter.m_pSkipTarget   = ctx->m_player_to_predict;
        filter.m_players_block = true;
        filter.m_skip_team     = g_cl.m_local->m_iTeamNum( );
        Ray_t   ray{ };
        trace_t trace{ };

        const f32 distance{ math::smaller( setup.m_position.dist_to( point ) + 32.0f,
                                           setup.m_speed * math::smaller( TICKS_TO_TIME( budget ), setup.m_lifetime ) ) };
        const f32 first{ math::smaller( distance, setup.m_speed * 0.25f ) };
        ray.Init( setup.m_position, setup.m_position + direction * first, setup.m_hull_centre - setup.m_hull, setup.m_hull_centre + setup.m_hull );
        g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );
        if ( trace.fraction == 1.0f && !trace.startsolid && first < distance )
        {
            filter.m_skip_team = 0;
            ray.Init( setup.m_position + direction * first, setup.m_position + direction * distance,
                      setup.m_hull_centre - setup.m_hull, setup.m_hull_centre + setup.m_hull );
            g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );
            trace.fraction = ( first + ( distance - first ) * trace.fraction ) / distance;
        } else
        {
            trace.fraction *= first / distance;
        }
        contact.m_position = trace.endpos;
        contact.m_target   = point;
        contact.m_distance = trace.endpos.dist_to( point );
        contact.m_time     = trace.fraction < 1.0f && !trace.startsolid && !trace.allsolid ? distance * trace.fraction / setup.m_speed : -1.0f;
        contact.m_impact   = impact_t{ trace.endpos, trace.plane.normal, contact.m_time, contact.m_time >= 0.0f, trace, false };
        return contact;
    }

    flight_cache_t *flight{ };
    for ( i32 at{ }; at < m_flight_count; ++at )
    {
        if ( m_flights[ at ].m_angle == angle && m_flights[ at ].m_splash == splash )
        {
            flight = &m_flights[ at ];
            break;
        }
    }
    if ( !flight )
    {
        if ( !gather( ) )
        {
            return contact;
        }
        if ( m_weapon_data.m_is_sticky && m_sticky_search && m_sticky_work.m_flights - m_candidate_first_flight >= 4 )
        {
            m_sticky_deferred = true;
            return contact;
        }
        if ( splash && physics && m_splash_simulations[ launch.m_high_arc ] <= 0 )
        {
            m_sticky_deferred |= m_sticky_search != nullptr;
            return contact;
        }
        if ( splash && physics )
        {
            --m_splash_simulations[ launch.m_high_arc ];
        }
        flight           = &m_flights[ m_flight_next ];
        m_flight_next    = ( m_flight_next + 1 ) % FLIGHT_CACHE_SIZE;
        m_flight_count   = math::smaller( m_flight_count + 1, FLIGHT_CACHE_SIZE );
        flight->m_angle  = angle;
        flight->m_splash = splash;
        flight->m_start  = setup.m_position;
        flight->m_count  = g_proj_sim.simulate( setup, flight->m_path, budget, interval, flight->m_impact,
                                                splash ? ctx->m_player_to_predict : nullptr,
                                                m_weapon_data.m_is_sticky ? &m_sticky_work : nullptr );
        if ( flight->m_impact.m_deferred )
        {
            m_sticky_deferred = true;
            return contact;
        }
        if ( splash && m_weapon_data.m_is_sticky )
        {
            flight->m_count = splash::clip_sticky_player( setup.m_position, flight->m_path, flight->m_count,
                                                          ctx->m_track + catch_up, ctx->m_pred_mins, ctx->m_pred_maxs, flight->m_impact );
        }
    }
    const vec3     *path{ flight->m_path };
    const impact_t &impact{ flight->m_impact };
    const i32       ticks{ flight->m_count };
    if ( impact.m_deferred )
    {
        m_sticky_deferred = true;
        return contact;
    }
    if ( ticks <= 0 )
    {
        return contact;
    }

    if ( splash && launch.m_air )
    {
        contact.m_impact = impact;
        if ( impact.m_attached )
        {
            const i32 arrival{ this->arrival_tick( impact.m_time ) };
            if ( catch_up + arrival < ctx->m_track_count )
            {
                contact.m_position = impact.m_position;
                contact.m_target   = ctx->m_track[ catch_up + arrival ] + point;
                contact.m_time     = impact.m_time;
                contact.m_distance = contact.m_position.dist_to( contact.m_target );
            }
        }
        const i32 end{ ticks - ( impact.m_hit ? 1 : 0 ) };
        const i32 first{ this->arrival_tick( m_weapon_data.m_arm_time ) - 1 };
        i32       first_open{ end }, last_open{ -1 };
        for ( i32 at{ first }; at < end; ++at )
        {
            const vec3 target{ ctx->m_track[ catch_up + at + 1 ] + point };
            const f32  distance{ path[ at ].dist_to( target ) };
            if ( distance < contact.m_distance )
            {
                contact.m_position = path[ at ];
                contact.m_target   = target;
                contact.m_time     = TICKS_TO_TIME( at + 1 );
                contact.m_distance = distance;
            }
            const vec3 origin{ ctx->m_track[ catch_up + at + 1 ] };
            const f32  radius{ splash::air_radius( m_weapon_data.m_blast_radius, TICKS_TO_TIME( at + 1 ) ) };
            if ( splash::grenade_near_target( path[ at ], radius,
                                              splash_target_t{ nullptr, origin, origin + ctx->m_pred_mins, origin + ctx->m_pred_maxs } ) )
            {
                first_open = math::smaller( first_open, at );
                last_open  = at;
            }
        }

        splash::select_air_times( path, ctx->m_track + catch_up + 1, first_open, last_open,
                                  ctx->m_pred_mins, ctx->m_pred_maxs, m_weapon_data.m_blast_radius, m_weapon_data.m_blast_falloff, contact.m_air_samples );
        return contact;
    }

    if ( splash )
    {
        contact.m_position = path[ ticks - 1 ];
        contact.m_target   = point;
        contact.m_time     = impact.m_hit ? impact.m_time : -1.0f;
        contact.m_distance = contact.m_position.dist_to( point );
        contact.m_impact   = impact;
        return contact;
    }

    const vec3 hull{ physics ? vec3( 2.0f, 2.0f, 2.0f ) : m_launch_setup.m_hull };
    const vec3 centre{ physics ? vec3{ } : m_launch_setup.m_hull_centre };
    const vec3 low{ ctx->m_pred_mins - centre - hull };
    const vec3 high{ ctx->m_pred_maxs - centre + hull };
    for ( i32 at{ }; at < ticks; ++at )
    {
        // path[at] ends tick at+1; a wall hit may end that interval early.
        const f32  end{ impact.m_hit && at == ticks - 1 ? math::clamp( impact.m_time / TICK_INTERVAL - at, 0.0f, 1.0f ) : 1.0f };
        const vec3 from{ at ? path[ at - 1 ] : flight->m_start };
        const vec3 origin{ ctx->m_track[ catch_up + at ] };
        const vec3 move{ ( ctx->m_track[ catch_up + at + 1 ] - origin ) * end };
        const vec3 relative_from{ from - origin };
        const vec3 relative_to{ path[ at ] - origin - move };
        const f32  fraction{ math::segment_box_fraction( relative_from, relative_to, low, high ) };
        const bool hit{ fraction != FLT_MAX };
        const vec3 along{ relative_to - relative_from };
        const f32  length{ along.length_sqr( ) };
        const f32  nearest{ hit ? fraction : length > 0.0f ? math::clamp( ( point - relative_from ).dot( along ) / length, 0.0f, 1.0f )
                                                           : 0.0f };
        const vec3 relative{ relative_from + along * nearest };
        const vec3 on_box{ math::nearest_on_box( low, high, relative ) };
        const f32  miss{ hit ? 0.0f : relative.dist_to( on_box ) };
        if ( miss < contact.m_distance )
        {
            contact.m_distance = miss;
            contact.m_position = from + ( path[ at ] - from ) * nearest;
            contact.m_target   = origin + move * nearest + on_box;
            contact.m_time     = ( at + end * nearest ) * TICK_INTERVAL;
            contact.m_impact   = impact;
        }
        if ( hit )
            return contact;
    }

    return contact;
}

bool ragebot::flight_time( const vec3 &delta, bool high_arc, f32 &time ) const
{
    if ( m_launch_setup.m_movetype != projectile_movetype::flame )
    {
        return ballistics::flight_time( delta, m_weapon_data.m_launch_speed, m_weapon_data.m_gravity, high_arc, time );
    }

    if ( high_arc )
    {
        return false;
    }

    const projectile_setup_t &flame{ m_launch_setup };
    const f32                 keep{ math::bigger( 1.0f - flame.m_drag * TICK_INTERVAL, 0.0f ) };
    const i32                 ticks{ TIME_TO_TICKS( flame.m_lifetime ) };

    f32 speed{ flame.m_speed };
    f32 reach{ };
    f32 behind{ -delta.length( ) };

    for ( i32 tick{ 1 }; tick <= ticks; ++tick )
    {
        const f32  elapsed{ TICKS_TO_TIME( tick ) };
        const vec3 aim{ delta - vec3( 0.0f, 0.0f, flame.m_rise * elapsed ) };
        const f32  distance{ aim.length( ) };

        reach += speed * TICK_INTERVAL;
        speed *= keep;

        const f32 carried{ distance > 0.0f ? flame.m_carry.dot( aim ) / distance * elapsed : 0.0f };
        const f32 ahead{ reach + carried - distance };
        if ( ahead >= 0.0f )
        {
            time = elapsed - TICK_INTERVAL * ahead / ( ahead - behind );
            return true;
        }

        behind = ahead;
    }

    return false;
}

bool ragebot::solve_launch( const vec3 &from, const vec3 &point, bool high_arc, launch_t &out ) const
{
    if ( m_launch_setup.m_movetype == projectile_movetype::flame )
    {
        if ( !this->flight_time( point - from, high_arc, out.m_time ) )
        {
            return false;
        }

        math::vector_angles( point - from - vec3( 0.0f, 0.0f, m_launch_setup.m_rise * out.m_time ), out.m_angle );
        out.m_high_arc = false;
        out.m_arrival  = this->arrival_tick( out.m_time );
        return true;
    }

    // Calc the initial angle to.
    vec3 ang_to{ };
    math::vector_angles( point - from, ang_to );

    // Fix up the fire origin so the time calculation is actually accurate to the set up projectile fire origin.
    vec3 resolved_origin{ };

    if ( m_weapon_data.m_is_vphysics )
    {
        if ( !g_cl.projectile_grenade_setup( g_cl.m_local, g_cl.m_weapon, ang_to, resolved_origin ) )
        {
            return false;
        }
    } else
    {
        resolved_origin = g_cl.projectile_fire_origin( g_cl.m_local, g_cl.m_weapon, ang_to,
                                                       g_proj_sim.get_fire_offset( g_cl.m_weapon, g_cl.m_local->IsDucked( ) ) );
    }

    const vec3 delta{ point - resolved_origin };
    if ( !this->flight_time( delta, high_arc, out.m_time ) )
    {
        return false;
    }

    const f32 tilt{ RAD2DEG( math::atan2( m_weapon_data.m_up_kick, m_weapon_data.m_projectile_speed ) ) };
    const f32 height{ delta.z + 0.5f * m_weapon_data.m_gravity * out.m_time * out.m_time };
    out.m_angle    = vec3( -RAD2DEG( math::atan2( height, delta.length2d( ) ) ) + tilt, RAD2DEG( math::atan2( delta.y, delta.x ) ), 0.0f );
    out.m_high_arc = high_arc;
    out.m_arrival  = this->arrival_tick( out.m_time );
    return true;
}

i32 ragebot::arrival_tick( f32 flight_time ) const
{
    flight_time = math::bigger( flight_time, m_weapon_data.m_arm_time );
    return math::bigger( static_cast< i32 >( flight_time / TICK_INTERVAL + 0.999f ), 1 );
}

bool ragebot::refine_launch( const vec3 &point, const move_sim_ctx_t *ctx, i32 catch_up, bool splash, f32 roll,
                             launch_t &launch, launch_t *blocked )
{
    launch.m_kind = splash ? launch_t::kind::splash : launch_t::kind::direct;
    const bool  air{ splash && m_weapon_data.m_is_sticky && launch.m_air };
    const bool  straight{ splash && this->straight_rocket( ) };
    const vec3 *held{ g_holdaim_mgr.held( ) };
    vec3        angle{ held ? *held : launch.m_angle };
    if ( !held )
    {
        angle.z = roll;
        if ( straight )
        {
            this->aim_rocket( point, ctx, catch_up, roll, angle );
        }
    }

    projectile_setup_t setup{ };
    if ( !this->gather_launch( angle, ctx, catch_up, setup ) )
    {
        return false;
    }

    const projectile_setup_t *gathered{ &setup };
    if ( !held && !m_resume_seed && !m_weapon_data.m_is_sticky && m_weapon_data.m_gravity_mod > 0.0f )
    {
        const i32  seed{ math::clamp( catch_up + launch.m_arrival, catch_up + 1, ctx->m_track_count - 1 ) };
        const vec3 target{ splash && !launch.m_air ? point : ctx->m_track[ seed ] + point };
        launch_t   from_muzzle{ };
        if ( !this->solve_launch( setup.m_position, target, launch.m_high_arc, from_muzzle ) )
        {
            return false;
        }

        angle    = from_muzzle.m_angle;
        angle.z  = roll;
        gathered = nullptr;
    }

    const f32  tolerance{ splash ? ( launch.m_air ? m_weapon_data.m_blast_radius + ( ctx->m_pred_maxs - ctx->m_pred_mins ).length( ) * 0.5f : setup.m_hull.z + 16.0f ) : 0.0f };
    const auto accept_air{ [ & ]( const contact_t &contact, const vec3 &angle )
    {
        if ( !air || m_sticky_deferred )
        {
            return false;
        }

        launch_t flight{ launch }, selected{ };
        flight.m_aim_point   = point;
        flight.m_angle       = angle;
        flight.m_impact      = contact.m_impact;
        flight.m_air_samples = contact.m_air_samples;
        if ( !this->select_sticky_air( ctx, g_cl.m_local->Weapon_ShootPosition( ), catch_up, flight, selected ) )
        {
            return false;
        }
        launch = selected;
        return true;
    } };
    contact_t contact{ this->closest_approach( angle, point, ctx, catch_up, splash, launch, gathered ) };
    if ( accept_air( contact, angle ) )
    {
        return true;
    }
    if ( m_sticky_deferred )
    {
        if ( m_sticky_search )
        {
            m_sticky_search->m_angle      = angle;
            m_sticky_search->m_seed       = true;
            m_sticky_search->m_seed_speed = m_launch_setup.m_speed;
        }
        return false;
    }
    if ( contact.m_distance == FLT_MAX )
    {
        return false;
    }

    const i32  completed{ m_resume_seed && m_sticky_search ? m_sticky_search->m_refine_passes : 0 };
    i32        iterations{ completed };
    const f32  convergence{ splash ? ( m_weapon_data.m_is_sticky ? ( launch.m_air ? m_weapon_data.m_blast_radius * 0.25f : 2.0f ) : 0.03125f ) : tolerance };
    const bool on_line{ straight && line_miss( setup, point ) <= convergence };
    const i32  passes{ held || on_line ? 0 : math::bigger( ( m_weapon_data.m_is_vphysics ? ( splash ? splash::quality( ).m_aim_passes : 5 ) : 2 ) - completed, 0 ) };
    for ( i32 pass{ 0 }; pass < passes && ( contact.m_distance > convergence || contact.m_time < 0.0f ); ++pass )
    {
        vec3 stepped{ angle };
        if ( m_weapon_data.m_is_sticky )
        {
            constexpr f32 STEP{ 0.25f };
            vec3          pitch{ angle }, yaw{ angle };
            pitch.x += STEP;
            yaw.y += STEP;
            const contact_t pitched{ this->closest_approach( pitch, point, ctx, catch_up, splash, launch ) };
            if ( accept_air( pitched, pitch ) )
            {
                return true;
            }
            if ( pitched.m_distance == FLT_MAX || m_sticky_deferred )
            {
                break;
            }
            const contact_t yawed{ this->closest_approach( yaw, point, ctx, catch_up, splash, launch ) };
            if ( accept_air( yawed, yaw ) )
            {
                return true;
            }
            if ( yawed.m_distance == FLT_MAX || m_sticky_deferred )
            {
                break;
            }
            const vec3 error{ contact.m_position - contact.m_target };
            const vec3 dx{ ( pitched.m_position - pitched.m_target - error ) / STEP };
            const vec3 dy{ ( yawed.m_position - yawed.m_target - error ) / STEP };
            stepped = ballistics::refine_step( angle, error, dx, dy );
        } else
        {
            vec3 wanted{ }, reached{ };
            math::vector_angles( contact.m_target - setup.m_position, wanted );
            math::vector_angles( contact.m_position - setup.m_position, reached );
            stepped.x += launch.m_high_arc ? reached.x - wanted.x : wanted.x - reached.x;
            stepped.y -= math::normalize_yaw( reached.y - wanted.y );
        }

        contact_t candidate{ this->closest_approach( stepped, point, ctx, catch_up, splash, launch ) };
        if ( accept_air( candidate, stepped ) )
        {
            return true;
        }
        if ( m_sticky_deferred )
        {
            if ( m_sticky_search )
            {
                if ( !air || m_sticky_search->m_air_tick == 0 )
                {
                    m_sticky_search->m_angle = angle;
                }
                m_sticky_search->m_seed          = true;
                m_sticky_search->m_seed_speed    = m_launch_setup.m_speed;
                m_sticky_search->m_refine_passes = iterations;
            }
            return false;
        }
        ++iterations;
        if ( candidate.m_distance >= contact.m_distance )
        {
            break;
        }

        angle   = stepped;
        contact = candidate;
    }

    if ( m_sticky_deferred )
    {
        if ( m_sticky_search )
        {
            m_sticky_search->m_angle         = angle;
            m_sticky_search->m_seed          = true;
            m_sticky_search->m_seed_speed    = m_launch_setup.m_speed;
            m_sticky_search->m_refine_passes = iterations;
        }
        return false;
    }

    const i32 arrival{ this->arrival_tick( contact.m_time ) };
    if ( blocked && splash && !air && !m_weapon_data.m_is_sticky && contact.m_time >= 0.0f &&
         contact.m_distance > tolerance && catch_up + arrival < ctx->m_track_count &&
         contact.m_impact.m_hit && !contact.m_impact.m_trace.startsolid && !contact.m_impact.m_trace.allsolid &&
         !( contact.m_impact.m_trace.surface.flags & SURF_SKY ) )
    {
        const vec3            origin{ ctx->m_track[ catch_up + arrival ] };
        const splash_target_t target{ ctx->m_player_to_predict, origin, origin + ctx->m_pred_mins,
                                      origin + ctx->m_pred_maxs, vec3{ } };
        const vec3            explosion{ contact.m_position + contact.m_impact.m_normal };
        const f32             potential{ splash::falloff( explosion, m_weapon_data.m_blast_radius,
                                                          m_weapon_data.m_blast_falloff, target ) };
        if ( potential > 0.0f )
        {
            blocked->m_kind         = launch_t::kind::splash;
            blocked->m_angle        = angle;
            blocked->m_position     = contact.m_position;
            blocked->m_aim_point    = point;
            blocked->m_time         = contact.m_time;
            blocked->m_arrival      = arrival;
            blocked->m_high_arc     = launch.m_high_arc;
            blocked->m_impact       = contact.m_impact;
            blocked->m_splash_score = potential;
        }
    }
    if ( air || contact.m_time < 0.0f ||
         contact.m_distance > tolerance ||
         catch_up + arrival >= ctx->m_track_count )
    {
        return false;
    }

    launch.m_aim_point   = point;
    launch.m_angle       = angle;
    launch.m_position    = contact.m_position;
    launch.m_time        = contact.m_time;
    launch.m_arrival     = arrival;
    launch.m_impact      = contact.m_impact;
    launch.m_air_samples = contact.m_air_samples;
    return true;
}

bool ragebot::begin_sticky_candidate( )
{
    if ( !m_sticky_search )
    {
        return true;
    }

    const i32 candidate{ m_candidate_index++ };
    if ( candidate < m_sticky_search->m_cursor )
    {
        return false;
    }
    if ( m_sticky_deferred || !m_sticky_work.available( ) )
    {
        m_sticky_deferred = true;
        return false;
    }
    m_candidate_first_flight = m_sticky_work.m_flights;
    return true;
}

bool ragebot::verify_launch( const vec3 &point, const move_sim_ctx_t *ctx, i32 catch_up, bool splash, launch_t &launch )
{
    const i32 candidate{ m_sticky_search ? m_candidate_index - 1 : 0 };
    launch_t  blocked_best{ };
    bool      blocked_found{ };
    if ( splash && m_weapon_data.m_is_vphysics && m_splash_simulations[ launch.m_high_arc ] <= 0 )
    {
        m_sticky_deferred |= m_sticky_search != nullptr;
        return false;
    }
    i32 first{ };
    m_resume_seed = false;
    if ( m_sticky_search )
    {
        if ( candidate == m_sticky_search->m_cursor && launch.m_high_arc == m_sticky_search->m_high_arc &&
             point.dist_to_sqr( m_sticky_search->m_point ) <= 64.0f )
        {
            first = m_sticky_search->m_roll;
            if ( m_sticky_search->m_seed && math::abs( m_sticky_search->m_seed_speed - m_launch_setup.m_speed ) <= m_launch_setup.m_speed * 0.02f )
            {
                launch.m_angle = m_sticky_search->m_angle;
                m_resume_seed  = true;
            }
        }

        m_sticky_search->m_cursor   = candidate;
        m_sticky_search->m_point    = point;
        m_sticky_search->m_high_arc = launch.m_high_arc;
        if ( !m_resume_seed )
        {
            m_sticky_search->m_air_tick      = 0;
            m_sticky_search->m_refine_passes = 0;
        }
    }
    for ( i32 roll{ first }; roll < 4; ++roll )
    {
        if ( roll == math::bigger( first, 1 ) )
        {
            const i32  seed{ math::clamp( catch_up + launch.m_arrival, catch_up + 1, ctx->m_track_count - 1 ) };
            const vec3 target{ splash && !launch.m_air ? point : ctx->m_track[ seed ] + point };
            if ( !this->rolls_allowed( target ) )
            {
                break;
            }
        }

        if ( math::abs( LAUNCH_ROLLS[ roll ] ) == 90.0f && !this->quarter_roll_allowed( ) )
        {
            continue;
        }

        if ( m_sticky_search )
        {
            m_sticky_search->m_roll = roll;
            if ( !m_resume_seed )
            {
                m_sticky_search->m_air_tick      = 0;
                m_sticky_search->m_refine_passes = 0;
            }
        }

        launch_t blocked{ };
        if ( this->refine_launch( point, ctx, catch_up, splash, LAUNCH_ROLLS[ roll ], launch,
                                  splash && !m_weapon_data.m_is_sticky ? &blocked : nullptr ) )
        {
            return true;
        }
        if ( blocked.m_splash_score > 0.0f &&
             ( !blocked_found || blocked.m_splash_score > blocked_best.m_splash_score ||
               ( blocked.m_splash_score == blocked_best.m_splash_score &&
                 blocked.m_arrival < blocked_best.m_arrival ) ) )
        {
            blocked_best  = blocked;
            blocked_found = true;
        }

        if ( m_sticky_deferred )
        {
            return false;
        }

        m_resume_seed = false;
    }

    if ( m_sticky_search )
    {
        m_sticky_search->m_cursor   = candidate + 1;
        m_sticky_search->m_roll     = 0;
        m_sticky_search->m_seed     = false;
        m_sticky_search->m_air_tick = 0;
    }

    if ( blocked_found )
    {
        launch = blocked_best;
        return true;
    }

    return false;
}

void ragebot::take_aimpoint( const move_sim_ctx_t *ctx, i32 catch_up, const launch_t &launch, ragebot_target_t &out )
{
    out.m_aim_origin   = launch.m_position;
    out.m_aim_angle    = launch.m_angle;
    out.m_got_hitpoint = true;
    out.m_arrival      = launch.m_arrival;
    if ( m_weapon_data.m_is_sticky && m_sticky_probe )
    {
        m_candidate_launch = launch;
        return;
    }
    if ( m_weapon_data.m_is_sticky && !this->m_reload_probe )
    {
        m_selected_launch            = launch;
        m_selected_sticky_target     = ctx->m_player_to_predict->GetRefEHandle( ).ToInt( );
        m_selected_sticky_weapon     = g_cl.m_weapon->GetRefEHandle( ).ToInt( );
        m_selected_sticky_generation = g_splash.generation( );

        if ( m_sticky_search )
        {
            m_selected_sticky_settings = m_sticky_search->m_settings;
        }

        m_pending_sticky = false;
    }

    if ( !this->m_reload_probe )
    {
        this->m_projectile_solved = true;
        this->keep_path( ctx, catch_up, catch_up + launch.m_arrival );
        if ( g_cfg.m_ragebot.m_projectile_draw_path && launch.m_kind == launch_t::kind::direct )
        {
            this->m_path.m_bounds_origin                      = ballistics::target_at( ctx->m_track, ctx->m_track_count, catch_up + launch.m_time / TICK_INTERVAL );
            this->m_path.m_points[ this->m_path.m_count - 1 ] = this->m_path.m_bounds_origin;
        }
    }
}

void ragebot::keep_path( const move_sim_ctx_t *ctx, i32 first_tick, i32 through_tick )
{
    if ( !g_cfg.m_ragebot.m_projectile_draw_path )
    {
        return;
    }

    this->m_path.m_count                  = through_tick - first_tick + 1;
    this->m_path.m_ground_agreement_ticks = ctx->m_ground_agreement_ticks < 0 ? -1 : math::bigger( ctx->m_ground_agreement_ticks - first_tick, 0 );
    atl::memcpy( this->m_path.m_points, ctx->m_track + first_tick, sizeof( vec3 ) * static_cast< usize >( this->m_path.m_count ) );

    this->m_path.m_bounds_origin = ctx->m_track[ through_tick ];
    this->m_path.m_bounds_mins   = ctx->m_pred_mins;
    this->m_path.m_bounds_maxs   = ctx->m_pred_maxs;
    this->m_path_time            = g_tf2.m_engine->Plat_FloatTime( );
}

i32 ragebot::sticky_intercepts( const vec3 &shoot_pos, const vec3 &offset, const move_sim_ctx_t *ctx, i32 catch_up, ballistics::intercept_t *roots ) const
{
    const i32 count{ ballistics::sticky_intercepts( shoot_pos, ctx->m_track + catch_up, ctx->m_track_count - catch_up, offset,
                                                    TICK_INTERVAL, m_launch_setup.m_speed, m_launch_setup.m_up_kick + m_launch_setup.m_jitter_up,
                                                    m_launch_setup.m_jitter_right, g_cl.m_vm_flipped ? -8.0f : 8.0f,
                                                    m_weapon_data.m_gravity, roots, 8 ) };
    return count;
}

bool ragebot::solve_intercept( const vec3 &shoot_pos, const vec3 &offset, const move_sim_ctx_t *ctx, i32 catch_up, bool high_arc, launch_t &out, bool roots_checked ) const
{
    if ( m_weapon_data.m_is_sticky && !roots_checked )
    {
        ballistics::intercept_t roots[ 8 ]{ };
        const i32               count{ this->sticky_intercepts( shoot_pos, offset, ctx, catch_up, roots ) };
        if ( count > ( high_arc ? 1 : 0 ) )
        {
            const auto &root{ roots[ high_arc ? count - 1 : 0 ] };
            out.m_angle    = root.m_angle;
            out.m_time     = root.m_time;
            out.m_arrival  = ballistics::arrival_tick( root.m_time, TICK_INTERVAL );
            out.m_high_arc = high_arc;
            return true;
        }
    }
    f32  closest{ FLT_MAX };
    f32  flight{ };
    vec3 previous{ };
    bool reachable{ };
    i32  best{ -1 };

    for ( i32 at{ catch_up + 1 }; at < ctx->m_track_count; ++at )
    {
        const vec3 point{ ctx->m_track[ at ] + offset };
        if ( at == catch_up + 1 || point != previous )
        {
            reachable = this->flight_time( point - this->muzzle_towards( shoot_pos, point ), high_arc, flight );
            previous  = point;
        }

        if ( !reachable )
        {
            continue;
        }

        const f32 miss{ math::abs( flight - TICKS_TO_TIME( at - catch_up ) ) };
        if ( miss < closest )
        {
            closest = miss;
            best    = at;
        }
    }

    if ( best < 0 )
    {
        return false;
    }

    const vec3 point{ ctx->m_track[ best ] + offset };
    if ( !this->solve_launch( this->muzzle_towards( shoot_pos, point ), point, high_arc, out ) )
    {
        return false;
    }
    out.m_arrival = best - catch_up;
    return true;
}

bool ragebot::solve_box( const move_sim_ctx_t *ctx, const vec3 &shoot_pos, i32 catch_up, bool high_arc, ragebot_target_t &out )
{
    i32       order[ 3 ]{ };
    const i32 count{ this->aim_order( ctx, order ) };

    for ( i32 at{ 0 }; at < count; ++at )
    {
        vec3 offset{ bbox_point( ctx, order[ at ] ) - ctx->m_pred_origin };

        const bool headshot{ order[ at ] == bbox_pos_top && g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_COMPOUND_BOW };
        if ( headshot )
        {
            if ( !this->arrow_safe_point( ctx, shoot_pos, catch_up, high_arc, offset ) )
            {
                continue;
            }
        }

        launch_t launch{ };
        if ( !this->solve_intercept( shoot_pos, offset, ctx, catch_up, high_arc, launch ) )
        {
            continue;
        }

        if ( !this->verify_launch( offset, ctx, catch_up, false, launch ) )
        {
            continue;
        }

        if ( headshot )
        {
            if ( !this->steer_arrow( offset, ctx, catch_up, launch ) )
            {
                continue;
            }
        }
        if ( !this->within_fov( shoot_pos, launch.m_position ) )
        {
            continue;
        }
        if ( this->m_projectile_profile.m_projectile_hitchance > 0 &&
             this->projectile_hitchance( ctx, catch_up + launch.m_arrival ) < this->required_hitchance( launch.m_arrival ) )
        {
            continue;
        }

        out.m_bbox_point = at;
        this->take_aimpoint( ctx, catch_up, launch, out );
        return true;
    }

    return false;
}

bool ragebot::capture_arrow_pose( C_TFPlayer *player )
{
    const auto model{ player->GetModel( ) };
    if ( !model )
    {
        return false;
    }

    const auto hdr{ g_tf2.m_model_info->GetStudiomodel( model ) };
    if ( !hdr )
    {
        return false;
    }

    const auto set{ hdr->pHitboxSet( player->m_nHitboxSet( ) ) };
    if ( !set || set->numhitboxes <= 0 )
    {
        return false;
    }

    const lag_record_t *record{ g_lagcomp.record( player->entindex( ), 0 ) };
    if ( !record )
    {
        return false;
    }

    const matrix3x4 *bones{ record->m_bones };
    if ( record->m_bone_count <= 0 )
    {
        if ( !g_lagcomp.build_bones( player, record, BONE_USED_BY_HITBOX, m_arrow_bones, MAX_STUDIO_BONES ) )
        {
            return false;
        }

        bones = m_arrow_bones;
    }

    m_arrow_pose = scan_pose_t{ bones, record->m_pose.m_origin, record->m_tick, record->m_pose.m_scale };
    m_arrow_set  = set;
    return true;
}

i32 ragebot::arrow_hitbox( const vec3 &entry, const vec3 &direction, f32 speed, const vec3 &forward, const vec3 &shift ) const
{
    const vec3 start{ entry - shift };
    const vec3 end{ start + direction * ( speed * TICK_INTERVAL ) };

    i32 struck{ -1 };
    f32 nearest{ FLT_MAX };
    for ( i32 index{ }; index < m_arrow_set->numhitboxes; ++index )
    {
        const mstudiobbox_t *box{ m_arrow_set->pHitbox( index ) };

        f32 fraction{ };
        if ( math::ray_hits_box( start, end, m_arrow_pose.m_bones[ box->bone ], box->bbmin, box->bbmax, 0.0f, &fraction ) &&
             fraction <= 1.0f && fraction <= nearest )
        {
            nearest = fraction;
            struck  = index;
        }
    }

    if ( struck >= 0 )
    {
        return struck;
    }

    const vec3 probe{ start + forward * 16.0f };

    f32 closest{ 99999.0f };
    for ( i32 index{ }; index < m_arrow_set->numhitboxes; ++index )
    {
        const mstudiobbox_t *box{ m_arrow_set->pHitbox( index ) };
        const matrix3x4     &bone{ m_arrow_pose.m_bones[ box->bone ] };
        const vec3           position{ bone[ 0 ][ 3 ], bone[ 1 ][ 3 ], bone[ 2 ][ 3 ] };
        const f32            fraction{ math::segment_box_fraction( probe, position, position + box->bbmin, position + box->bbmax ) };
        const f32            distance{ probe.dist_to( position ) * ( fraction == FLT_MAX ? 1.0f : fraction ) };
        if ( distance < closest )
        {
            closest = distance;
            struck  = index;
        }
    }

    return struck;
}

void ragebot::build_arrow_grid( const move_sim_ctx_t *ctx, const vec3 &origin, const vec3 &direction, f32 speed, const vec3 &forward,
                                arrow_grid_t &grid ) const
{
    vec3 angles{ };
    math::vector_angles( direction, angles );
    math::angle_vectors( angles, nullptr, &grid.m_right, &grid.m_up );

    const vec3           shift{ origin - m_arrow_pose.m_origin };
    const mstudiobbox_t *head{ m_arrow_set->pHitbox( HITBOX_HEAD ) };
    const matrix3x4     &bone{ m_arrow_pose.m_bones[ head->bone ] };

    grid.m_origin = origin;
    grid.m_low    = origin + ctx->m_pred_mins - m_launch_setup.m_hull;
    grid.m_high   = origin + ctx->m_pred_maxs + m_launch_setup.m_hull;
    grid.m_centre = math::transform_point( ( head->bbmin + head->bbmax ) * 0.5f, bone ) + shift;
    grid.m_reach  = direction * ( grid.m_high - grid.m_low ).length( );

    f32 extent{ };
    for ( i32 corner{ }; corner < 8; ++corner )
    {
        const vec3 spoke{ math::transform_point( math::box_corner( head->bbmin, head->bbmax, corner ), bone ) + shift - grid.m_centre };
        extent = math::bigger( extent, math::bigger( math::abs( spoke.dot( grid.m_right ) ), math::abs( spoke.dot( grid.m_up ) ) ) );
    }

    grid.m_cell = extent * 4.0f / static_cast< f32 >( ARROW_GRID_SIZE );

    for ( i32 cell{ }; cell < ARROW_GRID_SIZE * ARROW_GRID_SIZE; ++cell )
    {
        vec3 entry{ };
        if ( !arrow_cell_entry( grid, cell, entry ) )
        {
            grid.m_cells[ cell ] = arrow_cell_miss;
            continue;
        }

        const i32 hitbox{ this->arrow_hitbox( entry, direction, speed, forward, shift ) };
        grid.m_cells[ cell ] = hitbox >= 0 && m_arrow_set->pHitbox( hitbox )->group == HITGROUP_HEAD ? arrow_cell_head : arrow_cell_body;
    }
}

bool ragebot::arrow_safe_point( const move_sim_ctx_t *ctx, const vec3 &shoot_pos, i32 catch_up, bool high_arc, vec3 &offset )
{
    if ( !this->capture_arrow_pose( ctx->m_player_to_predict ) )
    {
        return false;
    }

    const mstudiobbox_t *head{ m_arrow_set->pHitbox( HITBOX_HEAD ) };
    vec3                 head_centre{ };
    math::vector_transform( ( head->bbmin + head->bbmax ) * 0.5f, m_arrow_pose.m_bones[ head->bone ], head_centre );

    launch_t seed{ };
    if ( !this->solve_intercept( shoot_pos, head_centre - m_arrow_pose.m_origin, ctx, catch_up, high_arc, seed ) )
    {
        return false;
    }

    if ( m_weapon_data.m_has_life_time && seed.m_time > m_weapon_data.m_projectile_life_time )
    {
        return false;
    }

    vec3 forward{ };
    math::angle_vectors( seed.m_angle, &forward );

    const vec3 velocity{ forward * m_weapon_data.m_launch_speed - vec3( 0.0f, 0.0f, m_weapon_data.m_gravity * seed.m_time ) };
    const f32  speed{ velocity.length( ) };
    const vec3 origin{ ctx->m_track[ catch_up + seed.m_arrival ] };

    this->build_arrow_grid( ctx, origin, velocity / speed, speed, forward, m_arrow_grid );

    vec3      entry{ };
    const i32 best{ deepest_head_cell( m_arrow_grid ) };
    if ( best < 0 || !arrow_cell_entry( m_arrow_grid, best, entry ) )
    {
        return false;
    }

    offset = entry - origin;
    return true;
}

bool ragebot::steer_arrow( const vec3 &offset, const move_sim_ctx_t *ctx, i32 catch_up, launch_t &launch )
{
    for ( i32 pass{ }; pass < 2 && !this->arrow_holds_safe_point( ctx, catch_up, launch ); ++pass )
    {
        projectile_setup_t setup{ };
        if ( !this->gather_launch( launch.m_angle, ctx, catch_up, setup ) )
        {
            return false;
        }

        const vec3 target{ ballistics::target_at( ctx->m_track, ctx->m_track_count, catch_up + launch.m_time / TICK_INTERVAL ) + offset };
        vec3       wanted{ }, reached{ };
        math::vector_angles( target - setup.m_position, wanted );
        math::vector_angles( launch.m_position - setup.m_position, reached );

        vec3 angle{ launch.m_angle };
        angle.x += launch.m_high_arc ? reached.x - wanted.x : wanted.x - reached.x;
        angle.y -= math::normalize_yaw( reached.y - wanted.y );

        const contact_t contact{ this->closest_approach( angle, offset, ctx, catch_up, false, launch ) };
        const i32       arrival{ this->arrival_tick( contact.m_time ) };
        if ( contact.m_distance > 0.0f || contact.m_time < 0.0f || catch_up + arrival >= ctx->m_track_count )
        {
            return false;
        }

        launch.m_angle    = angle;
        launch.m_position = contact.m_position;
        launch.m_time     = contact.m_time;
        launch.m_arrival  = arrival;
        launch.m_impact   = contact.m_impact;
    }

    return this->arrow_holds_safe_point( ctx, catch_up, launch );
}

bool ragebot::arrow_holds_safe_point( const move_sim_ctx_t *ctx, i32 catch_up, const launch_t &launch ) const
{
    const arrow_grid_t &grid{ m_arrow_grid };
    const vec3          origin{ ballistics::target_at( ctx->m_track, ctx->m_track_count, catch_up + launch.m_time / TICK_INTERVAL ) };
    const vec3          spoke{ launch.m_position - origin + grid.m_origin - grid.m_centre };
    const f32           half{ static_cast< f32 >( ARROW_GRID_SIZE ) * 0.5f };
    const f32           across{ spoke.dot( grid.m_right ) / grid.m_cell + half };
    const f32           down{ half - spoke.dot( grid.m_up ) / grid.m_cell };
    if ( across < 0.0f || across >= ARROW_GRID_SIZE || down < 0.0f || down >= ARROW_GRID_SIZE )
    {
        return false;
    }

    const i32 cell{ static_cast< i32 >( down ) * ARROW_GRID_SIZE + static_cast< i32 >( across ) };
    return grid.m_depth[ cell ] == grid.m_deepest;
}

ATLAS_FORCE_INLINE splash_target_t predicted_blast_target( const move_sim_ctx_t *ctx, i32 tick )
{
    const vec3 origin{ ctx->m_track[ tick ] };
    return splash_target_t{ ctx->m_player_to_predict, origin, origin + ctx->m_pred_mins, origin + ctx->m_pred_maxs,
                            splash::player_eye( ctx->m_player_to_predict, origin, ctx->m_pred_flags ) };
}

ATLAS_FORCE_INLINE bool better_splash( const launch_t &candidate, const launch_t &best, const vec3 &view )
{
    if ( candidate.m_splash_score != best.m_splash_score )
    {
        return candidate.m_splash_score > best.m_splash_score;
    }

    if ( candidate.m_arrival != best.m_arrival )
    {
        return candidate.m_arrival < best.m_arrival;
    }

    return math::calc_fov( view, candidate.m_angle ) < math::calc_fov( view, best.m_angle );
}

bool ragebot::handle_splashbot( const move_sim_ctx_t *ctx, ragebot_target_t &tgt, const vec3 &shoot_pos, i32 catch_up, bool high_arc )
{
    if ( !this->m_projectile_profile.m_projectile_splashbot )
    {
        return false;
    }

    if ( m_weapon_data.m_blast_radius <= 0.0f ||
         ( m_weapon_data.m_is_vphysics && m_splash_simulations[ high_arc ] <= 0 ) )
    {
        return false;
    }

    if ( !m_splash_started )
    {
        g_splash.begin( );
        m_low_splash_points.m_count = 0;
        m_splash_started            = true;
    }

    const splash_budget_t &budget{ splash::quality( ) };
    const i32              discovery_command{ m_sticky_search ? m_sticky_search->m_discovery_command : g_cl.m_cmd->command_number };
    const vec3             offset{ bbox_point( ctx, bbox_pos_mid ) - ctx->m_pred_origin };
    const i32              arcs{ m_weapon_data.m_gravity_mod > 0.0f ? 2 : 1 };
    splash_points_t        points{ };
    points.m_limit              = budget.m_candidates - ( m_weapon_data.m_is_sticky ? budget.m_air_samples * arcs : 0 );
    points.m_seam_limit         = budget.m_candidates / 8;
    points.m_seams_from_shooter = !m_weapon_data.m_is_vphysics && m_weapon_data.m_gravity_mod == 0.0f &&
                                  m_weapon_data.m_hull_mins.is_zero( ) && m_weapon_data.m_hull_maxs.is_zero( );
    if ( points.m_seams_from_shooter )
    {
        points.m_shoot_pos  = shoot_pos;
        points.m_seam_limit = math::smaller( budget.m_candidates / 4, 16 );
    }
    if ( m_weapon_data.m_is_sticky )
    {
        points.m_sticky    = true;
        points.m_spacing   = math::bigger( ( m_weapon_data.m_hull_maxs - m_weapon_data.m_hull_mins ).length2d( ) * 0.5f, 1.0f );
        points.m_margin    = 32.0f + ( m_weapon_data.m_hull_maxs - m_weapon_data.m_hull_mins ).length( ) * 0.5f;
        points.m_air_limit = budget.m_air_samples;
    }
    const i32 arc{ high_arc ? 1 : 0 };
    launch_t  reach{ };
    i32       estimate_tick{ };
    {

        if ( this->solve_intercept( shoot_pos, offset, ctx, catch_up, high_arc, reach ) )
        {
            estimate_tick = math::bigger( reach.m_arrival, this->arrival_tick( m_weapon_data.m_arm_time ) );
        } else
        {
            const f32 horizon{ TICKS_TO_TIME( ctx->m_track_count - catch_up - 1 ) };
            const f32 distance{ ( ctx->m_track[ catch_up ] + offset ).dist_to( shoot_pos ) };
            const f32 margin{ m_weapon_data.m_blast_radius + ( ctx->m_pred_maxs - ctx->m_pred_mins ).length( ) +
                              math::abs( m_weapon_data.m_muzzle_forward ) };
            if ( m_weapon_data.m_launch_speed <= 0.0f ||
                 distance > m_weapon_data.m_launch_speed * horizon + 0.5f * m_weapon_data.m_gravity * horizon * horizon + margin )
            {
                return false;
            }

            estimate_tick = this->arrival_tick( math::bigger( distance - m_weapon_data.m_blast_radius, 0.0f ) / m_weapon_data.m_launch_speed );
        }

        const i32 last{ ctx->m_track_count - catch_up - 1 };
        if ( last < this->arrival_tick( m_weapon_data.m_arm_time ) )
        {
            return false;
        }
        estimate_tick = math::clamp( estimate_tick, 1, last );
    }
    const splash_target_t estimate{ predicted_blast_target( ctx, catch_up + estimate_tick ) };
    if ( high_arc && m_low_splash_points.m_count )
    {
        points = m_low_splash_points;
    }
    g_splash.collect( estimate, shoot_pos, m_weapon_data.m_blast_radius, arcs > 1, points,
                      m_weapon_data.m_is_sticky ? &m_sticky_work : nullptr );
    if ( m_weapon_data.m_is_sticky && m_sticky_work.m_exhausted )
    {
        m_sticky_deferred = true;
        return false;
    }
    if ( !high_arc && !m_sticky_search )
    {
        m_low_splash_points = points;
    }
    launch_t       best{ };
    bool           found{ false };
    const bool     local_search{ budget.m_local_depth > 0 };
    bool           alternatives{ m_weapon_data.m_is_sticky };
    launch_t       candidate{ };
    splash_point_t seed{ };
    f32            seed_score{ -1.0f };
    i32            seed_tick{ catch_up + 1 };
    i32            tested{ };
    bool           seam_fallback{ };
    {

        if ( alternatives )
        {
            g_splash.sphere( estimate, m_weapon_data.m_blast_radius, discovery_command, points, true );
        }
        if ( m_weapon_data.m_is_sticky )
        {
            if ( this->solve_sticky_air( ctx, shoot_pos, catch_up, offset, arc != 0, candidate ) &&
                 ( !found || better_splash( candidate, best, m_view_angles ) ) )
            {
                best  = candidate;
                found = true;
            }
        }
        if ( m_sticky_search && m_sticky_search->m_refine_score >= 0.0f )
        {
            for ( i32 at{ }; at < points.m_count; ++at )
            {
                const splash_point_t &point{ points.m_points[ at ] };
                if ( point.m_face == m_sticky_search->m_refine_seed.m_face &&
                     point.m_position.dist_to_sqr( m_sticky_search->m_refine_seed.m_position ) < 1024.0f &&
                     point.m_normal.dot( m_sticky_search->m_refine_seed.m_normal ) > 0.99f )
                {
                    seed       = point; // use this command's transform, never a saved transform index
                    seed_score = m_sticky_search->m_refine_score;
                    seed_tick  = math::clamp( m_sticky_search->m_refine_tick, catch_up + 1, ctx->m_track_count - 1 );
                    break;
                }
            }
        }
        for ( ;; ++tested )
        {
            if ( m_sticky_deferred )
            {
                break;
            }
            if ( tested >= points.m_count && !found && !seam_fallback )
            {
                seam_fallback = true;
                for ( i32 at{ }; at < points.m_seam_count && points.m_count < splash_points_t::CAPACITY; ++at )
                {
                    const splash_point_t &seam{ points.m_seams[ at ] };
                    bool                  duplicate{ };
                    for ( i32 prior{ }; prior < points.m_count; ++prior )
                    {
                        duplicate |= seam.m_position.dist_to_sqr( points.m_points[ prior ].m_position ) < points.m_spacing * points.m_spacing &&
                                     seam.m_normal.dot( points.m_points[ prior ].m_normal ) > 0.99999f;
                    }
                    if ( !duplicate )
                    {
                        points.m_points[ points.m_count++ ] = seam;
                    }
                }
            }
            if ( tested >= points.m_count && !found && !alternatives )
            {
                g_splash.sphere( estimate, m_weapon_data.m_blast_radius, discovery_command, points, false );
                alternatives = true;
            }
            if ( tested >= points.m_count )
            {
                break;
            }
            if ( m_weapon_data.m_is_vphysics && ( m_splash_simulations[ arc ] <= 0 ||
                                                  ( !m_weapon_data.m_is_sticky && local_search && seed_score >= 0.0f && m_splash_simulations[ arc ] <= budget.m_local_reserve ) ) )
            {
                m_sticky_deferred |= m_sticky_search != nullptr;
                break;
            }
            const splash_point_t &point{ points.m_points[ tested ] };
            const bool            hit{ point.m_air
                                           ? this->solve_sticky_air( ctx, shoot_pos, catch_up, point.m_position - estimate.m_origin, arc != 0, candidate )
                                           : this->solve_splash_point( ctx, shoot_pos, catch_up, point, arc != 0, candidate ) };
            if ( hit && ( !found || better_splash( candidate, best, m_view_angles ) ) )
            {
                best  = candidate;
                found = true;
            }
            if ( local_search && points.m_points[ tested ].m_face != static_cast< usize >( -1 ) && candidate.m_impact.m_hit && ( !m_weapon_data.m_is_sticky || candidate.m_impact.m_attached ) &&
                 ( candidate.m_splash_score > seed_score ||
                   ( candidate.m_splash_score == seed_score && points.m_points[ tested ].m_distance < seed.m_distance ) ) )
            {
                seed       = points.m_points[ tested ];
                seed_score = candidate.m_splash_score;
                seed_tick  = math::clamp( catch_up + candidate.m_arrival, catch_up + 1, ctx->m_track_count - 1 );
                if ( m_sticky_search )
                {
                    m_sticky_search->m_refine_seed  = seed;
                    m_sticky_search->m_refine_score = seed_score;
                    m_sticky_search->m_refine_tick  = seed_tick;
                }
            }
        }
    }
    if ( ( !found || splash::buried( best.m_position + best.m_impact.m_normal ) ) && !m_sticky_deferred && points.m_seams_from_shooter )
    {
        if ( this->refine_rocket_seams( ctx, shoot_pos, catch_up, estimate, best ) )
        {
            found = true;
        }
    }
    const auto stands{ [ & ]( launch_t &shot )
    {
        if ( !points.m_seams_from_shooter || shot.m_ticked || !splash::buried( shot.m_position + shot.m_impact.m_normal ) )
        {
            return true;
        }
        projectile_setup_t setup{ };
        i32                ticks{ ctx->m_track_count };
        return this->gather_launch( shot.m_angle, ctx, catch_up, setup ) && this->confirm_rocket_seam( ctx, shoot_pos, catch_up, setup, shot, ticks );
    } };
    if ( m_sticky_deferred || !local_search || seed_score < 0.0f || ( found && ( best.m_splash_score >= 0.85f || ( !m_weapon_data.m_is_sticky && !budget.m_rocket_refinement ) ) ) )
    {
        found = found && stands( best );
        if ( found )
        {
            this->take_aimpoint( ctx, catch_up, best, tgt );
        }
        return found;
    }
    {

        splash_points_t local{ };
        local.m_limit   = budget.m_local_points * budget.m_local_depth;
        local.m_spacing = 0.5f;
        local.m_margin  = points.m_margin;
        local.m_sticky  = points.m_sticky;
        f32 step{ math::bigger( points.m_spacing * 2.0f, m_weapon_data.m_blast_radius * 0.125f ) };
        for ( i32 depth{ }; depth < budget.m_local_depth; ++depth, step *= 0.5f )
        {
            const i32 first{ local.m_count };
            g_splash.refine( seed, predicted_blast_target( ctx, seed_tick ), shoot_pos, m_weapon_data.m_blast_radius,
                             step, budget.m_local_points, local );
            // add_point sorts; test this small set once per level and exclude already tested positions.
            for ( i32 at{ }; at < local.m_count; ++at )
            {
                if ( m_sticky_deferred || ( m_weapon_data.m_is_vphysics && m_splash_simulations[ arc ] <= 0 ) )
                {
                    m_sticky_deferred |= m_sticky_search != nullptr;
                    break;
                }
                bool duplicate{ false };
                for ( i32 prior{ }; prior < tested; ++prior )
                {
                    duplicate |= local.m_points[ at ].m_position.dist_to_sqr( points.m_points[ prior ].m_position ) < 0.0001f &&
                                 local.m_points[ at ].m_normal.dot( points.m_points[ prior ].m_normal ) > 0.99999f;
                }
                if ( duplicate || local.m_points[ at ].m_tested )
                {
                    continue;
                }
                splash_point_t probe{ local.m_points[ at ] };
                local.m_points[ at ].m_tested = true;
                const bool refined{ this->solve_splash_point( ctx, shoot_pos, catch_up, probe, arc != 0, candidate ) };
                if ( refined && ( !found || better_splash( candidate, best, m_view_angles ) ) )
                {
                    best      = candidate;
                    found     = true;
                    seed      = probe;
                    seed_tick = catch_up + candidate.m_arrival;
                }
            }
            if ( m_sticky_deferred || local.m_count == first || ( found && best.m_splash_score >= 0.85f ) )
            {
                break;
            }
        }
    }
    found = found && stands( best );
    if ( found )
    {
        this->take_aimpoint( ctx, catch_up, best, tgt );
    }
    return found;
}

bool ragebot::refine_rocket_seams( const move_sim_ctx_t *ctx, const vec3 &shoot_pos, i32 catch_up, const splash_target_t &estimate, launch_t &out )
{
    if ( g_holdaim_mgr.held( ) )
    {
        return false;
    }

    seam_point_t seams[ 16 ]{ };
    const i32    count{ g_splash.seams( estimate, shoot_pos, m_weapon_data.m_blast_radius, m_weapon_data.m_projectile_speed * TICK_INTERVAL, seams, 16 ) };

    bool     found{ };
    launch_t best{ };
    i32      remaining_steps{ 128 };

    const auto aim_through{ [ & ]( const vec3 &point, f32 roll, vec3 &angle, projectile_setup_t &setup )
    {
        if ( !this->straight_rocket( ) || !this->aim_rocket( point, ctx, catch_up, roll, angle ) )
        {
            math::vector_angles( point - shoot_pos, angle );
            angle.z = roll;
        }
        f32                best{ FLT_MAX };
        vec3               kept{ angle };
        projectile_setup_t gathered{ };
        for ( i32 pass{ }; pass < 6 && this->gather_launch( angle, ctx, catch_up, gathered ); ++pass )
        {
            const f32 miss{ line_miss( gathered, point ) };
            if ( !( miss < best ) )
            {
                break;
            }
            best  = miss;
            kept  = angle;
            setup = gathered;

            vec3 wanted{ };
            math::vector_angles( point - gathered.m_position, wanted );
            angle.x = math::clamp( angle.x + math::normalize_yaw( wanted.x - gathered.m_angles.x ), -89.0f, 89.0f );
            angle.y = math::normalize_yaw( angle.y + math::normalize_yaw( wanted.y - gathered.m_angles.y ) );
        }
        angle = kept;
        return best != FLT_MAX;
    } };

    const auto take{ [ & ]( const vec3 &angle, const projectile_setup_t &setup, const vec3 &point, i32 arrival )
    {
        launch_t trial{ };
        trial.m_kind      = launch_t::kind::splash;
        trial.m_angle     = angle;
        trial.m_aim_point = point;
        trial.m_arrival   = arrival;
        if ( catch_up + arrival >= ctx->m_track_count || !this->confirm_rocket_seam( ctx, shoot_pos, catch_up, setup, trial, remaining_steps ) )
        {
            return false;
        }
        if ( !found || better_splash( trial, best, m_view_angles ) )
        {
            best  = trial;
            found = true;
        }
        return true;
    } };

    const auto measure{ [ & ]( const seam_point_t &seam, const projectile_setup_t &launch, i32 &tick, f32 &error, bool &inside )
    {
        vec3 direction{ };
        math::angle_vectors( launch.m_angles, &direction );
        const vec3 velocity{ direction * launch.m_speed };
        const i32  horizon{ math::smaller( ctx->m_track_count - catch_up - 1, TIME_TO_TICKS( launch.m_lifetime ) ) };

        if ( seam.m_rounded )
        {
            const f32 step{ launch.m_speed * TICK_INTERVAL };
            f32       low{ }, high{ };
            if ( !splash::rounded_window( seam, launch.m_position, direction, step, low, high ) )
            {
                return false;
            }

            CTraceFilterWorldOnly filter{ };
            Ray_t                 ray{ };
            trace_t               world{ };
            ray.Init( launch.m_position, launch.m_position + direction * ( high + step ) );
            g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &world );
            if ( world.startsolid )
            {
                return false;
            }

            vec3      explosion{ };
            const f32 wall{ world.fraction * ( high + step ) };
            const i32 reached{ static_cast< i32 >( wall / step ) + 1 };
            if ( world.fraction < 1.0f &&
                 splash::rounded_explosion( seam, world.endpos, direction, wall - step * static_cast< f32 >( reached - 1 ), explosion ) &&
                 seam.m_wall.dot( explosion ) < seam.m_wall_distance )
            {
                tick   = reached;
                error  = 0.0f;
                inside = true;
                return tick < horizon;
            }
            if ( wall < low )
            {
                return false;
            }
            high = math::smaller( high, wall );

            vec3 end{ launch.m_position };
            i32  at{ };
            while ( at < horizon && ( tick ? at < tick : ( end - launch.m_position ).dot( direction ) < low ) )
            {
                end += velocity * TICK_INTERVAL;
                ++at;
            }
            if ( at >= horizon )
            {
                return false;
            }

            const f32 travel{ ( end - launch.m_position ).dot( direction ) };
            tick   = at;
            error  = travel - ( low + high ) * 0.5f;
            inside = travel < wall && splash::rounded_explosion( seam, end, direction, step, explosion ) &&
                     seam.m_wall.dot( explosion ) < seam.m_wall_distance;
            return true;
        }

        const f32 facing{ seam.m_normal.dot( direction ) };
        const f32 closing{ -seam.m_wall.dot( direction ) };
        if ( facing >= 0.0f || closing <= 0.0f )
        {
            return false;
        }

        vec3      contact{ launch.m_position + direction * ( seam.m_normal.dot( seam.m_position - launch.m_position ) / facing ) };
        const f32 lifted{ seam.m_displacement ? 0.0f : static_cast< f32 >( DIST_EPSILON ) / -facing };
        if ( seam.m_wall.dot( contact ) - seam.m_wall_distance + closing * lifted >= static_cast< f32 >( DIST_EPSILON ) )
        {
            return false;
        }

        vec3 previous{ launch.m_position }, end{ launch.m_position };
        i32  at{ };
        while ( at < horizon && ( tick ? at < tick : ( end - contact ).dot( direction ) < 0.0f ) )
        {
            previous = end;
            end += velocity * TICK_INTERVAL;
            ++at;
        }
        if ( at >= horizon )
        {
            return false;
        }

        const vec3 segment{ end - previous };
        contact = previous + segment * ( seam.m_normal.dot( seam.m_position - previous ) / seam.m_normal.dot( segment ) );
        const f32 depth{ seam.m_wall.dot( contact ) - seam.m_wall_distance };
        if ( depth <= 0.0f )
        {
            return false;
        }

        const f32 past{ ( end - contact ).dot( direction ) };
        tick   = at;
        error  = past - depth / closing * 0.5f;
        inside = past >= 0.0f && seam.m_wall.dot( end ) > seam.m_wall_distance &&
                 depth + closing * lifted + seam.m_wall.dot( seam.m_normal ) < 0.0f;
        return true;
    } };

    const auto open_to{ [ & ]( const projectile_setup_t &setup, i32 ticks )
    {
        if ( ticks <= 0 )
        {
            return true;
        }

        vec3 direction{ };
        math::angle_vectors( setup.m_angles, &direction );
        const vec3 velocity{ direction * setup.m_speed };
        vec3       end{ setup.m_position };
        for ( i32 tick{ }; tick < ticks; ++tick )
        {
            end += velocity * TICK_INTERVAL;
        }

        CTraceFilterProjectile filter{ };
        filter.m_pSkip         = g_cl.m_local;
        filter.m_pSkipTarget   = ctx->m_player_to_predict;
        filter.m_players_block = true;
        filter.m_skip_team     = g_cl.m_local->m_iTeamNum( );
        const f32 length{ end.dist_to( setup.m_position ) };
        const f32 early{ math::smaller( length, setup.m_speed * 0.25f ) };
        Ray_t     ray{ };
        trace_t   trace{ };
        ray.Init( setup.m_position, setup.m_position + direction * early );
        g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );
        if ( trace.startsolid || trace.fraction < 1.0f || early >= length )
        {
            return !trace.startsolid && trace.fraction >= 1.0f;
        }

        filter.m_skip_team = 0;
        ray.Init( setup.m_position + direction * early, end );
        g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );
        return !trace.startsolid && trace.fraction >= 1.0f;
    } };

    const auto place_on_tick{ [ & ]( const seam_point_t &seam, f32 roll, vec3 &aim, vec3 &angle, projectile_setup_t &setup, bool &placed,
                                     i32 &segment )
    {
        i32  tick{ };
        f32  error{ };
        bool inside{ };
        if ( !measure( seam, setup, tick, error, inside ) || inside )
        {
            placed  = inside;
            segment = tick;
            return true;
        }

        if ( !open_to( setup, tick - 2 ) )
        {
            return false;
        }

        const auto fits{ [ & ]( f32 shift )
        {
            const vec3 blast{ seam.m_position + seam.m_slide * shift + seam.m_normal };
            return shift >= seam.m_low && shift <= seam.m_high &&
                   blast.dist_to_sqr( math::nearest_on_box( estimate.m_mins, estimate.m_maxs, blast ) ) <=
                       m_weapon_data.m_blast_radius * m_weapon_data.m_blast_radius;
        } };

        vec3 direction{ };
        math::angle_vectors( setup.m_angles, &direction );
        const f32  rate{ seam.m_slide.dot( direction ) };
        const f32  back{ error / rate };
        const f32  forward{ ( error - setup.m_speed * TICK_INTERVAL ) / rate };
        const bool back_fits{ fits( back ) };
        const bool forward_fits{ tick > 1 && fits( forward ) };
        if ( !back_fits && !forward_fits )
        {
            return false;
        }

        f32 shift{ back };
        if ( forward_fits && ( !back_fits || math::abs( forward ) < math::abs( back ) ) )
        {
            shift = forward;
            error -= setup.m_speed * TICK_INTERVAL;
            --tick;
        }

        const vec3 base{ aim };
        f32        last_shift{ }, last_error{ error }, kept{ FLT_MAX };
        for ( i32 pass{ }; pass < 6 && remaining_steps > 0; ++pass, --remaining_steps )
        {
            vec3               moved_angle{ };
            projectile_setup_t moved{ };
            const vec3         moved_aim{ base + seam.m_slide * shift };
            if ( !aim_through( moved_aim, roll, moved_angle, moved ) || !measure( seam, moved, tick, error, inside ) )
            {
                break;
            }
            if ( inside && math::abs( error ) < kept )
            {
                kept    = math::abs( error );
                aim     = moved_aim;
                angle   = moved_angle;
                setup   = moved;
                placed  = true;
                segment = tick;
            }

            if ( !( math::abs( error ) < math::abs( last_error ) * 0.5f ) )
            {
                break;
            }
            const f32 next{ shift - error * ( shift - last_shift ) / ( error - last_error ) };
            last_shift = shift;
            last_error = error;
            shift      = next;
            if ( !fits( shift ) )
            {
                break;
            }
        }
        return placed;
    } };

    const auto landing{ [ & ]( const seam_point_t &seam, const projectile_setup_t &launch, f32 &offset )
    {
#pragma clang fp contract( off )
        const usize hit{ face_axis( seam.m_normal ) };
        const usize across{ face_axis( seam.m_wall ) };
        const f32   face{ seam.m_position[ hit ] - seam.m_normal[ hit ] * static_cast< f32 >( DIST_EPSILON ) };
        const f32   plane{ seam.m_wall[ across ] * seam.m_wall_distance };
        const i32   horizon{ math::smaller( ctx->m_track_count - catch_up - 1, TIME_TO_TICKS( launch.m_lifetime ) ) };
        vec3        direction{ };
        math::angle_vectors( launch.m_angles, &direction );
        const vec3 velocity{ direction * launch.m_speed };
        vec3       from{ launch.m_position };
        for ( i32 tick{ }; tick < horizon; ++tick )
        {
            const vec3 to{ from + velocity * TICK_INTERVAL };
            if ( ( to[ hit ] - seam.m_position[ hit ] ) * seam.m_normal[ hit ] > 0.0f )
            {
                from = to;
                continue;
            }

            const vec3 delta{ to - from };
            const f32  entry{ ( face - from[ hit ] + seam.m_normal[ hit ] * static_cast< f32 >( DIST_EPSILON ) ) * ( 1.0f / delta[ hit ] ) };
            f32        leave{ FLT_MAX };
            for ( usize axis{ }; axis < 3; ++axis )
            {
                if ( axis != across && delta[ axis ] != 0.0f )
                {
                    const f32 bound{ delta[ axis ] > 0.0f ? seam.m_wall_maxs[ axis ] : seam.m_wall_mins[ axis ] };
                    leave = math::smaller( leave, ( bound - from[ axis ] ) * ( 1.0f / delta[ axis ] ) );
                }
            }
            const f32 cross{ delta[ across ] * seam.m_wall[ across ] < 0.0f ? ( plane - from[ across ] ) * ( 1.0f / delta[ across ] ) : FLT_MAX };
            offset = seam.m_wall[ across ] * ( ( from[ across ] - plane ) + entry * delta[ across ] );
            return entry * delta[ across ] + from[ across ] == plane && cross > leave && ( seam.m_diagonal || cross <= 1.0f );
        }
        return false;
    } };

    const auto corner_shot{ [ & ]( const seam_point_t &seam )
    {
        constexpr f32 SHIFTS[]{ 0.0f, 0.5f, -0.5f, 1.0f, -1.0f };
        constexpr i32 SECANT{ 256 }, GALLOP{ 12 }, CANDIDATES{ 3 };
        const usize   axis{ math::abs( seam.m_wall.z ) < 0.5f ? 1u : 0u };
        const usize   along{ 3u - face_axis( seam.m_normal ) - face_axis( seam.m_wall ) };
        const f32     plane{ math::abs( seam.m_wall_distance ) };
        const f32     half_step{ ( __builtin_bit_cast( f32, __builtin_bit_cast( u32, plane ) + 1u ) - plane ) * 0.5f };
        for ( i32 roll{ }; roll < 2; ++roll )
        {
            if ( roll == 1 && !this->rolls_allowed( seam.m_position + seam.m_wall * ( half_step * 2.0f ) ) )
            {
                break;
            }

            for ( const f32 shift : SHIFTS )
            {
                if ( shift < seam.m_low || shift > seam.m_high )
                {
                    continue;
                }

                seam_point_t shifted{ seam };
                shifted.m_position[ along ] += shift;

                vec3               base{ };
                projectile_setup_t setup{ };
                if ( remaining_steps <= 0 || !aim_through( shifted.m_position, LAUNCH_ROLLS[ roll ], base, setup ) )
                {
                    continue;
                }

                const i32 arrival{ static_cast< i32 >( setup.m_position.dist_to( shifted.m_position ) / ( setup.m_speed * TICK_INTERVAL ) ) };
                if ( catch_up + arrival >= ctx->m_track_count || !open_to( setup, arrival - 2 ) )
                {
                    continue;
                }

                const auto moved{ [ & ]( i32 floats )
                {
                    vec3 angle{ base };
                    angle[ axis ] = __builtin_bit_cast( f32, __builtin_bit_cast( i32, base[ axis ] ) + floats );
                    return angle;
                } };

                f32 at_base{ -FLT_MAX }, at_secant{ -FLT_MAX };
                if ( landing( shifted, setup, at_base ) && take( base, setup, shifted.m_position, arrival ) )
                {
                    return;
                }
                if ( !this->gather_launch( moved( SECANT ), ctx, catch_up, setup ) )
                {
                    continue;
                }
                if ( landing( shifted, setup, at_secant ) && take( moved( SECANT ), setup, shifted.m_position, arrival ) )
                {
                    return;
                }
                if ( at_base == -FLT_MAX || at_secant == -FLT_MAX || at_secant == at_base )
                {
                    continue;
                }

                const i32  sign{ at_secant > at_base ? 1 : -1 };
                bool       took{ };
                const auto in_front{ [ & ]( i32 index )
                {
                    const vec3 angle{ moved( sign * index ) };
                    f32        offset{ -FLT_MAX };
                    took = this->gather_launch( angle, ctx, catch_up, setup ) && landing( shifted, setup, offset ) &&
                           take( angle, setup, shifted.m_position, arrival );
                    return offset >= 0.0f;
                } };

                const f32 slope{ math::abs( at_secant - at_base ) / static_cast< f32 >( SECANT ) };
                const i32 guess{ static_cast< i32 >( math::clamp( ( half_step * 0.5f - at_base ) / slope, -16384.0f, 16384.0f ) ) };
                i32       behind{ guess }, front{ guess };
                bool      bracketed{ };
                if ( in_front( guess ) )
                {
                    for ( i32 at{ }, reach{ 1 }; at < GALLOP && !took && !bracketed; ++at, reach *= 2 )
                    {
                        behind    = guess - reach;
                        bracketed = !in_front( behind );
                        front     = bracketed ? front : behind;
                    }
                } else
                {
                    for ( i32 at{ }, reach{ 1 }; at < GALLOP && !took && !bracketed; ++at, reach *= 2 )
                    {
                        front     = guess + reach;
                        bracketed = in_front( front );
                        behind    = bracketed ? behind : front;
                    }
                }
                while ( !took && bracketed && front - behind > 1 )
                {
                    const i32 middle{ behind + ( front - behind ) / 2 };
                    if ( in_front( middle ) )
                    {
                        front = middle;
                    } else
                    {
                        behind = middle;
                    }
                }
                for ( i32 at{ 1 }; !took && bracketed && at < CANDIDATES; ++at )
                {
                    in_front( front + at );
                }
                if ( took )
                {
                    return;
                }
            }
        }
    } };

    const auto plane_shot{ [ & ]( const seam_point_t &seam )
    {
        if ( seam.m_flush || seam.m_diagonal )
        {
            corner_shot( seam );
            return;
        }

        vec3               base{ };
        projectile_setup_t setup{ };
        if ( !aim_through( seam.m_position, 0.0f, base, setup ) )
        {
            return;
        }

        const f32 range{ setup.m_position.dist_to( seam.m_position ) };
        const i32 arrival{ static_cast< i32 >( range / ( setup.m_speed * TICK_INTERVAL ) ) };
        if ( catch_up + arrival >= ctx->m_track_count || !open_to( setup, arrival - 2 ) )
        {
            return;
        }

        const usize axis{ math::abs( seam.m_wall.z ) < 0.5f ? 1u : 0u };
        const f32   unit{ math::abs( base[ axis ] ) };
        const f32   shift{ range * DEG2RAD( __builtin_bit_cast( f32, __builtin_bit_cast( u32, unit ) + 1u ) - unit ) };
        const f32   plane{ math::abs( seam.m_wall_distance ) };
        const f32   drift{ static_cast< f32 >( arrival + 1 ) * ( __builtin_bit_cast( f32, __builtin_bit_cast( u32, plane ) + 1u ) - plane ) * 0.5f };
        const f32   across{ math::abs( seam.m_wall.dot( line_offset( setup, seam.m_position ) ) ) };
        const i32   reach{ static_cast< i32 >( math::ceil( ( across + drift ) / shift ) ) };
        for ( i32 at{ }; at <= 2 * reach && remaining_steps > 0; ++at )
        {
            vec3 angle{ base };
            angle[ axis ] = __builtin_bit_cast( f32, __builtin_bit_cast( i32, base[ axis ] ) + ( at & 1 ? -( at + 1 ) / 2 : at / 2 ) );
            if ( this->gather_launch( angle, ctx, catch_up, setup ) && take( angle, setup, seam.m_position, arrival ) )
            {
                return;
            }
        }
    } };

    for ( i32 at{ }; at < count && remaining_steps > 0 && !( found && best.m_splash_score >= 0.85f ); ++at )
    {
        if ( seams[ at ].m_plane )
        {
            if ( this->within_fov( shoot_pos, seams[ at ].m_position ) )
            {
                plane_shot( seams[ at ] );
            }
            continue;
        }

        const vec3 &normal{ seams[ at ].m_normal };
        const vec3  want{ seams[ at ].m_position + normal * ( seams[ at ].m_displacement || seams[ at ].m_rounded ? 0.0f : static_cast< f32 >( DIST_EPSILON ) ) };
        if ( !this->within_fov( shoot_pos, want ) )
        {
            continue;
        }

        for ( i32 roll{ }; roll < 4; ++roll )
        {
            if ( roll == 1 && !this->rolls_allowed( want ) )
            {
                break;
            }

            vec3               angle{ };
            projectile_setup_t setup{ };
            if ( ( math::abs( LAUNCH_ROLLS[ roll ] ) == 90.0f && !this->quarter_roll_allowed( ) ) ||
                 !aim_through( want, LAUNCH_ROLLS[ roll ], angle, setup ) )
            {
                continue;
            }

            vec3 aim{ want };
            bool placed{ };
            i32  segment{ };
            if ( !place_on_tick( seams[ at ], LAUNCH_ROLLS[ roll ], aim, angle, setup, placed, segment ) )
            {
                continue;
            }

            if ( placed )
            {
                const bool open{ open_to( setup, segment - 2 ) };
                const bool took{ open && take( angle, setup, aim, segment ) };
                DBGOUT( "ragebot::refine_rocket_seams: seam %i/%i disp %i roll %i placed segment %i track %i open %i took %i aim %.4f %.4f %.4f ang %.5f %.5f eye %.2f %.2f %.2f",
                        at, count, static_cast< i32 >( seams[ at ].m_displacement ), roll, segment, ctx->m_track_count - catch_up,
                        static_cast< i32 >( open ), static_cast< i32 >( took ), aim.x, aim.y, aim.z, angle.x, angle.y, shoot_pos.x, shoot_pos.y,
                        shoot_pos.z );
                if ( open )
                {
                    break;
                }
                continue;
            }
            if ( seams[ at ].m_rounded )
            {
                continue;
            }

            launch_t        trial{ };
            const contact_t contact{ this->closest_approach( angle, want, ctx, catch_up, true, trial, &setup ) };
            const bool      took{ contact.m_time >= 0.0f && contact.m_impact.m_hit && contact.m_distance <= 16.0f &&
                                  take( angle, setup, want, this->arrival_tick( contact.m_time ) ) };
            if ( took )
            {
                DBGOUT( "ragebot::refine_rocket_seams: seam %i/%i disp %i roll %i took aim %.4f %.4f %.4f ang %.5f %.5f", at, count,
                        static_cast< i32 >( seams[ at ].m_displacement ), roll, want.x, want.y, want.z, angle.x, angle.y );
            }
            if ( took || ( contact.m_time >= 0.0f && contact.m_impact.m_normal.dot( normal ) >= 0.99999f &&
                           math::abs( ( contact.m_position - want ).dot( normal ) ) <= static_cast< f32 >( DIST_EPSILON ) ) )
            {
                break;
            }
        }
    }
    if ( found )
    {
        out = best;
    }
    return found;
}

void ragebot::on_entity_deleted( C_BaseEntity *entity, u32 handle )
{
    const bool local{ entity == g_cl.m_local };
    if ( local || this->m_visual_target == handle )
    {
        this->m_visual_target = INVALID_EHANDLE_INDEX;
    }
    for ( usize at{ }; at < this->m_targets.size( ); )
    {
        if ( local || this->m_targets[ at ].m_entity == entity )
        {
            this->m_targets.erase( at );
        } else
        {
            ++at;
        }
    }
    if ( local || this->m_selected_walk.m_player_to_predict == entity )
    {
        this->m_selected_walk.m_player_to_predict = nullptr;
        this->m_selected_walk.m_prediction_broken = true;
        this->m_selected_walk.m_track_count       = 0;
        this->m_projectile_solved                 = false;
        this->m_pending_sticky                    = false;
    }
    const i32 identity{ static_cast< i32 >( handle ) };
    for ( auto &search : this->m_sticky_searches )
    {
        if ( local || search.m_target == identity || search.m_weapon == identity )
        {
            if ( this->m_sticky_search == &search )
            {
                this->m_sticky_search = nullptr;
            }
            search = sticky_search_t{ };
        }
    }
    if ( local || this->m_selected_sticky_target == identity || this->m_selected_sticky_weapon == identity )
    {
        this->m_pending_sticky         = false;
        this->m_selected_sticky_target = this->m_selected_sticky_weapon = -1;
    }
    const i32 index{ static_cast< i32 >( handle & ENT_ENTRY_MASK ) };
    if ( local || this->m_charge_weapon == index )
    {
        this->m_charge_managed = this->m_charge_starting = this->m_charge_discarded = false;
        this->m_charge_weapon                                                       = 0;
    }
    if ( local || this->m_charge_reselect == index )
    {
        this->m_charge_reselect = 0;
    }
    for ( auto &flight : this->m_flights )
    {
        if ( flight.m_impact.m_trace.m_pEnt == entity )
        {
            flight.m_impact.m_trace.m_pEnt = nullptr;
            this->m_flight_count           = 0;
        }
    }
    if ( this->m_selected_launch.m_impact.m_trace.m_pEnt == entity )
    {
        this->m_selected_launch.m_impact.m_trace.m_pEnt = nullptr;
    }
    if ( this->m_candidate_launch.m_impact.m_trace.m_pEnt == entity )
    {
        this->m_candidate_launch.m_impact.m_trace.m_pEnt = nullptr;
    }
}

bool ragebot::confirm_rocket_seam( const move_sim_ctx_t *ctx, const vec3 &shoot_pos, i32 catch_up, const projectile_setup_t &setup,
                                   launch_t &launch, i32 &remaining_steps )
{
    if ( remaining_steps <= 0 || setup.m_movetype != projectile_movetype::fly || setup.m_gravity != 0.0f ||
         !setup.m_hull.is_zero( ) || setup.m_speed <= 0.0f )
    {
        return false;
    }
    const i32 horizon{ math::smaller( ctx->m_track_count - catch_up - 1, TIME_TO_TICKS( setup.m_lifetime ) ) };
    const i32 limit{ math::smaller( horizon, launch.m_arrival + 2 ) };
    vec3      direction{ };
    math::angle_vectors( setup.m_angles, &direction );
    const vec3             velocity{ direction * setup.m_speed };
    vec3                   position{ setup.m_position };
    CTraceFilterProjectile filter{ };
    filter.m_pSkip         = g_cl.m_local;
    filter.m_pSkipTarget   = ctx->m_player_to_predict;
    filter.m_players_block = true;
    const i32 first{ math::bigger( launch.m_arrival - 2, 0 ) };
    for ( i32 tick{ }; tick < first; ++tick )
    {
        position += velocity * TICK_INTERVAL;
    }
    for ( i32 tick{ first }; tick < limit && remaining_steps > 0; ++tick )
    {
        // MOVETYPE_FLY advances in float ticks; a long ray through a rounded prop
        // edge can choose a different contact than the actual short sweep;
        const vec3 previous{ position };
        position += velocity * TICK_INTERVAL;
        filter.m_skip_team = TICKS_TO_TIME( tick ) < 0.25f ? g_cl.m_local->m_iTeamNum( ) : 0;
        Ray_t   ray{ };
        trace_t trace{ };
        ray.Init( previous, position );
        g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );
        --remaining_steps;
        if ( trace.startsolid || trace.allsolid )
        {
            return false;
        }
        if ( trace.fraction >= 1.0f )
        {
            continue;
        }
        const f32 time{ ( static_cast< f32 >( tick ) + trace.fraction ) * TICK_INTERVAL };
        if ( trace.endpos.dist_to( launch.m_aim_point ) > 16.0f || ( trace.surface.flags & SURF_SKY ) )
        {
            return false;
        }
        launch.m_position = trace.endpos;
        launch.m_time     = time;
        launch.m_arrival  = this->arrival_tick( time );
        launch.m_ticked   = true;
        launch.m_impact   = impact_t{ trace.endpos, trace.plane.normal, time, true, trace, false };
        const bool hurts{ catch_up + launch.m_arrival < ctx->m_track_count && this->check_splash_damage( ctx, shoot_pos, catch_up, launch ) };
        DBGOUT( "ragebot::confirm_rocket_seam: tick %i end %.4f %.4f %.4f normal %.5f %.5f %.5f buried %i hurts %i", tick, trace.endpos.x,
                trace.endpos.y, trace.endpos.z, trace.plane.normal.x, trace.plane.normal.y, trace.plane.normal.z,
                static_cast< i32 >( splash::buried( trace.endpos + trace.plane.normal ) ), static_cast< i32 >( hurts ) );
        return hurts;
    }
    return false;
}

bool ragebot::check_splash_damage( const move_sim_ctx_t *ctx, const vec3 &shoot_pos, i32 catch_up, launch_t &launch )
{
    const i32 tick{ catch_up + launch.m_arrival };
    if ( tick < 0 || tick >= ctx->m_track_count )
    {
        return false;
    }
    if ( !this->within_fov( shoot_pos, launch.m_position ) )
    {
        return false;
    }
    const splash_target_t target{ predicted_blast_target( ctx, tick ) };
    vec3                  explosion{ };
    f32                   radius{ m_weapon_data.m_blast_radius };
    if ( m_weapon_data.m_is_sticky )
    {
        if ( !launch.m_air && ( !launch.m_impact.m_hit || !launch.m_impact.m_attached ) )
        {
            return false;
        }
        if ( launch.m_air )
        {
            if ( launch.m_time < m_weapon_data.m_arm_time )
            {
                return false;
            }
            radius = splash::air_radius( radius, launch.m_time );
        }
        if ( !m_sticky_work.damage( splash::quality( ).m_damage_checks ) )
        {
            m_sticky_deferred = true;
            return false;
        }
        if ( !splash::grenade_origin( launch.m_position, nullptr, explosion, &target ) )
        {
            return false;
        }
    } else
    {
        const trace_t &impact{ launch.m_impact.m_trace };
        if ( !launch.m_impact.m_hit || impact.startsolid || impact.allsolid || ( impact.surface.flags & SURF_SKY ) )
        {
            return false;
        }
        explosion = launch.m_position + launch.m_impact.m_normal;
    }

    if ( this->m_projectile_profile.m_projectile_hitchance > 0 )
    {
        const f32 distance{ explosion.dist_to( math::nearest_on_box( target.m_mins, target.m_maxs, explosion ) ) };
        const f32 tolerance{ math::bigger( radius - distance, 0.0f ) };
        if ( this->projectile_hitchance( ctx, tick, tolerance ) < this->required_hitchance( launch.m_arrival ) )
            return false;
    }
    const f32 score{ splash::falloff( explosion, radius, m_weapon_data.m_blast_falloff, target ) };
    if ( score <= 0.0f )
    {
        return false;
    }
    launch.m_splash_score = score;
    if ( !splash::reaches( explosion, radius, target ) )
    {
        return false;
    }
    return true;
}

bool ragebot::solve_splash_point( const move_sim_ctx_t *ctx, const vec3 &shoot_pos, i32 catch_up,
                                  const splash_point_t &point, bool high_arc, launch_t &out )
{
    out = launch_t{ };
    if ( !this->m_projectile_profile.m_projectile_splashbot )
    {
        return false;
    }
    if ( !this->within_fov( shoot_pos, point.m_position ) )
    {
        return false;
    }
    if ( m_weapon_data.m_is_vphysics && m_splash_simulations[ high_arc ] <= 0 )
    {
        return false;
    }
    const vec3              hull{ ( m_weapon_data.m_hull_maxs - m_weapon_data.m_hull_mins ) * 0.5f };
    const vec3              centre{ ( m_weapon_data.m_hull_maxs + m_weapon_data.m_hull_mins ) * 0.5f };
    const f32               support{ math::abs( point.m_normal.x ) * hull.x + math::abs( point.m_normal.y ) * hull.y +
                                     math::abs( point.m_normal.z ) * hull.z - point.m_normal.dot( centre ) };
    const bool              straight{ this->straight_rocket( ) };
    const f32               lift{ point.m_face == static_cast< usize >( -1 ) ? 0.0f : static_cast< f32 >( DIST_EPSILON ) };
    const vec3              aim{ point.m_position + point.m_normal * ( straight ? lift : support + 0.001f ) };
    ballistics::intercept_t roots[ 8 ]{ };
    i32                     root_count{ };
    if ( m_weapon_data.m_is_sticky )
    {
        const vec3 path[]{ aim, aim };
        root_count = ballistics::sticky_intercepts( shoot_pos, path, 2, vec3{ },
                                                    TICKS_TO_TIME( ctx->m_track_count - 1 - catch_up ), m_launch_setup.m_speed,
                                                    m_launch_setup.m_up_kick + m_launch_setup.m_jitter_up, m_launch_setup.m_jitter_right,
                                                    g_cl.m_vm_flipped ? -8.0f : 8.0f, m_weapon_data.m_gravity, roots, 8 );
    }
    bool buried{ m_weapon_data.m_is_sticky && catch_up + 1 < ctx->m_track_count };
    for ( i32 at{ catch_up + 1 }; at < ctx->m_track_count && buried; ++at )
    {
        buried = math::overlaps( aim, aim, ctx->m_track[ at ] + ctx->m_pred_mins, ctx->m_track[ at ] + ctx->m_pred_maxs );
    }

    const i32 first{ high_arc && root_count > 1 ? 1 : 0 };
    const i32 count{ high_arc ? math::bigger( root_count - first, 1 ) : 1 };
    for ( i32 root{ first }; root < first + count; ++root )
    {
        if ( !this->begin_sticky_candidate( ) || m_sticky_deferred || buried )
        {
            continue;
        }
        launch_t launch{ };
        if ( root < root_count )
        {
            launch.m_angle    = roots[ root ].m_angle;
            launch.m_time     = roots[ root ].m_time;
            launch.m_arrival  = this->arrival_tick( launch.m_time );
            launch.m_high_arc = high_arc;
        } else if ( !this->solve_launch( this->muzzle_towards( shoot_pos, aim ), aim, high_arc, launch ) )
        {
            continue;
        }

        if ( !this->verify_launch( aim, ctx, catch_up, true, launch ) )
        {
            continue;
        }
        if ( this->check_splash_damage( ctx, shoot_pos, catch_up, launch ) )
        {
            out = launch;
            return true;
        }
        out = launch;
    }
    return false;
}

bool ragebot::solve_sticky_air( const move_sim_ctx_t *ctx, const vec3 &shoot_pos, i32 catch_up,
                                const vec3 &offset, bool high_arc, launch_t &out )
{
    out = launch_t{ };
    if ( !this->m_projectile_profile.m_projectile_splashbot )
    {
        return false;
    }

    ballistics::intercept_t roots[ 8 ]{ };
    const i32               root_count{ this->sticky_intercepts( shoot_pos, offset, ctx, catch_up, roots ) };
    const i32               first{ high_arc && root_count > 1 ? 1 : 0 };
    const i32               count{ high_arc ? math::bigger( root_count - first, 1 ) : 1 };
    for ( i32 root{ first }; root < first + count; ++root )
    {
        if ( !this->begin_sticky_candidate( ) || m_sticky_deferred )
        {
            continue;
        }
        launch_t launch{ };
        if ( root < root_count )
        {
            launch.m_angle    = roots[ root ].m_angle;
            launch.m_time     = roots[ root ].m_time;
            launch.m_arrival  = this->arrival_tick( launch.m_time );
            launch.m_high_arc = high_arc;
        } else if ( !this->solve_intercept( shoot_pos, offset, ctx, catch_up, high_arc, launch, true ) )
        {
            continue;
        }
        launch.m_air = true;
        if ( this->verify_launch( offset, ctx, catch_up, true, launch ) )
        {
            out = launch;
            return true;
        }
    }
    return false;
}

bool ragebot::select_sticky_air( const move_sim_ctx_t *ctx, const vec3 &shoot_pos, i32 catch_up, const launch_t &launch, launch_t &out )
{
    out = launch_t{ };
    bool found{ };
    for ( i32 at{ }; at < launch.m_air_samples.m_count; ++at )
    {
        launch_t candidate{ launch };
        candidate.m_position = launch.m_air_samples.m_samples[ at ].m_position;
        candidate.m_time     = launch.m_air_samples.m_samples[ at ].m_time;
        candidate.m_arrival  = this->arrival_tick( candidate.m_time );
        if ( this->check_splash_damage( ctx, shoot_pos, catch_up, candidate ) &&
             ( !found || better_splash( candidate, out, m_view_angles ) ) )
        {
            out   = candidate;
            found = true;
        }
    }
    if ( launch.m_impact.m_attached )
    {
        launch_t attached{ launch };
        attached.m_air       = false;
        attached.m_position  = launch.m_impact.m_position;
        attached.m_aim_point = attached.m_position;
        attached.m_time      = launch.m_impact.m_time;
        attached.m_arrival   = this->arrival_tick( attached.m_time );
        if ( this->check_splash_damage( ctx, shoot_pos, catch_up, attached ) &&
             ( !found || better_splash( attached, out, m_view_angles ) ) )
        {
            out   = attached;
            found = true;
        }
    }
    if ( found )
    {
        return true;
    }

    for ( i32 flight_index{ }; flight_index < m_flight_count; ++flight_index )
    {
        const flight_cache_t &flight{ m_flights[ flight_index ] };
        if ( !flight.m_splash || flight.m_angle != launch.m_angle )
        {
            continue;
        }
        i32 first{ this->arrival_tick( m_weapon_data.m_arm_time ) - 1 };
        if ( m_resume_seed && m_sticky_search && launch.m_angle == m_sticky_search->m_angle )
        {
            first = math::bigger( first, m_sticky_search->m_air_tick );
        }
        const i32 end{ flight.m_count - ( flight.m_impact.m_hit ? 1 : 0 ) };
        for ( i32 at{ first }; at < end; ++at )
        {
            const f32  time{ TICKS_TO_TIME( at + 1 ) };
            const f32  radius{ splash::air_radius( m_weapon_data.m_blast_radius, time ) };
            const vec3 target{ ctx->m_track[ catch_up + at + 1 ] };
            if ( !splash::grenade_near_target( flight.m_path[ at ], radius,
                                               splash_target_t{ nullptr, target, target + ctx->m_pred_mins, target + ctx->m_pred_maxs } ) )
            {
                continue;
            }
            bool tested{ };
            for ( i32 sample{ }; sample < launch.m_air_samples.m_count; ++sample )
            {
                tested |= launch.m_air_samples.m_samples[ sample ].m_time == time;
            }
            if ( tested )
            {
                continue;
            }
            if ( !m_sticky_work.available( ) )
            {
                m_sticky_deferred = true;
                if ( m_sticky_search )
                {
                    m_sticky_search->m_air_tick = at;
                    m_sticky_search->m_angle    = launch.m_angle;
                }
                return false;
            }
            launch_t candidate{ launch };
            candidate.m_position = flight.m_path[ at ];
            candidate.m_time     = time;
            candidate.m_arrival  = at + 1;
            if ( this->check_splash_damage( ctx, shoot_pos, catch_up, candidate ) )
            {
                out = candidate;
                return true;
            }
        }
        break;
    }
    return false;
}

struct query_scope_t
{
    query_scope_t( move_sim_ctx_t *ctx, move_sim_queries_t *queries ) :
        m_ctx{ ctx }, m_previous{ ctx->m_queries }
    {
        ctx->m_queries = queries;
    }

    ~query_scope_t( )
    {
        m_ctx->m_queries = m_previous;
    }

    move_sim_ctx_t     *m_ctx{ };
    move_sim_queries_t *m_previous{ };
};

bool ragebot::walk_target( C_TFPlayer *player, move_sim_ctx_t &ctx, i32 &catch_up ) const
{
    ctx      = move_sim_ctx_t{ player };
    catch_up = this->projectile_catch_up_ticks( player );

    if ( m_weapon_data.m_splashes )
    {
        ctx.m_walk_aim = walk_aim::blast;
    } else if ( g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_COMPOUND_BOW && this->priority_bbox_pos( &ctx ) == bbox_pos_top )
    {
        ctx.m_walk_aim = walk_aim::point;
    }

    const velocity_sample_t *seed{ g_player_data.velocity_sample( player->entindex( ), 0 ) };
    if ( seed && !m_weapon_data.m_splashes && ctx.m_walk_ground && ( ctx.m_walk_keyed || ( ctx.m_walk_input && !ctx.m_walk_unread ) ) )
        catch_up = math::bigger( catch_up - ( seed->m_lead - seed->m_batch_lead ), 0 );

    i32 horizon{ TIME_TO_TICKS( this->m_projectile_profile.m_projectile_max_sim_time ) };
    if ( m_weapon_data.m_is_sticky )
    {
        const vec3 point{ bbox_point( &ctx, this->priority_bbox_pos( &ctx ) ) };
        horizon = ballistics::arrival_tick( ballistics::initial_horizon( point - g_cl.m_local->Weapon_ShootPosition( ),
                                                                         m_weapon_data.m_launch_speed, m_weapon_data.m_gravity,
                                                                         this->m_projectile_profile.m_projectile_max_sim_time ),
                                            TICK_INTERVAL );

        ctx.m_walk_aim = walk_aim::trap;
    }

    if ( m_weapon_data.m_has_life_time )
    {
        horizon = math::smaller( horizon, TIME_TO_TICKS( m_weapon_data.m_projectile_life_time ) );
    }

    horizon = math::smaller( horizon, MAX_WALK_TICKS - 1 - catch_up );
    if ( horizon <= 0 )
    {
        return false;
    }

    move_sim_queries_t  queries{ };
    const query_scope_t scope{ &ctx, &queries };
    for ( i32 tick{ 0 }; tick < catch_up + horizon; ++tick )
    {
        if ( m_weapon_data.m_is_sticky && ( tick & 15 ) == 0 && !m_sticky_work.available( ) )
        {
            return false;
        }
        if ( !g_move_sim.run_tick( &ctx ) )
        {
            return false;
        }
    }

    return true;
}

bool ragebot::revalidate_sticky( move_sim_ctx_t *ctx, i32 catch_up, ragebot_target_t &out )
{
    launch_t launch{ m_selected_launch };
    if ( launch.m_kind == launch_t::kind::splash && !this->m_projectile_profile.m_projectile_splashbot )
    {
        return false;
    }
    m_sticky_search   = nullptr;
    m_sticky_deferred = false;
    bool valid{ };
    if ( launch.m_family )
    {
        // the arc the shot came from, restarted at the shot's own pitch
        for ( sticky_search_t &search : m_sticky_searches )
        {
            if ( search.m_target == m_selected_sticky_target )
            {
                sticky_arc_t &arc{ search.m_arcs[ launch.m_high_arc ] };
                arc.m_pitch = launch.m_angle.x;
                valid       = this->search_sticky_arc( ctx, catch_up, launch.m_high_arc, 2, arc, launch );
                break;
            }
        }
    } else
    {
        m_resume_seed  = true;
        m_flight_count = m_flight_next            = 0;
        m_splash_simulations[ launch.m_high_arc ] = splash::quality( ).m_flights;

        valid         = this->refine_launch( launch.m_aim_point, ctx, catch_up, true, launch.m_angle.z, launch ) &&
                        ( launch.m_air || this->check_splash_damage( ctx, g_cl.m_local->Weapon_ShootPosition( ), catch_up, launch ) );
        m_resume_seed = false;
    }
    m_pending_sticky = !valid && m_sticky_work.m_exhausted;
    if ( valid )
    {
        this->take_aimpoint( ctx, catch_up, launch, out );
    }
    return valid;
}

bool ragebot::extend_sticky_walk( move_sim_ctx_t *ctx, i32 catch_up, f32 time )
{
    move_sim_queries_t  queries{ };
    const query_scope_t scope{ ctx, &queries };
    if ( time < 0.0f )
    {
        f32       lowest{ 0.0f };
        const f32 eye{ g_cl.m_local->Weapon_ShootPosition( ).z };
        for ( i32 at{ catch_up }; at < ctx->m_track_count; ++at )
        {
            lowest = math::smaller( lowest, ctx->m_track[ at ].z + ctx->m_pred_mins.z - eye );
        }
        const f32 speed{ m_weapon_data.m_launch_speed };
        const f32 gravity{ m_weapon_data.m_gravity };
        time = gravity > 0.0f ? ( speed + math::sqrt( speed * speed - 2.0f * gravity * lowest ) ) / gravity : 0.0f;
    }
    const i32 end{ math::smaller( catch_up + ballistics::arrival_tick( math::smaller( time, MAX_SIM_TIME ), TICK_INTERVAL ), MAX_WALK_TICKS - 1 ) };
    while ( ctx->m_track_count <= end )
    {
        if ( !m_sticky_work.available( ) )
        {
            m_sticky_deferred = true;
            return false;
        }
        if ( !g_move_sim.run_tick( ctx ) )
        {
            return false;
        }
    }
    return true;
}

bool ragebot::fly_sticky( const move_sim_ctx_t *ctx, i32 catch_up, const vec3 &angle, bool high_arc, sticky_flight_t &flight, launch_t &out )
{
    flight = sticky_flight_t{ };
    projectile_setup_t setup{ };
    if ( !this->gather_launch( angle, ctx, catch_up, setup ) )
    {
        return false;
    }

    const vec3               *path{ m_sticky_path };
    const i32                 budget{ ctx->m_track_count - 1 - catch_up };
    const projectile_target_t intercept{ ctx->m_track + catch_up, ctx->m_pred_mins, ctx->m_pred_maxs };
    impact_t                  impact{ };
    const i32                 count{ g_proj_sim.simulate( setup, m_sticky_path, budget, math::clamp( static_cast< i32 >( math::sqrt( static_cast< f32 >( budget ) ) ), 1, 12 ),
                                                          impact, ctx->m_player_to_predict, &m_sticky_work, &intercept ) };
    if ( impact.m_deferred )
    {
        m_sticky_deferred = true;
        return false;
    }
    if ( count <= 0 )
    {
        return false;
    }
    flight.m_flown = true;

    const vec3 middle{ ( ctx->m_pred_mins + ctx->m_pred_maxs ) * 0.5f };
    vec3       bearing{ ctx->m_track[ catch_up ] + middle - setup.m_position };
    bearing.z = 0.0f;
    bearing.normalize( );
    vec3 down{ path[ count - 1 ] };
    vec3 centre{ ctx->m_track[ catch_up + count ] + middle };
    i32  at{ };
    for ( ; at < count; ++at )
    {
        const vec3 previous{ at ? path[ at - 1 ] : setup.m_position };
        const vec3 ahead{ ctx->m_track[ catch_up + at + 1 ] + middle };
        if ( previous.z >= ahead.z && path[ at ].z < ahead.z )
        {
            down   = previous + ( path[ at ] - previous ) * ( ( previous.z - ahead.z ) / ( previous.z - path[ at ].z ) );
            centre = ahead;
            break;
        }
    }
    flight.m_error     = ( down - centre ).dot( bearing );
    flight.m_lateral   = ( down - centre ).dot( vec3( -bearing.y, bearing.x, 0.0f ) );
    flight.m_blocked   = at == count && impact.m_hit && flight.m_error < 0.0f; // stopped past the target is just long
    flight.m_ceiling   = flight.m_blocked && impact.m_normal.z < -0.5f;
    flight.m_stop      = impact.m_position;
    flight.m_truncated = at == count && !impact.m_hit && !impact.m_target_hit;

    launch_t base{ };
    base.m_angle    = angle;
    base.m_high_arc = high_arc;
    base.m_family   = true;
    base.m_impact   = impact;
    if ( impact.m_target_hit )
    {
        const i32 arrival{ ballistics::arrival_tick( impact.m_time, TICK_INTERVAL ) };
        if ( catch_up + arrival < ctx->m_track_count && this->within_fov( g_cl.m_local->Weapon_ShootPosition( ), impact.m_position ) &&
             ( this->m_projectile_profile.m_projectile_hitchance <= 0 ||
               this->projectile_hitchance( ctx, catch_up + arrival ) >= this->required_hitchance( arrival ) ) )
        {
            const vec3 touched{ ballistics::target_at( ctx->m_track, ctx->m_track_count, catch_up + impact.m_time / TICK_INTERVAL ) + middle };
            const f32  side{ math::abs( ( impact.m_position - touched ).dot( vec3( -bearing.y, bearing.x, 0.0f ) ) ) };
            flight.m_quality = 1.0f - 0.3f * math::smaller( side / ( ( ctx->m_pred_maxs.x - ctx->m_pred_mins.x ) * 0.5f ), 1.0f );

            out             = base;
            out.m_position  = impact.m_position;
            out.m_aim_point = impact.m_position;
            out.m_time      = impact.m_time;
            out.m_arrival   = arrival;
            return true;
        }
    }
    if ( !this->m_projectile_profile.m_projectile_splashbot )
    {
        return false;
    }

    const vec3 eye{ g_cl.m_local->Weapon_ShootPosition( ) };
    bool       found{ };
    if ( impact.m_hit && impact.m_attached )
    {
        launch_t rest{ base };
        rest.m_kind      = launch_t::kind::splash;
        rest.m_position  = impact.m_position;
        rest.m_aim_point = impact.m_position;
        rest.m_time      = impact.m_time;
        rest.m_arrival   = this->arrival_tick( impact.m_time );
        if ( this->check_splash_damage( ctx, eye, catch_up, rest ) )
        {
            out   = rest;
            found = true;
        }
    }

    const i32 last{ count - ( impact.m_hit || impact.m_target_hit ? 1 : 0 ) };
    i32       first_open{ last }, last_open{ -1 };
    for ( i32 tick{ this->arrival_tick( m_weapon_data.m_arm_time ) - 1 }; tick < last; ++tick )
    {
        const vec3 origin{ ctx->m_track[ catch_up + tick + 1 ] };
        if ( splash::grenade_near_target( path[ tick ], splash::air_radius( m_weapon_data.m_blast_radius, TICKS_TO_TIME( tick + 1 ) ),
                                          splash_target_t{ nullptr, origin, origin + ctx->m_pred_mins, origin + ctx->m_pred_maxs } ) )
        {
            first_open = math::smaller( first_open, tick );
            last_open  = tick;
        }
    }
    splash_air_samples_t samples{ };
    splash::select_air_times( path, ctx->m_track + catch_up + 1, first_open, last_open, ctx->m_pred_mins, ctx->m_pred_maxs,
                              m_weapon_data.m_blast_radius, m_weapon_data.m_blast_falloff, samples );
    for ( i32 sample{ }; sample < samples.m_count && !m_sticky_deferred; ++sample )
    {
        launch_t burst{ base };
        burst.m_kind      = launch_t::kind::splash;
        burst.m_air       = true;
        burst.m_position  = samples.m_samples[ sample ].m_position;
        burst.m_aim_point = burst.m_position;
        burst.m_time      = samples.m_samples[ sample ].m_time;
        burst.m_arrival   = this->arrival_tick( burst.m_time );
        if ( this->check_splash_damage( ctx, eye, catch_up, burst ) && ( !found || better_splash( burst, out, m_view_angles ) ) )
        {
            out   = burst;
            found = true;
        }
    }
    flight.m_quality = found ? out.m_splash_score : 0.0f;
    return found;
}

bool ragebot::search_sticky_arc( move_sim_ctx_t *ctx, i32 catch_up, bool high_arc, i32 allowance, sticky_arc_t &arc, launch_t &out )
{
    const f32 speed{ m_launch_setup.m_speed };
    if ( speed < arc.m_short_speed )
    {
        return false;
    }

    const vec3 eye{ g_cl.m_local->Weapon_ShootPosition( ) };
    const vec3 middle{ ( ctx->m_pred_mins + ctx->m_pred_maxs ) * 0.5f };
    const vec3 delta{ ctx->m_track[ catch_up ] + middle - eye };
    const f32  distance{ delta.length2d( ) };
    const f32  launch{ m_weapon_data.m_launch_speed };
    const f32  gravity{ m_weapon_data.m_gravity };
    // past the vphysics cap more charge adds no reach
    const f32  capped{ math::sqrt( math::bigger( 2000.0f * 2000.0f - m_launch_setup.m_up_kick * m_launch_setup.m_up_kick, 0.0f ) ) };
    f32        vacuum{ };
    if ( !ballistics::flight_time( delta, launch, gravity, high_arc, vacuum ) )
    {
        const f32 needed{ math::sqrt( gravity * ( delta.z + delta.length( ) ) ) };
        arc.m_short_speed = math::smaller( math::sqrt( math::bigger( needed * needed - m_launch_setup.m_up_kick * m_launch_setup.m_up_kick, 0.0f ) ), capped );
        arc.m_short_by    = distance - ( launch * launch >= 2.0f * gravity * delta.z
                                             ? launch / gravity * math::sqrt( launch * launch - 2.0f * gravity * delta.z )
                                             : 0.0f );
        return false;
    }

    const f32 horizon{ high_arc ? math::smaller( vacuum * 1.25f + 0.25f, MAX_SIM_TIME )
                                : math::smaller( vacuum * 2.0f + 0.25f, this->m_projectile_profile.m_projectile_max_sim_time ) };
    if ( catch_up + ballistics::arrival_tick( horizon, TICK_INTERVAL ) >= ctx->m_track_count )
    {
        this->extend_sticky_walk( ctx, catch_up, horizon );
        if ( m_sticky_deferred )
        {
            return false;
        }
    }

    ballistics::intercept_t roots[ 8 ]{ };
    const i32               found{ this->sticky_intercepts( eye, middle, ctx, catch_up, roots ) };
    if ( found < ( high_arc ? 2 : 1 ) )
    {
        return false;
    }
    const ballistics::intercept_t &root{ roots[ high_arc ? found - 1 : 0 ] };
    if ( !arc.m_seeded || arc.m_short_speed > 0.0f || math::abs( speed - arc.m_speed ) > speed * 0.25f )
    {
        arc         = sticky_arc_t{ };
        arc.m_pitch = root.m_angle.x;
        arc.m_slope = ballistics::reach_slope( launch, gravity, delta.z, distance, DEG2RAD( -root.m_angle.x ), high_arc );
    }

    if ( math::abs( speed - arc.m_held_speed ) > speed * 0.03f || ctx->m_track[ catch_up ].dist_to_sqr( arc.m_held_origin ) >= 1024.0f )
    {
        arc.m_lower       = -90.0f;
        arc.m_upper       = 90.0f;
        arc.m_rested      = false;
        arc.m_held_speed  = speed;
        arc.m_held_origin = ctx->m_track[ catch_up ];
    } else if ( arc.m_rested )
    {
        return false;
    }

    f32  pitch{ arc.m_pitch }, slope{ arc.m_slope };
    f32  lower{ arc.m_lower }, upper{ arc.m_upper };
    f32  best_pitch{ pitch }, best_error{ FLT_MAX }, previous_pitch{ }, previous_error{ };
    f32  shot_pitch{ }, shot_quality{ -1.0f };
    bool has_previous{ }, settled{ };
    for ( i32 flown{ }; flown < allowance && !settled; ++flown )
    {
        sticky_flight_t flight{ };
        launch_t        shot{ };
        if ( this->fly_sticky( ctx, catch_up, vec3( pitch, root.m_angle.y + arc.m_yaw, 0.0f ), high_arc, flight, shot ) &&
             flight.m_quality > shot_quality )
        {
            out          = shot;
            shot_pitch   = pitch;
            shot_quality = flight.m_quality;
        }

        if ( !flight.m_flown || shot_quality >= 0.85f || ( shot_quality >= 0.0f && pitch != shot_pitch ) )
        {
            break;
        }

        bool flatter{ };
        f32  next{ };
        if ( flight.m_blocked )
        {
            launch_t   low{ }, high{ };
            const vec3 edge{ flight.m_stop + vec3( 0.0f, 0.0f, flight.m_ceiling ? -12.0f : 12.0f ) };
            const bool roots{ this->solve_launch( eye, edge, false, low ) && this->solve_launch( eye, edge, true, high ) };
            if ( !roots && !flight.m_ceiling )
            {
                settled = true;
                break;
            }
            if ( !roots )
            {
                next = pitch + 3.0f;
            } else if ( flight.m_ceiling )
            {
                next = math::bigger( low.m_angle.x + 0.5f, pitch + 0.5f );
            } else if ( pitch < high.m_angle.x )
            {
                next = math::bigger( high.m_angle.x + 0.5f, pitch + 0.5f ); // came down onto it: flatter
            } else
            {
                next = math::smaller( low.m_angle.x - 0.5f, pitch - 0.5f ); // ran into it: climb
            }
            flatter = next > pitch;
        } else if ( flight.m_truncated )
        {
            flatter = true;
            next    = pitch + 6.0f;
        } else
        {
            arc.m_yaw -= RAD2DEG( math::atan2( flight.m_lateral, math::bigger( distance, 32.0f ) ) );
            if ( math::abs( flight.m_error ) < best_error )
            {
                best_error = math::abs( flight.m_error );
                best_pitch = pitch;
            }
            if ( has_previous )
            {
                const f32 measured{ ( flight.m_error - previous_error ) / ( pitch - previous_pitch ) };
                if ( high_arc ? measured > 0.0f : measured < 0.0f )
                {
                    slope = measured;
                } else if ( flight.m_error < 0.0f && previous_error < 0.0f )
                {
                    arc.m_short_by    = -math::bigger( flight.m_error, previous_error );
                    arc.m_short_speed = math::smaller( speed * math::sqrt( distance / math::bigger( distance - arc.m_short_by, 1.0f ) ) * 0.98f, capped );
                    break;
                }
            }
            flatter        = high_arc ? flight.m_error < 0.0f : flight.m_error > 0.0f;
            next           = math::clamp( pitch - flight.m_error / slope, pitch - 6.0f, pitch + 6.0f );
            previous_pitch = pitch;
            previous_error = flight.m_error;
            has_previous   = true;
        }
        if ( flatter )
        {
            lower = math::bigger( lower, pitch );
        } else
        {
            upper = math::smaller( upper, pitch );
        }
        if ( next <= lower || next >= upper )
        {
            if ( lower > -90.0f && upper < 90.0f )
            {
                next = ( lower + upper ) * 0.5f;
            } else
            {
                next = lower > -90.0f ? lower + 3.0f : upper - 3.0f;
            }
        }
        next    = math::clamp( next, -89.0f, 89.0f );
        settled = math::abs( next - pitch ) < 0.01f || upper - lower < 0.25f;
        pitch   = next;
    }
    arc.m_slope  = slope;
    arc.m_speed  = speed;
    arc.m_seeded = true;
    if ( shot_quality >= 0.0f )
    {
        arc.m_pitch = shot_pitch;
        return true;
    }

    arc.m_pitch  = settled && best_error != FLT_MAX ? best_pitch : pitch;
    arc.m_lower  = lower;
    arc.m_upper  = upper;
    arc.m_rested = settled;
    return false;
}

bool ragebot::solve_sticky_arcs( move_sim_ctx_t *ctx, i32 catch_up, sticky_arc_t *arcs, launch_t &out )
{
    if ( const vec3 *held{ g_holdaim_mgr.held( ) } )
    {
        sticky_flight_t flight{ };
        return this->fly_sticky( ctx, catch_up, *held, false, flight, out );
    }
    return this->search_sticky_arc( ctx, catch_up, false, 3, arcs[ 0 ], out ) ||
           ( !m_sticky_deferred && m_launch_setup.m_speed >= arcs[ 0 ].m_short_speed &&
             this->search_sticky_arc( ctx, catch_up, true, 3, arcs[ 1 ], out ) );
}

bool ragebot::solve_sticky_walk( move_sim_ctx_t *ctx, i32 catch_up, ragebot_target_t &out )
{
    const i32  command{ g_cl.m_cmd->command_number };
    const i32  target{ ctx->m_player_to_predict->GetRefEHandle( ).ToInt( ) };
    const i32  weapon{ g_cl.m_weapon->GetRefEHandle( ).ToInt( ) };
    const vec3 eye{ g_cl.m_local->Weapon_ShootPosition( ) };
    const u32  settings{ static_cast< u32 >( g_cfg.m_ragebot.m_splash_quality ) |
                         ( static_cast< u32 >( this->m_projectile_profile.m_projectile_splashbot ) << 3 ) |
                         ( static_cast< u32 >( this->m_projectile_profile.m_projectile_splashbot_mode ) << 4 ) |
                         ( static_cast< u32 >( this->m_projectile_profile.m_aimpoint_priority ) << 8 ) |
                         ( static_cast< u32 >( this->m_projectile_profile.m_projectile_aimpoints[ config::PROJECTILE_BBOX_TOP ] ) << 12 ) |
                         ( static_cast< u32 >( this->m_projectile_profile.m_projectile_aimpoints[ config::PROJECTILE_BBOX_MIDDLE ] ) << 13 ) |
                         ( static_cast< u32 >( this->m_projectile_profile.m_projectile_aimpoints[ config::PROJECTILE_BBOX_BOTTOM ] ) << 14 ) |
                         ( static_cast< u32 >( this->m_projectile_profile.m_projectile_roll_angles ) << 15 ) };
    if ( m_pending_sticky && target == m_selected_sticky_target && weapon == m_selected_sticky_weapon &&
         m_selected_sticky_generation == g_splash.generation( ) && m_selected_sticky_settings == settings )
    {
        const bool forecast{ catch_up + m_selected_launch.m_arrival < ctx->m_track_count || this->extend_sticky_walk( ctx, catch_up ) };
        if ( forecast && this->revalidate_sticky( ctx, catch_up, out ) )
        {
            return true;
        }

        m_pending_sticky = false;
        if ( m_sticky_work.m_exhausted )
        {
            return false;
        }
    }

    // this target's search, else the slot used longest ago
    sticky_search_t *state{ &m_sticky_searches[ 0 ] };
    for ( i32 at{ }; at < this->m_projectile_profile.m_projectile_max_targets; ++at )
    {
        sticky_search_t &search{ m_sticky_searches[ at ] };
        if ( search.m_target == target )
        {
            state = &search;
            break;
        }
        if ( search.m_command < state->m_command )
        {
            state = &search;
        }
    }
    if ( state->m_target != target || state->m_weapon != weapon || state->m_generation != g_splash.generation( ) ||
         state->m_settings != settings || command < state->m_command || command - state->m_command > 32 ||
         state->m_eye.dist_to_sqr( eye ) > 65536.0f || state->m_origin.dist_to_sqr( ctx->m_track[ 0 ] ) > 65536.0f ||
         state->m_mins != ctx->m_pred_mins || state->m_maxs != ctx->m_pred_maxs ||
         state->m_gravity != m_weapon_data.m_gravity || state->m_radius != m_weapon_data.m_blast_radius ||
         state->m_arm != m_weapon_data.m_arm_time || state->m_horizon != TIME_TO_TICKS( this->m_projectile_profile.m_projectile_max_sim_time ) )
    {
        *state              = sticky_search_t{ };
        state->m_target     = target;
        state->m_weapon     = weapon;
        state->m_generation = g_splash.generation( );
        state->m_settings   = settings;
        state->m_eye        = eye;
        state->m_origin     = ctx->m_track[ 0 ];
        state->m_mins       = ctx->m_pred_mins;
        state->m_maxs       = ctx->m_pred_maxs;
        state->m_gravity    = m_weapon_data.m_gravity;
        state->m_radius     = m_weapon_data.m_blast_radius;
        state->m_arm        = m_weapon_data.m_arm_time;
        state->m_horizon    = TIME_TO_TICKS( this->m_projectile_profile.m_projectile_max_sim_time );
    }
    state->m_command  = command;
    m_sticky_search   = state;
    m_sticky_deferred = m_resume_seed = false;
    m_flight_count = m_flight_next = 0;
    m_splash_started               = false;

    launch_t launch{ };
    bool     solved{ this->solve_sticky_arcs( ctx, catch_up, state->m_arcs, launch ) };
    if ( solved )
    {
        this->take_aimpoint( ctx, catch_up, launch, out );
    }

    const sticky_arc_t         &flat{ state->m_arcs[ 0 ] };
    const bool                  reachable{ m_launch_setup.m_speed >= flat.m_short_speed };
    const bool                  near{ reachable || flat.m_short_by <= m_weapon_data.m_blast_radius + ( ctx->m_pred_maxs - ctx->m_pred_mins ).length2d( ) * 0.5f };
    const bool                  splashing{ this->m_projectile_profile.m_projectile_splashbot };
    const ragebot_weapon_data_t saved_weapon{ m_weapon_data };
    const projectile_setup_t    saved_setup{ m_launch_setup };
    for ( ; !solved && !m_sticky_deferred && state->m_stage < 5 && m_sticky_work.available( ); ++state->m_stage )
    {
        if ( state->m_discovery_command == 0 )
        {
            state->m_discovery_command = command;
        }
        const bool fresh{ state->m_stage >= 2 };
        const bool arcs{ state->m_stage == 2 };
        const bool high{ state->m_stage == 1 || state->m_stage == 4 };
        const bool skip{ ( fresh && !reachable ) || ( !arcs && ( !splashing || !near ) ) ||
                         ( fresh && ( !m_charge_managed || m_charge_discarded || !g_cl.weapon_charge_pending( ) || !pre_charges_launcher( ) ||
                                      !this->fill_weapon_data( 0.0f ) || m_launch_setup.m_speed == saved_setup.m_speed ) ) };
        if ( !skip && !arcs )
        {
            this->extend_sticky_walk( ctx, catch_up, high ? -1.0f : m_weapon_data.m_arm_time + TICK_INTERVAL );
        }
        if ( skip || m_sticky_deferred || ( !arcs && catch_up + this->arrival_tick( m_weapon_data.m_arm_time ) >= ctx->m_track_count ) )
        {
            m_weapon_data  = saved_weapon;
            m_launch_setup = saved_setup;
            if ( m_sticky_deferred )
            {
                break;
            }
            state->m_cursor = state->m_roll = 0;
            state->m_seed                   = false;
            state->m_refine_score           = -1.0f;
            state->m_discovery_command      = 0;
            continue;
        }
        m_candidate_index = 0;
        m_flight_count = m_flight_next = 0;
        m_splash_simulations[ 0 ] = m_splash_simulations[ 1 ] = splash::quality( ).m_flights;

        m_sticky_probe = true;
        solved         = arcs ? this->solve_sticky_arcs( ctx, catch_up, state->m_fresh_arcs, m_candidate_launch )
                              : this->handle_splashbot( ctx, out, eye, catch_up, high );
        m_sticky_probe = false;

        m_weapon_data  = saved_weapon;
        m_launch_setup = saved_setup;
        if ( solved && fresh )
        {
            solved              = false;
            m_projectile_solved = false;
            out.m_got_hitpoint  = false;
            m_pending_sticky    = false;
            state->m_stage      = 5;
            if ( !this->m_reload_probe )
            {
                this->cancel_charge( );
            }
            break;
        }
        if ( solved )
        {
            this->take_aimpoint( ctx, catch_up, m_candidate_launch, out );
            break;
        }
        if ( m_sticky_deferred || m_sticky_work.m_exhausted )
        {
            break;
        }
        state->m_cursor = state->m_roll = 0;
        state->m_seed                   = false;
        state->m_refine_score           = -1.0f;
        state->m_discovery_command      = 0;
    }
    if ( solved || state->m_stage == 5 )
    {
        state->m_stage = state->m_cursor = state->m_roll = 0;
        state->m_seed                                    = false;
        state->m_refine_score                            = -1.0f;
        state->m_discovery_command                       = 0;
    }
    m_sticky_search = nullptr;
    m_resume_seed   = false;
    return solved;
}

bool ragebot::solve_walk( move_sim_ctx_t *ctx, i32 catch_up, ragebot_target_t &out )
{
    m_sticky_deferred = m_resume_seed = false;
    if ( m_weapon_data.m_is_sticky )
    {
        return this->solve_sticky_walk( ctx, catch_up, out );
    }

    m_splash_started          = false;
    m_splash_simulations[ 0 ] = m_splash_simulations[ 1 ] = splash::quality( ).m_flights;
    const vec3 shoot_pos{ g_cl.m_local->Weapon_ShootPosition( ) };
    const bool splashing{ this->m_projectile_profile.m_projectile_splashbot && m_weapon_data.m_splashes };
    const bool splash_first{ splashing && this->m_projectile_profile.m_projectile_splashbot_mode == config::SPLASHBOT_PREFER };

    m_flight_count = m_flight_next = 0;
    const i32 arcs{ m_weapon_data.m_gravity_mod > 0.0f ? 2 : 1 };
    for ( i32 arc{ }; arc < arcs; ++arc )
    {
        // NOTE: Hoku - 9/18/26; This code before was running splashbot once even if we didn't call for splashbot.
        // I redid the logic here to make it more efficient and only explicitly do splashbot when the user requests for it.
        // Should also help with a bunch of other weirdness like enforcing splashbot on non-splashable weapons.
        bool solved{ false };
        if ( !splashing )
        {
            solved = this->solve_box( ctx, shoot_pos, catch_up, arc != 0, out );
        } else
        {
            if ( splash_first )
            {
                solved = this->handle_splashbot( ctx, out, shoot_pos, catch_up, arc != 0 );
                if ( !solved )
                {
                    solved = this->solve_box( ctx, shoot_pos, catch_up, arc != 0, out );
                }
            } else
            {
                solved = this->solve_box( ctx, shoot_pos, catch_up, arc != 0, out );
                if ( !solved )
                {
                    solved = this->handle_splashbot( ctx, out, shoot_pos, catch_up, arc != 0 );
                }
            }
        }

        if ( solved )
        {
            return solved;
        }
    }
    return false;
}

bool ragebot::get_projectile_aimpoint( ragebot_target_t &out )
{
    Assert( out.m_entity != nullptr );
    if ( out.m_class_id != ETFClassID::CTFPlayer )
    {
        return this->fill_weapon_data( m_weapon_data.m_charge_time ) &&
               this->get_building_aimpoint( out );
    }

    Assert( m_selected_walk.m_player_to_predict == out.m_entity );
    if ( !this->fill_weapon_data( m_weapon_data.m_charge_time ) )
    {
        return false;
    }
    return m_weapon_data.m_is_sticky ? this->revalidate_sticky( &m_selected_walk, m_selected_catch_up, out )
                                     : this->solve_walk( &m_selected_walk, m_selected_catch_up, out );
}

bool ragebot::get_projectile_target( ragebot_target_t &out )
{
    if ( m_weapon_data.m_is_sticky )
    {
        m_sticky_work.begin( m_render_frame.load( ) );
    }

    move_sim_ctx_t walks[ MAX_PROJECTILE_TARGETS ]{ };
    i32            catch_ups[ MAX_PROJECTILE_TARGETS ]{ };
    i32            walked{ };

    const i32 count{ static_cast< i32 >( m_targets.size( ) ) };
    const i32 first{ m_weapon_data.m_is_sticky && count ? m_sticky_target % count : 0 };
    for ( i32 at{ }; at < count; ++at )
    {
        const i32 index{ ( first + at ) % count };
        auto     &tgt{ m_targets[ index ] };
        if ( tgt.m_class_id != ETFClassID::CTFPlayer )
        {
            if ( this->get_building_aimpoint( tgt ) )
            {
                m_sticky_target = 0;
                out             = tgt;
                return true;
            }
            continue;
        }

        if ( m_weapon_data.m_is_sticky )
        {
            if ( !m_sticky_work.available( ) )
            {
                break;
            }

            m_sticky_target = ( index + 1 ) % count;
        }

        Assert( tgt.m_should_predict );
        Assert( walked < this->m_projectile_profile.m_projectile_max_targets );

        move_sim_ctx_t &ctx{ walks[ walked ] };
        i32            &catch_up{ catch_ups[ walked ] };
        if ( !this->walk_target( tgt.m_entity->As< C_TFPlayer * >( ), ctx, catch_up ) )
        {
            continue;
        }

        ++walked;

        const bool solved{ this->solve_walk( &ctx, catch_up, tgt ) };
        if ( solved )
        {
            Assert( tgt.m_got_hitpoint );
            if ( !this->m_reload_probe )
            {
                m_selected_walk     = ctx;
                m_selected_catch_up = catch_up;
                m_sticky_target     = 0;
            }
            out = tgt;
            return true;
        }

        if ( g_cl.m_cmd->weaponselect != 0 )
        {
            break;
        }
    }

    if ( !this->m_reload_probe && !m_weapon_data.m_is_sticky &&
         this->charge_blocks_shot( walks, catch_ups, walked ) )
    {
        this->cancel_charge( );
    }

    return false;
}

bool ragebot::charge_blocks_shot( move_sim_ctx_t *walks, const i32 *catch_ups, i32 walked )
{
    if ( walked == 0 || !this->m_charge_managed || this->m_charge_discarded || !g_cl.weapon_charge_pending( ) )
    {
        return false;
    }

    switch ( g_cl.m_weapon->GetWeaponID( ) )
    {
        case TF_WEAPON_PIPEBOMBLAUNCHER:
        case TF_WEAPON_CANNON: break; // a spent charge and a burnt fuse both shorten what a fresh launch reaches
        default: return false;
    }

    const ragebot_weapon_data_t saved_weapon{ m_weapon_data };
    const projectile_setup_t    saved_setup{ m_launch_setup };
    if ( !this->fill_weapon_data( 0.0f ) )
    {
        m_weapon_data  = saved_weapon;
        m_launch_setup = saved_setup;
        return false;
    }

    if ( saved_setup.m_speed == m_launch_setup.m_speed && saved_setup.m_lifetime == m_launch_setup.m_lifetime &&
         saved_setup.m_gravity == m_launch_setup.m_gravity )
    {
        m_weapon_data  = saved_weapon;
        m_launch_setup = saved_setup;
        return false;
    }

    for ( i32 at{ 0 }; at < walked; ++at )
    {
        ragebot_target_t fresh{ };
        const bool       solved{ this->solve_walk( &walks[ at ], catch_ups[ at ], fresh ) };
        if ( solved )
        {
            m_weapon_data       = saved_weapon;
            m_launch_setup      = saved_setup;
            m_projectile_solved = false; // the fresh-charge probe is not the current shot
            return true;
        }
    }

    m_weapon_data  = saved_weapon;
    m_launch_setup = saved_setup;
    return false;
}

constexpr f32 BOX_HIT_ODDS{ 0.2432f };

i32 ragebot::projectile_hitchance( const move_sim_ctx_t *ctx, i32 through_tick, f32 splash_tolerance ) const
{
    if ( ctx->m_prediction_broken || through_tick < 0 || through_tick >= ctx->m_track_count )
        return 0;
    const f32 initial_speed{ ctx->m_track_count > 1
                                 ? ( ctx->m_track[ 1 ] - ctx->m_track[ 0 ] ).length2d( ) / TICK_INTERVAL
                                 : ctx->m_pred_velocity.length2d( ) };
    const f32 final_speed{ through_tick > 0
                               ? ( ctx->m_track[ through_tick ] - ctx->m_track[ through_tick - 1 ] ).length2d( ) / TICK_INTERVAL
                               : initial_speed };
    const f32 speed{ math::bigger( initial_speed, final_speed ) };

    constexpr f32 DECISION_SIGMA{ 0.015f }; // radians per tick, from the pre impact grades
    constexpr f32 DOUBT{ 1.4f };
    constexpr f32 STEERED_AIR_SPEED{ 900.0f };

    const f32  turn_rate{ 8.0f / math::bigger( speed, 30.0f ) * 0.4f * DOUBT };
    const bool steered_air{ ctx->m_track_air[ through_tick ] && speed <= STEERED_AIR_SPEED };
    const f32  sigma_rate{ steered_air ? turn_rate : math::bigger( DECISION_SIGMA, turn_rate ) };
    const f32  t{ static_cast< f32 >( through_tick ) };
    const f32  spread{ speed * TICK_INTERVAL * sigma_rate * t * t * 0.5f };
    const vec3 size{ ctx->m_pred_maxs - ctx->m_pred_mins };

    f32 radius{ splash_tolerance >= 0.0f ? splash_tolerance : math::smaller( size.x, size.y ) * 0.5f };
    if ( splash_tolerance < 0.0f && m_weapon_data.m_has_hull_size )
    {
        const vec3 hull{ m_weapon_data.m_hull_maxs - m_weapon_data.m_hull_mins };
        radius += math::smaller( hull.x, hull.y ) * 0.5f;
    }

    if ( radius <= 0.0f )
        return 0;

    const bool unsolved{ ctx->m_walk_ground && ( !ctx->m_walk_input || ctx->m_walk_unread ) };
    f32        odds{ };
    if ( splash_tolerance >= 0.0f )
    {
        odds = unsolved ? 0.2368f * 1.161f : 0.2368f;
    } else if ( ctx->m_walk_aim == walk_aim::point )
    {
        odds = unsolved ? 0.2164f * 1.632f : 0.2164f;
    } else
    {
        odds = ( unsolved ? BOX_HIT_ODDS * 1.477f : BOX_HIT_ODDS ) * ( ctx->m_walk_ground ? 1.0f : 1.316f );
    }

    const f32 ratio{ ( radius + spread ) / radius };
    return static_cast< i32 >( 100.0f / ( 1.0f + odds * ratio * math::sqrt( math::sqrt( ratio ) ) ) );
}

i32 ragebot::required_hitchance( i32 arrival ) const
{
    const i32 setting{ this->m_projectile_profile.m_projectile_hitchance };
    if ( setting <= 0 || !m_weapon_data.m_is_sticky || arrival <= this->arrival_tick( m_weapon_data.m_arm_time ) )
    {
        return setting;
    }

    const f32 ratio{ 100.0f / static_cast< f32 >( setting ) };
    return static_cast< i32 >( 100.0f / ( 1.0f + BOX_HIT_ODDS * ratio * math::sqrt( math::sqrt( ratio ) ) ) );
}

void ragebot::maintain_managed_charge( )
{
    if ( g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_MINIGUN )
    {
        if ( g_cfg.m_ragebot.m_preferences[ config::PREFERENCE_AUTO_REV ] &&
             g_cfg.m_ragebot.m_activate_key.m_key != 0x0 && this->should_run( ) )
        {
            g_cl.m_cmd->buttons |= IN_ATTACK2;
        }
    }

    if ( !g_cl.m_local->IsAlive( ) )
    {
        this->m_charge_managed   = false;
        this->m_charge_starting  = false;
        this->m_charge_discarded = false;
        this->m_charge_reselect  = 0; // a respawn reissues entindices; the take-back dies here
        return;
    }

    // ask for the launcher straight back; the switch is not predicted through the datamap restore
    if ( this->m_charge_reselect != 0 )
    {
        if ( g_cl.m_cmd->weaponselect == 0 )
        {
            g_cl.m_cmd->weaponselect = this->m_charge_reselect;
        }

        this->m_charge_reselect = 0;
        return;
    }

    if ( this->m_charge_discarded && !g_cl.weapon_charge_pending( ) )
    {
        this->m_charge_discarded = false;
    }

    if ( this->m_charge_managed && g_cl.m_weapon->entindex( ) != this->m_charge_weapon )
    {
        this->m_charge_managed  = false;
        this->m_charge_starting = false;
    }

    if ( !this->m_charge_managed )
    {
        this->m_charge_starting = false;

        // a pending charge that is not ours and not stale is the user's, left alone
        if ( !pre_charges_launcher( ) || !should_run( ) )
        {
            return;
        }

        switch ( g_cl.m_weapon->GetWeaponID( ) )
        {
            // pre-lighting the cannon only burns its fuse, so it never banks anything
            case TF_WEAPON_COMPOUND_BOW:
            case TF_WEAPON_PIPEBOMBLAUNCHER: break;
            default: return;
        }

        if ( ( !g_cl.weapon_charge_pending( ) || this->m_charge_discarded ) &&
             should_fire( true ) ) // ammo and the lowered bow still refuse a draw
        {
            g_cl.m_cmd->buttons |= IN_ATTACK;
            this->m_charge_managed  = true;
            this->m_charge_starting = true;
            this->m_charge_weapon   = g_cl.m_weapon->entindex( );
        }

        return;
    }

    if ( this->m_charge_discarded )
    {
        g_cl.m_cmd->buttons |= IN_ATTACK;
        return;
    }

    if ( this->m_charge_starting )
    {
        if ( g_cl.weapon_charge_pending( ) )
        {
            this->m_charge_starting = false;
        } else
        {
            if ( !should_run( ) || !should_fire( true ) )
            {
                this->m_charge_managed  = false;
                this->m_charge_starting = false;
                return;
            }

            g_cl.m_cmd->buttons |= IN_ATTACK;
            return;
        }
    }

    // Hoku says: This is bad dumb code and more importantly it's bad dumb code that doesn't make sense here.
    if ( !g_cl.weapon_charge_pending( ) )
    {
        if ( !should_fire( ) )
        {
            g_cl.m_cmd->buttons |= IN_ATTACK;
            return;
        }

        this->m_charge_managed  = false;
        this->m_charge_starting = false;
        return;
    }

    if ( !should_run( ) )
    {
        cancel_charge( );
        return;
    }

    const f32 now{ TICKS_TO_TIME( g_cl.m_local->m_nTickBase( ) ) };

    switch ( g_cl.m_weapon->GetWeaponID( ) )
    {
        case TF_WEAPON_COMPOUND_BOW:
        {
            // past ~4.5s drawn the arrow enters the +-6 degree wobble zone with nothing left to gain
            const f32 begin{ g_cl.m_weapon->As< C_TFPipebombLauncher * >( )->m_flChargeBeginTime( ) };
            if ( now - begin > 4.5f )
            {
                cancel_charge( );
                return;
            }

            break;
        }

        case TF_WEAPON_PIPEBOMBLAUNCHER:
        {
            // ItemPostFrame force launches at the charge ceiling whatever the buttons say
            const f32 begin{ g_cl.m_weapon->As< C_TFPipebombLauncher * >( )->m_flChargeBeginTime( ) };
            const f32 ceiling{ g_cl.attrib_hook( g_cl.m_weapon, 4.0f, ATLAS_XORSTR( "stickybomb_charge_rate" ).get( ) ) };

            if ( now - begin >= ceiling * 0.95f )
            {
                cancel_charge( );
                return;
            }

            break;
        }

        case TF_WEAPON_CANNON:
        {
            // holding past the fuse detonates the ball inside the barrel; the holster clears it
            const f32 detonate{ g_cl.m_weapon->As< C_TFGrenadeLauncher * >( )->m_flDetonateTime( ) };
            if ( detonate - now <= TICKS_TO_TIME( 3 ) )
            {
                cancel_charge( );
                return;
            }

            break;
        }

        default: break;
    }

    g_cl.m_cmd->buttons |= IN_ATTACK; // hold; aim_at_target strips it on the firing command
}

void ragebot::cancel_charge( )
{
    this->m_charge_managed  = false;
    this->m_charge_starting = false;

    switch ( g_cl.m_weapon->GetWeaponID( ) )
    {
        case TF_WEAPON_COMPOUND_BOW:
        {
            g_cl.m_cmd->buttons |= IN_ATTACK2;
            g_cl.m_cmd->buttons &= ~IN_ATTACK;
            return;
        }

        case TF_WEAPON_PIPEBOMBLAUNCHER:
        case TF_WEAPON_CANNON:
        {
            EHANDLE *weapons{ g_cl.m_local->m_hMyWeapons( ) };
            if ( !weapons )
            {
                return;
            }

            C_BaseEntity *swap{ nullptr };
            for ( i32 index{ }; index < 48; ++index )
            {
                C_BaseEntity *entity{ weapons[ index ].Get( ) };
                if ( !entity || entity == g_cl.m_weapon )
                {
                    continue;
                }

                swap = entity;

                if ( entity->As< C_TFWeaponBase * >( )->GetSlot( ) == 2 )
                {
                    break;
                }
            }

            if ( swap )
            {
                g_cl.m_cmd->weaponselect = swap->entindex( );

                this->m_charge_reselect  = g_cl.m_weapon->entindex( );
                this->m_charge_discarded = true;
            }

            // with nothing to swap to, dropping m_charge_managed fires the charge as a plain release
            return;
        }

        default: return;
    }
}

bool ragebot::cannon_fuse_holds( const ragebot_target_t &tgt ) const
{
    if ( !g_cfg.m_ragebot.m_preferences[ config::PREFERENCE_AUTO_CHARGE ] )
    {
        return false;
    }

    if ( tgt.m_class_id == ETFClassID::CTFPlayer && tgt.m_entity )
    {
        f32 impact{ math::remap_clamped( tgt.m_aim_origin.dist_to_sqr( g_cl.m_local->GetAbsOrigin( ) ),
                                         512.0f * 512.0f, 1024.0f * 1024.0f, 50.0f, 25.0f ) };
        if ( g_cl.m_local->IsCritBoosted( ) )
        {
            impact *= 3.0f;
        } else if ( g_cl.m_local->IsMiniCritBoosted( ) )
        {
            impact *= 1.35f;
        }

        if ( static_cast< f32 >( tgt.m_entity->As< C_TFPlayer * >( )->m_iHealth( ) ) <= impact )
        {
            return false;
        }
    }

    const i32 poll{ TIME_TO_TICKS( 0.2f ) };
    const i32 blast{ ( tgt.m_arrival / poll + 1 ) * poll }; // the first poll after the donk
    const f32 remaining{ g_cl.m_weapon->As< C_TFGrenadeLauncher * >( )->m_flDetonateTime( ) -
                         TICKS_TO_TIME( g_cl.m_local->m_nTickBase( ) ) };

    // release a tick before the fuse would land on that poll, so the server's float clock cannot carry it past
    return TIME_TO_TICKS( remaining ) > blast - 1;
}

bool ragebot::should_fire( bool ignore_timer )
{
    // a lowered bow refuses the draw until both attack buttons spent a command released
    if ( g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_COMPOUND_BOW &&
         g_cl.m_weapon->As< C_TFCompoundBow * >( )->m_bNoFire( ) )
    {
        return false;
    }

    if ( g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_FLAME_BALL && !g_cl.flame_ball_ready( g_cl.m_weapon ) )
    {
        return false;
    }

    // auto charge presses through a busy timer; the press banks and takes effect when it clears
    const auto curtime{ TICKS_TO_TIME( g_cl.m_local->m_nTickBase( ) ) };
    const bool pending_release{ !this->m_charge_discarded && g_cl.weapon_charge_pending( ) && g_cl.m_weapon->m_iClip1( ) > 0 };
    if ( !ignore_timer && ( g_cl.m_local->m_flNextAttack( ) > curtime || ( !pending_release && g_cl.m_weapon->m_flNextPrimaryAttack( ) > curtime ) ) )
    {
        return false;
    }

    // @ CTFWeaponBase::CheckReload runs earlier in the same ItemPostFrame, so a reload whose timer
    // expired this tick refills m_iClip1 and the shot leaves before we are called again
    if ( const i32 clip{ g_cl.m_weapon->m_iClip1( ) }; clip <= 0 )
    {
        if ( const i32 ammo_type{ g_cl.m_weapon->m_iPrimaryAmmoType( ) };
             ammo_type >= 0 && ( &g_cl.m_local->m_iAmmo( ) )[ ammo_type ] <= 0 )
        {
            return false;
        }
    }

    return true;
}

inline bool is_kill_check_weapon( C_TFWeaponBase *wep )
{
    switch ( wep->GetWeaponID( ) )
    {
        case TF_WEAPON_COMPOUND_BOW:
        case TF_WEAPON_SNIPERRIFLE:
        case TF_WEAPON_SNIPERRIFLE_DECAP:
        case TF_WEAPON_SNIPERRIFLE_CLASSIC: return true;
        case TF_WEAPON_REVOLVER:
        {
            return wep->As< C_TFRevolver * >( )->CanHeadshot( );
        }

        default: break;
    }

    return false;
}

bool ragebot::aimable_target_is_kill_condition( const ragebot_target_t &tgt )
{
    const auto entity{ tgt.m_entity };
    if ( tgt.m_class_id != ETFClassID::CTFPlayer )
    {
        return true;
    }

    const auto player{ entity->As< C_TFPlayer * >( ) };

    if ( !is_kill_check_weapon( g_cl.m_weapon ) )
    {
        return true;
    }

    const i32 weapon_id{ g_cl.m_weapon->GetWeaponID( ) };
    if ( tgt.m_hitbox == HITBOX_HEAD && ( weapon_id == TF_WEAPON_SNIPERRIFLE || weapon_id == TF_WEAPON_SNIPERRIFLE_DECAP ) &&
         !g_cl.m_weapon->As< C_TFSniperRifle * >( )->IsReadyForHeadshot( ) )
    {
        return false;
    }

    if ( is_weapon_sniper_rifle( g_cl.m_weapon ) && g_cfg.m_ragebot.m_preferences[ config::PREFERENCE_SNIPER_ONLY_ON_KILL ] == true )
    {
        auto damage{ g_cl.m_weapon->As< C_TFWeaponBaseGun * >( )->GetProjectileDamage( ) };
        if ( tgt.m_hitbox == HITBOX_HEAD )
        {
            damage *= TF_DAMAGE_CRIT_MULTIPLIER;
        }

        return damage >= player->m_iHealth( );
    }

    if ( is_weapon_ambassador( g_cl.m_weapon ) )
    {
        const auto revolver{ g_cl.m_weapon->As< C_TFRevolver * >( ) };
        const bool is_aiming_head{ tgt.m_hitbox == HITBOX_HEAD };
        if ( !revolver->IsReadyForHeadshot( ) && is_aiming_head )
        {
            return false;
        }

        if ( is_aiming_head || g_cl.m_local->IsCritBoosted( ) )
        {
            return true;
        }

        const auto damage{ revolver->CalcRangedDamage( player ) };
        return damage >= player->m_iHealth( );
    }

    if ( g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_COMPOUND_BOW && g_cfg.m_ragebot.m_huntsman_wait_for_kill )
    {
        auto damage{ g_cl.m_weapon->As< C_TFWeaponBaseGun * >( )->GetProjectileDamage( ) };
        if ( tgt.m_bbox_point == bbox_pos_top )
        {
            damage *= TF_DAMAGE_CRIT_MULTIPLIER;
        }

        return damage >= player->m_iHealth( );
    }

    return true;
}

void ragebot::aim_at_target( const ragebot_target_t &tgt )
{
    Assert( tgt.m_entity != nullptr );
    if ( !tgt.m_entity )
    {
        return;
    }

    vec3       aim_angle{ tgt.m_aim_angle };
    vec3       aim_point{ tgt.m_aim_origin };
    const bool managed_charge{ this->m_charge_managed };
    const bool starting_charge{ this->m_charge_starting };

    const bool thrown_jar{ g_cl.is_delayed_throw_weapon( g_cl.m_weapon ) };
    const bool able_to_fire{ this->should_fire( ) && !g_tickbase.withholds_shot( ) && this->aimable_target_is_kill_condition( tgt ) };

    if ( able_to_fire || m_weapon_data.m_should_fire_always_set_attack_bits )
    {
        if ( g_cfg.m_ragebot.m_auto_shoot )
        {
            if ( m_weapon_data.m_fires_on_attack_release &&
                 m_weapon_data.m_weapon_type == weapon_type::projectile_weapon )
            {
                // the release edge comes from the previous command's buttons, which our press never reaches
                if ( g_cl.weapon_charge_pending( ) && !this->m_charge_discarded )
                {
                    if ( g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_CANNON && this->cannon_fuse_holds( tgt ) )
                    {
                        g_cl.m_cmd->buttons |= IN_ATTACK; // the fuse still outlasts the flight; the blast has to land right behind the donk
                    } else
                    {
                        g_cl.m_cmd->buttons &= ~IN_ATTACK; // this command carries the shot
                        this->m_charge_managed  = false;   // the release ends management, not a guess about why the charge vanished
                        this->m_charge_starting = false;
                    }
                } else
                {
                    g_cl.m_cmd->buttons |= IN_ATTACK; // nock the arrow / prime the charge
                    this->m_charge_managed  = true;
                    this->m_charge_starting = true;
                    this->m_charge_weapon   = g_cl.m_weapon->entindex( );

                    this->m_pending_sticky = m_weapon_data.m_is_sticky && this->m_projectile_solved;
                }
            } else if ( m_weapon_data.m_fires_on_attack_release && ( g_cl.m_cmd->buttons & IN_ATTACK ) )
            {
                g_cl.m_cmd->buttons &= ~IN_ATTACK;
            } else
            {
                g_cl.m_cmd->buttons |= IN_ATTACK;
            }
        }
    }

    if ( m_weapon_data.m_weapon_type == weapon_type::projectile_weapon && g_cl.will_fire_this_tick( ) )
    {
        const i32 command{ g_cl.m_cmd->command_number };
        g_crits.run( );
        if ( command != g_cl.m_cmd->command_number )
        {
            g_tf2.m_CBaseEntity_SetPredictionRandomSeed.call_as< void >( g_cl.m_cmd );
        }
        if ( command != g_cl.m_cmd->command_number && ( m_weapon_data.m_is_vphysics || m_weapon_data.m_launch_spread != 0.0f ) )
        {
            ragebot_target_t revised{ tgt };
            revised.m_got_hitpoint = false;
            if ( m_weapon_data.m_is_sticky )
            {
                m_sticky_work.finalize( true );
            }
            const bool valid{ this->get_projectile_aimpoint( revised ) };
            if ( m_weapon_data.m_is_sticky )
            {
                m_sticky_work.finalize( false );
            }
            if ( !valid )
            {
                this->m_projectile_solved = false;
                this->m_visual_target     = INVALID_EHANDLE_INDEX;
                this->m_charge_managed    = managed_charge;
                this->m_charge_starting   = starting_charge;
                if ( g_cfg.m_ragebot.m_auto_shoot )
                {
                    if ( m_weapon_data.m_fires_on_attack_release )
                    {
                        g_cl.m_cmd->buttons |= IN_ATTACK;
                    } else
                    {
                        g_cl.m_cmd->buttons &= ~IN_ATTACK;
                    }
                }
                return;
            }
            aim_angle = revised.m_aim_angle;
            aim_point = revised.m_aim_origin;
        }
    }

    this->m_visual_live  = true;
    this->m_visual_point = aim_point;
    this->m_visual_time  = g_tf2.m_engine->Plat_FloatTime( );

    const u32 visual_target{ tgt.m_class_id == ETFClassID::CTFPlayer && tgt.m_entity
                                 ? static_cast< u32 >( tgt.m_entity->GetRefEHandle( ).ToInt( ) )
                                 : INVALID_EHANDLE_INDEX };
    if ( visual_target != this->m_visual_target )
    {
        this->m_visual_target_started = this->m_visual_time;
    }
    this->m_visual_target      = visual_target;
    this->m_visual_target_time = this->m_visual_time;

    const bool fire_now{ g_cl.will_fire_this_tick( ) };
    const bool psilent{ m_weapon_data.m_weapon_type == weapon_type::projectile_weapon &&
                        !thrown_jar };

    const bool shot_input{ m_weapon_data.m_fires_on_attack_release
                               ? g_cl.weapon_charge_pending( ) && !( g_cl.m_cmd->buttons & IN_ATTACK )
                               : ( g_cl.m_cmd->buttons & IN_ATTACK ) != 0 };
    const bool psilent_shot{ psilent && ( fire_now || shot_input ) };

    if ( able_to_fire && ( !psilent || psilent_shot ) )
    {
        vec3 send_angle{ aim_angle };
        if ( m_weapon_data.m_weapon_type == weapon_type::hitscan_weapon )
        {
            send_angle -= g_cl.m_local->m_vecPunchAngle( );
            // clamp_angles zeroes the roll a steered salvo was placed with
            const f32 roll{ send_angle.z };
            math::clamp_angles( send_angle );
            send_angle.z = roll;
        }

        const vec3 *held{ g_holdaim_mgr.held( ) };
        g_cl.m_cmd->viewangles = held ? *held : send_angle; // NOT DOING THIS RESULTS IN UNDER-DAMAGE: BAD!

        if ( m_weapon_data.m_weapon_type == weapon_type::hitscan_weapon &&
             tgt.m_class_id == ETFClassID::CTFPlayer )
        {
            g_cl.m_cmd->tick_count = tgt.m_tick_count + TIME_TO_TICKS( lag_compensation::lerp_time( ) );
        }

        this->m_shot_target    = m_weapon_data.m_weapon_type == weapon_type::hitscan_weapon && tgt.m_class_id == ETFClassID::CTFPlayer
                                     ? tgt.m_entity
                                     : nullptr;
        this->m_shot_eye       = g_cl.m_local->Weapon_ShootPosition( );
        this->m_shot_point     = aim_point;
        this->m_aimed_sequence = g_networking.sequence_number( );

        const lag_record_t *newest{ this->m_shot_target ? g_lagcomp.record( this->m_shot_target->entindex( ), 0 ) : nullptr };
        this->m_shot_velocity = newest ? newest->m_velocity : vec3{ };

        this->m_evaluated_sequence = tgt.m_steered ? this->m_aimed_sequence : -1;

        if ( fire_now && m_weapon_data.m_weapon_type == weapon_type::hitscan_weapon &&
             tgt.m_class_id == ETFClassID::CTFPlayer && this->m_pending_draw.m_boxes > 0 )
        {
            this->m_shot_draw        = this->m_pending_draw;
            this->m_shot_draw.m_time = g_tf2.m_engine->Plat_FloatTime( );
        }

        if ( fire_now && this->m_shot_target )
        {
            g_resolver.on_shot( this->m_shot_target->As< C_TFPlayer * >( ), tgt.m_tick_count, tgt.m_hitbox, this->m_shot_eye, aim_point );
        }

        g_cl.m_psilent_choked = psilent_shot;

        if ( fire_now && this->m_projectile_solved )
        {
            this->m_fired_path      = this->m_path;
            this->m_fired_path_time = g_tf2.m_engine->Plat_FloatTime( );
            const double travel_ticks{ static_cast< double >( this->m_fired_path.m_count - 1 + PATH_TRAVEL_FADE_TICKS ) };
            const double travel_time{ travel_ticks * TICK_INTERVAL };
            this->m_path_hold_until = this->m_fired_path_time +
                                      ( g_cfg.m_ragebot.m_projectile_path_animate
                                            ? math::bigger( 1.0, travel_time )
                                            : 1.0 );
        }
    }
}

bool ragebot::schedule_rapid_fire( const rapid_fire_plan_t &plan, u32 &withheld ) const
{
    withheld = 0;
    if ( !this->aimed_this_command( ) || !this->m_shot_target || ( plan.m_step.is_zero( ) && this->m_shot_velocity.is_zero( ) ) )
    {
        return true;
    }

    Assert( plan.m_delay > 0.0f && plan.m_shots > 0 );

    salvo_kernel_t kernel{ };
    this->build_kernel( kernel );
    if ( kernel.m_count == 0 )
    {
        kernel_add( kernel, 0.0f, 0.0f, 1.0f );
        kernel.m_range = g_cl.m_weapon->GetWeaponInfo( )->GetWeaponData( g_cl.m_weapon->m_iWeaponMode( ) ).m_flRange;
    }

    f32       open{ };
    const f32 aimed{ this->shot_pellets( plan, kernel, 0, plan.m_extra[ 0 ], open ) };
    const f32 cost{ open * TICK_INTERVAL / plan.m_delay };

    f32 now{ aimed };
    i32 fired{ plan.m_extra[ 0 ] };
    for ( i32 shot{ 1 }; shot < plan.m_shots; ++shot )
    {
        const i32 due{ plan.next_shot( fired ) };
        if ( due > plan.m_extras )
        {
            break;
        }

        f32 ignored{ };
        f32 best{ this->shot_pellets( plan, kernel, 0, due, ignored ) };
        i32 at{ due };
        for ( i32 extra{ due + 1 }; plan.m_slides && best < aimed && extra <= plan.m_extras && plan.keeps_firing( fired, extra - 1 ); ++extra )
        {
            const f32 waited{ this->shot_pellets( plan, kernel, 0, extra, ignored ) - cost * static_cast< f32 >( extra - due ) };
            if ( waited > best )
            {
                best = waited;
                at   = extra;
            }
        }

        for ( i32 extra{ due }; extra < at; ++extra )
        {
            withheld |= 1u << extra;
        }

        now += best;
        fired = at;
    }

    f32 behind{ };
    for ( i32 shot{ 1 }; shot < plan.m_shots; ++shot )
    {
        behind = math::bigger( behind, plan.m_path[ plan.m_extra[ 0 ] ] - plan.m_path[ plan.m_extra[ shot ] ] );
    }

    const i32 horizon{ behind > 0.0f ? static_cast< i32 >( behind / plan.m_step.length2d( ) ) + 1 : 1 };
    for ( i32 ticks{ 1 }; ticks <= horizon; ++ticks )
    {
        f32 later{ };
        for ( i32 shot{ }; shot < plan.m_shots; ++shot )
        {
            f32 ignored{ };
            later += this->shot_pellets( plan, kernel, ticks, plan.m_extra[ shot ], ignored );
        }

        if ( later - now > static_cast< f32 >( ticks ) * cost )
        {
            withheld = 0;
            return false;
        }
    }

    return true;
}

f32 ragebot::shot_pellets( const rapid_fire_plan_t &plan, const salvo_kernel_t &kernel, i32 ticks, i32 extra, f32 &open ) const
{
    const vec3 drift{ this->m_shot_velocity * ( TICK_INTERVAL * static_cast< f32 >( ticks ) ) };
    const vec3 first{ this->m_shot_eye + plan.m_step * static_cast< f32 >( ticks ) - drift + plan.m_axis * plan.m_path[ plan.m_extra[ 0 ] ] };
    vec3       forward{ this->m_shot_point - first };
    forward.normalize( );

    vec3 angles{ };
    math::vector_angles( forward, angles );

    vec3 right{ };
    vec3 up{ };
    math::angle_vectors( angles, nullptr, &right, &up );

    const vec3 slide{ plan.m_axis * ( plan.m_path[ extra ] - plan.m_path[ plan.m_extra[ 0 ] ] ) };
    const vec3 from{ first + slide - forward * slide.dot( forward ) };
    const i32  stride{ math::bigger( 1, ( kernel.m_count + TRACE_BUDGET - 1 ) / TRACE_BUDGET ) };

    f32 landed{ };
    open = 0.0f;
    for ( i32 stratum{ }; stratum < kernel.m_count; stratum += stride )
    {
        f32 weight{ };
        for ( i32 index{ stratum }; index < math::smaller( stratum + stride, kernel.m_count ); ++index )
        {
            weight += kernel.m_entries[ index ].m_weight;
        }

        const kernel_entry_t &entry{ kernel.m_entries[ stratum ] };
        const vec3            direction{ forward + right * ( entry.m_x * kernel.m_spread ) + up * ( entry.m_y * kernel.m_spread ) };

        vec3 hit{ };
        if ( !this->enters_pose( from, from + direction.normalized( ) * kernel.m_range, hit ) )
        {
            continue;
        }

        open += weight;
        if ( g_cl.trace_line( g_cl.m_local, this->m_shot_target, from + drift, hit + drift, MASK_SOLID | CONTENTS_HITBOX ) )
        {
            landed += weight;
        }
    }

    return landed;
}

bool ragebot::enters_pose( const vec3 &from, const vec3 &end, vec3 &hit ) const
{
    if ( math::segment_box_fraction( from, end, this->m_shot_hull_mins, this->m_shot_hull_maxs ) == FLT_MAX )
    {
        return false;
    }

    f32 nearest{ FLT_MAX };
    for ( i32 index{ }; index < this->m_shot_box_count; ++index )
    {
        const shot_box_t &box{ this->m_shot_boxes[ index ] };

        f32 entry{ };
        if ( math::ray_hits_box( from, end, box.m_bone, box.m_mins, box.m_maxs, 0.0f, &entry ) )
        {
            nearest = math::smaller( nearest, entry );
        }
    }

    if ( nearest > 1.0f )
    {
        return false;
    }

    hit = from + ( end - from ) * nearest;
    return true;
}

bool ragebot::cancel_reload( )
{
    if ( !g_cl.m_cmd || !g_cl.m_weapon || g_cl.m_weapon->m_iClip1( ) <= 0 )
    {
        return false;
    }

    // manual/managed attack was present before RunCommand, so UpdateButtonState already latched
    // it and CTFWeaponBase::ItemBusyFrame will consume it without any late input repair..
    if ( g_cl.m_cmd->buttons & IN_ATTACK )
    {
        return false;
    }

    if ( !g_cl.m_weapon->m_bInReload( ) &&
         g_cl.m_weapon->m_iReloadMode( ) == TF_RELOAD_START )
    {
        return false;
    }

    if ( !g_cfg.m_ragebot.m_auto_shoot || !should_run( ) )
    {
        return false;
    }

    if ( !this->should_fire( true ) )
    {
        return false;
    }

    const bool        sticky_probe{ g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_PIPEBOMBLAUNCHER };
    sticky_search_t   saved_searches[ MAX_PROJECTILE_TARGETS ]{ };
    projectile_work_t saved_work{ };
    sticky_search_t  *saved_search{ };
    i32               saved_sticky_target{ };
    bool              saved_pending_sticky{ };
    if ( sticky_probe )
    {
        for ( i32 index{ }; index < this->m_projectile_profile.m_projectile_max_targets; ++index )
        {
            saved_searches[ index ] = this->m_sticky_searches[ index ];
        }

        saved_work           = this->m_sticky_work;
        saved_search         = this->m_sticky_search;
        saved_sticky_target  = this->m_sticky_target;
        saved_pending_sticky = this->m_pending_sticky;
    }

    this->m_reload_probe      = true;
    this->m_projectile_solved = false;

    ragebot_target_t target{ };
    const bool       found{ this->fill_weapon_data( ) && this->find_target( target ) };

    this->m_reload_probe = false;

    if ( sticky_probe )
    {
        for ( i32 index{ }; index < this->m_projectile_profile.m_projectile_max_targets; ++index )
        {
            this->m_sticky_searches[ index ] = saved_searches[ index ];
        }

        this->m_sticky_work    = saved_work;
        this->m_sticky_search  = saved_search;
        this->m_sticky_target  = saved_sticky_target;
        this->m_pending_sticky = saved_pending_sticky;
    }

    if ( !found )
    {
        return false;
    }

    // at the budget cap a tick we hold after this command is a copy of it on the server, and with the reload aborted here
    // the copy can shoot; It shoots where the shot would have..
    if ( g_tickbase.at_budget_cap( ) )
    {
        vec3 send_angle{ target.m_aim_angle };
        if ( m_weapon_data.m_weapon_type == weapon_type::hitscan_weapon )
        {
            send_angle -= g_cl.m_local->m_vecPunchAngle( );
            math::clamp_angles( send_angle );
        }

        g_cl.m_cmd->viewangles = send_angle;
    }

    // this command cannot fire; CBasePlayer already chose ItemBusyFrame; The original TF weapon
    // code uses this bit only to abort a reload when its own reload gates allow it!
    g_cl.m_cmd->buttons |= IN_ATTACK;
    return true;
}
