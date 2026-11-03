#include "splash.hpp"
#include "runtime/thread_pool.hpp"


ATLAS_NO_DESTROY splash g_splash{ };


const splash_budget_t &splash::quality( )
{
    static constexpr splash_budget_t BUDGETS[]{
        { 24, 8, 3, 1, 0, 0, 0, false, false, 16, 16 },
        { 64, 16, 5, 4, 4, 1, 4, true, false, 32, 32 },
        { 96, 32, 5, 8, 8, 2, 8, true, true, 64, 64 },
    };
    static_assert( sizeof( BUDGETS ) / sizeof( BUDGETS[ 0 ] ) == config::SPLASH_QUALITY_COUNT );
    return BUDGETS[ math::smaller( g_cfg.m_ragebot.m_splash_quality, config::SPLASH_QUALITY_COUNT - 1 ) ];
}

void splash::begin( )
{
    m_budget = quality( );
    m_transforms.clear( );
}

ATLAS_FORCE_INLINE C_BaseEntity *entity_from_handle( IHandleEntity *handle )
{
    if ( g_tf2.m_static_prop_mgr->IsStaticProp( handle ) )
    {
        return nullptr;
    }
    IClientEntity *entity{ g_tf2.m_entity_list->GetClientEntityFromHandle( handle->GetRefEHandle( ) ) };
    return entity ? entity->GetBaseEntity( ) : nullptr;
}

ATLAS_FORCE_INLINE bool combat_item( C_BaseEntity *entity )
{
    switch ( entity->GetClientClass( )->GetClassID( ) )
    {
        case ETFClassID::CTFMedigunShield:
        case ETFClassID::CTFReviveMarker: return true;
        default: return false;
    }
}

ATLAS_FORCE_INLINE bool projectile_class( ETFClassID id )
{
    switch ( id )
    {
        case ETFClassID::CBaseGrenade:
        case ETFClassID::CBaseProjectile:
        case ETFClassID::CTFBaseRocket:
        case ETFClassID::CTFGrenadePipebombProjectile:
        case ETFClassID::CTFWeaponBaseGrenadeProj:
        case ETFClassID::CTFWeaponBaseMerasmusGrenade:
        case ETFClassID::CTFProjectile_Arrow:
        case ETFClassID::CTFProjectile_BallOfFire:
        case ETFClassID::CTFProjectile_Cleaver:
        case ETFClassID::CTFProjectile_EnergyBall:
        case ETFClassID::CTFProjectile_EnergyRing:
        case ETFClassID::CTFProjectile_Flare:
        case ETFClassID::CTFProjectile_GrapplingHook:
        case ETFClassID::CTFProjectile_HealingBolt:
        case ETFClassID::CTFProjectile_Jar:
        case ETFClassID::CTFProjectile_JarGas:
        case ETFClassID::CTFProjectile_JarMilk:
        case ETFClassID::CTFProjectile_MechanicalArmOrb:
        case ETFClassID::CTFProjectile_Rocket:
        case ETFClassID::CTFProjectile_SentryRocket:
        case ETFClassID::CTFProjectile_SpellBats:
        case ETFClassID::CTFProjectile_SpellFireball:
        case ETFClassID::CTFProjectile_SpellKartBats:
        case ETFClassID::CTFProjectile_SpellKartOrb:
        case ETFClassID::CTFProjectile_SpellLightningOrb:
        case ETFClassID::CTFProjectile_SpellMeteorShower:
        case ETFClassID::CTFProjectile_SpellMirv:
        case ETFClassID::CTFProjectile_SpellPumpkin:
        case ETFClassID::CTFProjectile_SpellSpawnBoss:
        case ETFClassID::CTFProjectile_SpellSpawnHorde:
        case ETFClassID::CTFProjectile_SpellSpawnZombie:
        case ETFClassID::CTFProjectile_SpellTransposeTeleport:
        case ETFClassID::CTFProjectile_Throwable:
        case ETFClassID::CTFProjectile_ThrowableBreadMonster:
        case ETFClassID::CTFProjectile_ThrowableBrick:
        case ETFClassID::CTFProjectile_ThrowableRepel: return true;
        default: return false;
    }
}

class CTraceFilterRadiusDamage : public CTraceFilterSimple
{
public:
    CTraceFilterRadiusDamage( ) :
        CTraceFilterSimple( nullptr, COLLISION_GROUP_PROJECTILE )
    {
    }

    bool ShouldHitEntity( IHandleEntity *handle, i32 mask ) override
    {
        C_BaseEntity *entity{ entity_from_handle( handle ) };
        if ( entity )
        {
            const ETFClassID id{ entity->GetClientClass( )->GetClassID( ) };
            if ( id == ETFClassID::CTFPlayer ||
                 ( m_ignore_projectiles && projectile_class( id ) ) ||
                 ( combat_item( entity ) && entity->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) ) )
            {
                return false;
            }
        }
        return CTraceFilterSimple::ShouldHitEntity( handle, mask );
    }

    bool m_ignore_projectiles{ true };
};

class splash_entities : public IEntityEnumerator
{
public:
    bool EnumEntity( IHandleEntity *entity ) override
    {
        if ( m_count == 256 )
        {
            m_overflow = true;
            return false;
        }
        m_handles[ m_count++ ] = entity;
        return true;
    }

    IHandleEntity *m_handles[ 256 ]{ };
    i32            m_count{ };
    bool           m_overflow{ };
};

vec3 splash::player_eye( C_TFPlayer *player, const vec3 &origin, i32 flags )
{
    const vec3 offset{ flags & FL_DUCKING ? TFGameRules( )->GetViewVectors( )->m_vDuckView * player->m_flModelScale( )
                                          : player->GetClassEyeHeight( ) };
    return origin + offset;
}

f32 splash::arm_time( C_TFWeaponBase *weapon )
{
    f32 time{ g_cl.attrib_hook( weapon, g_tf2.tf_grenadelauncher_livetime->GetFloat( ), ATLAS_XORSTR( "sticky_arm_time" ).get( ) ) };
    if ( g_cl.m_local->InCond( TF_COND_RUNE_HASTE ) )
    {
        time *= 0.5f;
    } else if ( g_cl.m_local->InCond( TF_COND_RUNE_KING ) || g_cl.m_local->InCond( TF_COND_KING_BUFFED ) )
    {
        time *= 0.75f;
    }
    return time;
}

f32 splash::air_radius( f32 radius, f32 age )
{
    const f32 base{ g_tf2.tf_grenadelauncher_livetime->GetFloat( ) };
    return radius * math::remap_clamped( age, base, base + g_tf2.tf_sticky_radius_ramp_time->GetFloat( ),
                                         g_tf2.tf_sticky_airdet_radius->GetFloat( ), 1.0f );
}

void splash::select_air_times( const vec3 *path, const vec3 *targets, i32 first, i32 last,
                               const vec3 &mins, const vec3 &maxs, f32 blast_radius, f32 edge_damage, splash_air_samples_t &out )
{
    out.m_count = 0;
    if ( first > last )
    {
        return;
    }
    const i32 samples{ math::smaller( quality( ).m_air_samples, last - first + 1 ) };
    for ( i32 bin{ }; bin < samples; ++bin )
    {
        const i32           begin{ first + ( last - first + 1 ) * bin / samples };
        const i32           through{ first + ( last - first + 1 ) * ( bin + 1 ) / samples };
        f32                 best{ -1.0f };
        splash_air_sample_t chosen{ };
        for ( i32 at{ begin }; at < through; ++at )
        {
            const f32             time{ TICKS_TO_TIME( at + 1 ) };
            const f32             radius{ air_radius( blast_radius, time ) };
            const splash_target_t target{ nullptr, targets[ at ], targets[ at ] + mins, targets[ at ] + maxs };
            if ( !grenade_near_target( path[ at ], radius, target ) )
            {
                continue;
            }
            const f32 score{ falloff( path[ at ], radius, edge_damage, target ) };
            if ( score > best )
            {
                best   = score;
                chosen = splash_air_sample_t{ path[ at ], time };
            }
        }
        if ( best >= 0.0f )
        {
            out.m_samples[ out.m_count++ ] = chosen;
        }
    }
}

class CTraceFilterGrenadeOrigin : public CTraceFilterSimple
{
public:
    CTraceFilterGrenadeOrigin( C_BaseEntity *projectile, const splash_target_t *target ) :
        CTraceFilterSimple( projectile, COLLISION_GROUP_NONE ), m_target{ target ? target->m_entity : nullptr }
    {
    }

    bool ShouldHitEntity( IHandleEntity *entity, i32 mask ) override
    {
        return entity != m_target && CTraceFilterSimple::ShouldHitEntity( entity, mask );
    }

    C_BaseEntity *m_target{ };
};

bool splash::grenade_origin( const vec3 &position, C_BaseEntity *projectile, vec3 &out, const splash_target_t *target )
{
    // @CTFWeaponBaseGrenadeProj::Detonate
    CTraceFilterGrenadeOrigin filter{ projectile, target };
    Ray_t                     ray{ };
    trace_t                   trace{ };
    const vec3                start{ position + vec3( 0.0f, 0.0f, 8.0f ) };
    ray.Init( start, start - vec3( 0.0f, 0.0f, 32.0f ) );
    g_tf2.m_trace->TraceRay( ray, MASK_SHOT_HULL, &filter, &trace );
    if ( trace.startsolid || trace.allsolid )
    {
        return false;
    }
    if ( target )
    {
        const vec3 end{ start - vec3( 0.0f, 0.0f, 32.0f ) };
        const f32  fraction{ math::segment_box_fraction( start, end, target->m_mins, target->m_maxs ) };
        if ( fraction < trace.fraction )
        {
            trace.endpos       = start + ( end - start ) * fraction;
            trace.plane.normal = fraction > 0.0f ? vec3( 0.0f, 0.0f, 1.0f ) : vec3( );
            trace.fraction     = fraction;
        }
    }
    out = trace.fraction < 1.0f ? trace.endpos + trace.plane.normal : position;
    return true;
}

bool splash::grenade_near_target( const vec3 &position, f32 radius, const splash_target_t &target )
{
    // z + 8 to z - 24
    const vec3 mins{ target.m_mins - vec3( 1.0f, 1.0f, 9.0f ) };
    const vec3 maxs{ target.m_maxs + vec3( 1.0f, 1.0f, 25.0f ) };
    return radius > 0.0f && position.dist_to_sqr( math::nearest_on_box( mins, maxs, position ) ) <= radius * radius;
}

i32 splash::clip_sticky_player( const vec3 &start, vec3 *path, i32 count, const vec3 *targets,
                                const vec3 &mins, const vec3 &maxs, impact_t &impact )
{
    for ( i32 at{ }; at < count; ++at )
    {
        const vec3 from{ at ? path[ at - 1 ] : start };
        const f32 duration{ at == count - 1 && impact.m_hit
                                ? math::clamp( impact.m_time / TICK_INTERVAL - static_cast< f32 >( at ), 0.0f, 1.0f ) : 1.0f };
        const vec3 target{ targets[ at ] + ( targets[ at + 1 ] - targets[ at ] ) * duration };
        const f32 fraction{ math::segment_box_fraction( from - targets[ at ], path[ at ] - target, mins, maxs ) };
        if ( fraction <= 1.0f )
        {
            path[ at ] = from + ( path[ at ] - from ) * fraction;
            impact = impact_t{ };
            impact.m_hit      = true;
            impact.m_position = path[ at ];
            impact.m_time     = TICKS_TO_TIME( at ) + TICK_INTERVAL * duration * fraction;
            return at + 1;
        }
    }
    return count;
}

bool splash::reaches( const vec3 &origin, f32 radius, const splash_target_t &target )
{
    const vec3 nearest{ math::nearest_on_box( target.m_mins, target.m_maxs, origin ) };
    const f32 distance{ origin.dist_to_sqr( nearest ) };
    if ( radius <= 0.0f || distance > radius * radius )
    {
        return false;
    }

    CTraceFilterRadiusDamage filter{ };
    Ray_t                    ray{ };
    trace_t                  trace{ };
    ray.Init( origin, target.m_eye );
    g_tf2.m_trace->TraceRay( ray, MASK_RADIUS_DAMAGE, &filter, &trace );

    // @::ApplyToEntity retries only the entity containing the explosion; the engine traces the world before it asks the
    // filter and stops on a world start solid, so a retry from inside the world returns the same trace
    if ( trace.startsolid && trace.m_pEnt && trace.m_pEnt->entindex( ) != 0 )
    {
        if ( combat_item( trace.m_pEnt ) && trace.m_pEnt != target.m_entity &&
             trace.m_pEnt->m_iTeamNum( ) == target.m_entity->m_iTeamNum( ) )
        {
            return false;
        }
        filter.SetPassEntity( trace.m_pEnt );
        filter.m_ignore_projectiles = false;
        g_tf2.m_trace->TraceRay( ray, MASK_RADIUS_DAMAGE, &filter, &trace );
    }
    return trace.fraction == 1.0f || trace.m_pEnt == target.m_entity;
}

f32 splash::falloff( const vec3 &origin, f32 radius, f32 edge_damage, const splash_target_t &target )
{
    const f32 distance{ math::smaller( origin.dist_to( target.m_origin ), origin.dist_to( ( target.m_mins + target.m_maxs ) * 0.5f ) ) };
    return math::remap_clamped( distance, 0.0f, radius, 1.0f, edge_damage );
}

f32 splash::damage( const vec3 &origin, f32 radius, f32 edge_damage, const splash_target_t &target )
{
    const f32 score{ falloff( origin, radius, edge_damage, target ) };
    if ( score <= 0.0f )
    {
        return 0.0f;
    }
    if ( !reaches( origin, radius, target ) )
    {
        return 0.0f;
    }
    return score;
}

bool splash::can_attach( const trace_t &trace )
{
    if ( !trace.m_pEnt || trace.startsolid || trace.allsolid || ( trace.surface.flags & SURF_SKY ) )
    {
        return false;
    }
    if ( trace.m_pEnt->entindex( ) == 0 )
    {
        return true;
    }
    return trace.m_pEnt->GetClientClass( )->GetClassID( ) == ETFClassID::CDynamicProp &&
           trace.m_pEnt->m_hMoveParent( ).Get( ) == nullptr && trace.m_pEnt->GetAbsVelocity( ).is_zero( );
}

bool splash::store_face( const vec3 *vertices, usize count, const vec3 &normal, geometry_t &out )
{
    const usize needed{ m_vertices.size( ) + count };
    if ( count < 3 ||
         ( needed > m_vertices.capacity( ) && !m_vertices.reserve( math::bigger( needed, m_vertices.capacity( ) * 2 ) ) ) ||
         ( needed > m_boundary.capacity( ) && !m_boundary.reserve( math::bigger( needed, m_boundary.capacity( ) * 2 ) ) ) ||
         ( needed > m_edges.capacity( ) && !m_edges.reserve( math::bigger( needed, m_edges.capacity( ) * 2 ) ) ) )
    {
        return false;
    }
    face_t *face{ m_faces.emplace_back( ) };
    if ( !face )
    {
        return false;
    }
    face->m_first  = m_vertices.size( );
    face->m_count  = count;
    face->m_normal = normal;
    face->m_mins = face->m_maxs = vertices[ 0 ];
    for ( usize at{ }; at < count; ++at )
    {
        m_vertices.push_back( vertices[ at ] );
        m_boundary.push_back( 1 );
        m_edges.push_back( edge_t{ m_faces.size( ) - 1 } );
        face->m_mins = math::lower( face->m_mins, vertices[ at ] );
        face->m_maxs = math::upper( face->m_maxs, vertices[ at ] );
    }
    ++out.m_face_count;
    return true;
}

void splash::link_edges( const geometry_t &geometry )
{
    const auto less{ []( const vec3 &a, const vec3 &b )
    {
        return a.x != b.x ? a.x < b.x : a.y != b.y ? a.y < b.y : a.z < b.z;
    } };
    m_edge_order.clear( );
    for ( usize at{ geometry.m_first_face }; at < geometry.m_first_face + geometry.m_face_count; ++at )
    {
        const face_t &face{ m_faces[ at ] };
        for ( usize edge{ face.m_first }; edge < face.m_first + face.m_count; ++edge )
        {
            const vec3 &a{ m_vertices[ edge ] }, &b{ m_vertices[ face.m_first + ( edge - face.m_first + 1 ) % face.m_count ] };
            const bool ordered{ less( a, b ) };
            if ( !m_edge_order.push_back( edge_key_t{ ordered ? a : b, ordered ? b : a, edge } ) )
            {
                return; // open edges still work without adjacency
            }
        }
    }
    const auto edge_less{ [ & ]( const edge_key_t &a, const edge_key_t &b )
    {
        return a.m_low == b.m_low ? less( a.m_high, b.m_high ) : less( a.m_low, b.m_low );
    } };

    const auto sift{ [ & ]( usize root, usize count )
    {
        for ( usize child{ root * 2 + 1 }; child < count; child = root * 2 + 1 )
        {
            if ( child + 1 < count && edge_less( m_edge_order[ child ], m_edge_order[ child + 1 ] ) )
            {
                ++child;
            }
            if ( !edge_less( m_edge_order[ root ], m_edge_order[ child ] ) )
            {
                break;
            }
            const edge_key_t value{ m_edge_order[ root ] };
            m_edge_order[ root ] = m_edge_order[ child ];
            m_edge_order[ child ] = value;
            root = child;
        }
    } };
    for ( usize root{ m_edge_order.size( ) / 2 }; root > 0; )
    {
        sift( --root, m_edge_order.size( ) );
    }
    for ( usize count{ m_edge_order.size( ) }; count > 1; )
    {
        const edge_key_t value{ m_edge_order[ 0 ] };
        m_edge_order[ 0 ] = m_edge_order[ --count ];
        m_edge_order[ count ] = value;
        sift( 0, count );
    }
    for ( usize at{ }; at < m_edge_order.size( ); )
    {
        usize end{ at + 1 };
        const usize first{ m_edge_order[ at ].m_index };
        while ( end < m_edge_order.size( ) && m_edge_order[ at ].m_low == m_edge_order[ end ].m_low &&
                m_edge_order[ at ].m_high == m_edge_order[ end ].m_high )
        {
            ++end;
        }
        if ( end - at == 2 )
        {
            const usize second{ m_edge_order[ at + 1 ].m_index };
            edge_t &a{ m_edges[ first ] }, &b{ m_edges[ second ] };
            const face_t &fa{ m_faces[ a.m_face ] }, &fb{ m_faces[ b.m_face ] };
            a.m_neighbor = b.m_face;
            b.m_neighbor = a.m_face;
            const f32 dot{ fa.m_normal.dot( fb.m_normal ) };
            vec3 center{ };
            for ( usize vertex{ fb.m_first }; vertex < fb.m_first + fb.m_count; ++vertex )
            {
                center += m_vertices[ vertex ];
            }
            center /= static_cast< f32 >( fb.m_count );
            a.m_kind = b.m_kind = dot > 0.99999f ? 1 : ( center - m_vertices[ first ] ).dot( fa.m_normal ) > 0.001f ? 3 : 2;
            if ( a.m_kind == 1 )
            {
                m_boundary[ first ] = m_boundary[ second ] = 0;
            }
        }
        at = end;
    }
}

void splash::index_faces( geometry_t &geometry )
{
    geometry.m_first_block = m_face_blocks.size( );
    geometry.m_block_count = 0;
    for ( usize first{ }; first < geometry.m_face_count; first += 64 )
    {
        const usize   end{ geometry.m_first_face + math::smaller( first + 64, geometry.m_face_count ) };
        const face_t &face{ m_faces[ geometry.m_first_face + first ] };
        face_block_t  block{ face.m_mins, face.m_maxs };
        for ( usize at{ geometry.m_first_face + first + 1 }; at < end; ++at )
        {
            block.m_mins = math::lower( block.m_mins, m_faces[ at ].m_mins );
            block.m_maxs = math::upper( block.m_maxs, m_faces[ at ].m_maxs );
        }
        if ( !m_face_blocks.push_back( block ) )
        {
            geometry.m_block_count = 0; // allocation failure keeps the unindexed scan available
            return;
        }
        ++geometry.m_block_count;
    }
}

bool splash::cache_convex( CPhysConvex *convex, geometry_t &out )
{
    CPolyhedron *polyhedron{ g_tf2.m_physics_collision->PolyhedronFromConvex( convex, true ) };
    if ( !polyhedron )
    {
        return false;
    }
    bool complete{ true };
    for ( u16 polygon{ }; polygon < polyhedron->m_polygon_count && complete; ++polygon )
    {
        const polyhedron_polygon_t &poly{ polyhedron->m_polygons[ polygon ] };
        if ( poly.m_count < 3 )
        {
            continue;
        }
        bool visited{ };
        for ( u16 at{ }; at < polygon; ++at )
        {
            visited |= polyhedron->m_polygons[ at ].m_normal.dot( poly.m_normal ) > 0.99999f;
        }
        if ( visited )
        {
            continue;
        }
        m_face_vertices.clear( );
        for ( u16 side{ polygon }; side < polyhedron->m_polygon_count && complete; ++side )
        {
            const polyhedron_polygon_t &coplanar{ polyhedron->m_polygons[ side ] };
            if ( coplanar.m_normal.dot( poly.m_normal ) < 0.99999f )
            {
                continue;
            }
            for ( u16 at{ }; at < coplanar.m_count; ++at )
            {
                const polyhedron_reference_t &reference{ polyhedron->m_references[ coplanar.m_first + at ] };
                const vec3                    point{ polyhedron->m_vertices[ polyhedron->m_lines[ reference.m_line ].m_points[ reference.m_endpoint ] ] };
                bool                          duplicate{ };
                for ( const vec3 &existing : m_face_vertices )
                {
                    duplicate |= point.dist_to_sqr( existing ) < 0.000001f;
                }
                if ( !duplicate && !m_face_vertices.push_back( point ) )
                {
                    complete = false;
                    break;
                }
            }
        }
        if ( !complete || m_face_vertices.size( ) < 3 )
        {
            complete = false;
            break;
        }
        vec3 centre{ };
        for ( const vec3 &point : m_face_vertices )
        {
            centre += point;
        }
        centre /= static_cast< f32 >( m_face_vertices.size( ) );
        const vec3 u{ ( m_face_vertices.front( ) - centre ).normalized( ) };
        const vec3 v{ u.cross( poly.m_normal ) };
        atl::sort( m_face_vertices.begin( ), m_face_vertices.end( ), [ & ]( const vec3 &a, const vec3 &b )
        {
            return math::atan2( v.dot( a - centre ), u.dot( a - centre ) ) <
                   math::atan2( v.dot( b - centre ), u.dot( b - centre ) );
        } );
        complete = this->store_face( m_face_vertices.data( ), m_face_vertices.size( ), poly.m_normal, out );
    }
    polyhedron->Release( );
    return complete;
}

bool splash::cache_mesh( const CPhysCollide *collide, geometry_t &out, const vec3 *cell_mins, const vec3 *cell_maxs )
{
    vec3     *vertices{ };
    const i32 count{ g_tf2.m_physics_collision->CreateDebugMesh( collide, &vertices ) };
    bool      complete{ count >= 0 && count % 3 == 0 };
    for ( i32 at{ }; at + 2 < count && complete; at += 3 )
    {
        const vec3 normal{ ( vertices[ at ] - vertices[ at + 2 ] ).cross( vertices[ at ] - vertices[ at + 1 ] ).normalized( ) };
        if ( !normal.is_zero( ) )
        {
            vec3 polygon[ 12 ]{ vertices[ at ], vertices[ at + 1 ], vertices[ at + 2 ] };
            i32  points{ 3 };
            if ( cell_mins )
            {
                // engine.dll selects whole triangles by vertex inclusion; clip the sampling faces to this cell
                const vec3 low{ math::lower( polygon[ 0 ], math::lower( polygon[ 1 ], polygon[ 2 ] ) ) };
                const vec3 high{ math::upper( polygon[ 0 ], math::upper( polygon[ 1 ], polygon[ 2 ] ) ) };
                for ( i32 axis{ }; axis < 3; ++axis )
                {
                    if ( high[ axis ] < ( *cell_mins )[ axis ] || low[ axis ] >= ( *cell_maxs )[ axis ] )
                    {
                        points = 0;
                        break;
                    }
                }
                for ( i32 plane{ }; plane < 6 && points; ++plane )
                {
                    vec3      clipped[ 12 ]{ };
                    i32       count{ };
                    const i32 axis{ plane / 2 };
                    for ( i32 point{ }; point < points; ++point )
                    {
                        const vec3 &a{ polygon[ ( point + points - 1 ) % points ] }, &b{ polygon[ point ] };
                        const f32   da{ plane & 1 ? ( *cell_maxs )[ axis ] - a[ axis ] : a[ axis ] - ( *cell_mins )[ axis ] };
                        const f32   db{ plane & 1 ? ( *cell_maxs )[ axis ] - b[ axis ] : b[ axis ] - ( *cell_mins )[ axis ] };
                        if ( ( da < 0.0f && db > 0.0f ) || ( da > 0.0f && db < 0.0f ) )
                        {
                            clipped[ count++ ] = a + ( b - a ) * ( da / ( da - db ) );
                        }
                        if ( db >= 0.0f )
                        {
                            clipped[ count++ ] = b;
                        }
                    }
                    Assert( count <= 12 );
                    points = count;
                    atl::memcpy( polygon, clipped, sizeof( vec3 ) * points );
                }
            }
            if ( points < 3 )
            {
                continue;
            }
            vec3 area{ };
            for ( i32 point{ 1 }; point + 1 < points; ++point )
            {
                area += ( polygon[ point ] - polygon[ 0 ] ).cross( polygon[ point + 1 ] - polygon[ 0 ] );
            }
            if ( area.length_sqr( ) < 0.00000001f )
            {
                continue;
            }
            complete = this->store_face( polygon, points, normal, out );
            if ( complete && cell_mins )
            {
                const face_t &face{ m_faces.back( ) };
                for ( i32 point{ }; point < points; ++point )
                {
                    bool original{ };
                    for ( i32 edge{ }; edge < 3; ++edge )
                    {
                        const vec3 &a{ vertices[ at + edge ] }, &b{ vertices[ at + ( edge + 1 ) % 3 ] };
                        original |= polygon[ point ].dist_to_sqr( math::closest_on_edge( polygon[ point ], a, b ) ) < 0.0001f &&
                                    polygon[ ( point + 1 ) % points ].dist_to_sqr( math::closest_on_edge( polygon[ ( point + 1 ) % points ], a, b ) ) < 0.0001f;
                    }
                    m_boundary[ face.m_first + point ] = original; // a cell cut is not a physical edge
                }
            }
        }
    }
    if ( vertices )
    {
        g_tf2.m_physics_collision->DestroyDebugMesh( count, vertices );
    }

    this->link_edges( out );
    this->index_faces( out );
    return complete;
}

splash::geometry_t *splash::brush( i32 index, projectile_work_t *work )
{
    if ( index < 0 || !m_brushes.resize( math::bigger( m_brushes.size( ), static_cast< usize >( index + 1 ) ) ) )
    {
        return nullptr;
    }
    geometry_t &geometry{ m_brushes[ index ] };
    if ( geometry.m_attempted )
    {
        return geometry.m_ready ? &geometry : nullptr;
    }
    if ( work && !work->prepare( ) )
    {
        return nullptr;
    }
    geometry.m_attempted = true;
    i32 contents{ };
    m_planes.m_Size = 0;
    if ( !g_tf2.m_trace->GetBrushInfo( index, &m_planes, &contents ) || !( contents & MASK_SOLID ) || m_planes.m_Size < 4 )
    {
        return nullptr;
    }
    CPhysConvex *convex{ g_tf2.m_physics_collision->ConvexFromPlanes( reinterpret_cast< f32 * >( m_planes.m_Memory.m_pMemory ), m_planes.m_Size, 0.0f ) };
    if ( !convex )
    {
        return nullptr;
    }
    geometry.m_first_face = m_faces.size( );
    geometry.m_face_count = 0;
    if ( !this->cache_convex( convex, geometry ) )
    {
        g_tf2.m_physics_collision->ConvexFree( convex );
        return nullptr;
    }
    this->link_edges( geometry );
    this->index_faces( geometry );
    geometry.m_owned = g_tf2.m_physics_collision->ConvertConvexToCollide( &convex, 1 );
    geometry.m_ready = geometry.m_owned != nullptr;
    if ( geometry.m_ready )
    {
        g_tf2.m_physics_collision->CollideGetAABB( &geometry.m_mins, &geometry.m_maxs, geometry.m_owned, vec3( ), vec3( ) );
    }
    return geometry.m_ready ? &geometry : nullptr;
}

bool splash::planes( i32 index, plane_range_t &out )
{
    if ( index < 0 || !m_plane_ranges.resize( math::bigger( m_plane_ranges.size( ), static_cast< usize >( index + 1 ) ) ) )
    {
        return false;
    }

    plane_range_t &range{ m_plane_ranges[ index ] };
    if ( range.m_attempted )
    {
        out = range;
        return range.m_ready;
    }

    range.m_attempted = true;
    m_planes.m_Size   = 0;
    if ( !g_tf2.m_trace->GetBrushInfo( index, &m_planes, &range.m_contents ) || m_planes.m_Size < 4 )
    {
        return false;
    }

    const usize needed{ m_brush_planes.size( ) + static_cast< usize >( m_planes.m_Size ) };
    if ( needed > m_brush_planes.capacity( ) && !m_brush_planes.reserve( math::bigger( needed, m_brush_planes.capacity( ) * 2 ) ) )
    {
        return false;
    }

    range.m_first = m_brush_planes.size( );
    range.m_count = static_cast< usize >( m_planes.m_Size );
    range.m_ready = true;
    for ( i32 at{ }; at < m_planes.m_Size; ++at )
    {
        m_brush_planes.push_back( m_planes.m_Memory.m_pMemory[ at ] );
    }

    out = range;
    return true;
}

constexpr i32 SEAM_CLIP_CAPACITY{ 64 };

ATLAS_FORCE_INLINE i32 clip_polygon( vec3 *polygon, i32 count, const vec3 &normal, f32 distance )
{
    vec3 kept[ SEAM_CLIP_CAPACITY ]{ };
    i32  out{ };
    for ( i32 at{ }; at < count; ++at )
    {
        if ( out + 2 > SEAM_CLIP_CAPACITY )
        {
            return 0;
        }

        const vec3 &a{ polygon[ ( at + count - 1 ) % count ] }, &b{ polygon[ at ] };
        const f32   da{ normal.dot( a ) - distance }, db{ normal.dot( b ) - distance };
        if ( ( da < 0.0f && db > 0.0f ) || ( da > 0.0f && db < 0.0f ) )
        {
            kept[ out++ ] = a + ( b - a ) * ( da / ( da - db ) );
        }
        if ( db <= 0.0f )
        {
            kept[ out++ ] = b;
        }
    }
    atl::memcpy( polygon, kept, sizeof( vec3 ) * static_cast< usize >( out ) );
    return out;
}

ATLAS_FORCE_INLINE vec3 nearest_in_polygon( const vec3 &point, const vec3 *vertices, usize count, const vec3 &normal )
{
    const vec3 projected{ point - normal * ( point - vertices[ 0 ] ).dot( normal ) };
    vec3       nearest{ projected };
    f32        distance{ FLT_MAX }, sign{ };
    bool       inside{ true };
    for ( usize at{ }; at < count; ++at )
    {
        const vec3 &a{ vertices[ at ] }, &b{ vertices[ ( at + 1 ) % count ] };
        const f32   side{ ( b - a ).cross( projected - a ).dot( normal ) };
        if ( side != 0.0f )
        {
            inside &= sign == 0.0f || side * sign > 0.0f;
            sign = side;
        }
        const vec3 edge{ math::closest_on_edge( projected, a, b ) };
        const f32  gap{ edge.dist_to_sqr( projected ) };
        if ( gap < distance )
        {
            nearest  = edge;
            distance = gap;
        }
    }
    return inside ? projected : nearest;
}

ATLAS_FORCE_INLINE void polygon_span( const vec3 &point, const vec3 &direction, const vec3 *vertices, usize count, const vec3 &normal,
                                      f32 &low, f32 &high )
{
    vec3 area{ };
    for ( usize at{ 1 }; at + 1 < count; ++at )
    {
        area += ( vertices[ at ] - vertices[ 0 ] ).cross( vertices[ at + 1 ] - vertices[ 0 ] );
    }
    const f32 winding{ area.dot( normal ) < 0.0f ? -1.0f : 1.0f };

    low  = -FLT_MAX;
    high = FLT_MAX;
    for ( usize at{ }; at < count; ++at )
    {
        const vec3 &a{ vertices[ at ] }, &b{ vertices[ ( at + 1 ) % count ] };
        const f32   inset{ ( b - a ).cross( point - a ).dot( normal ) * winding };
        const f32   toward{ ( b - a ).cross( direction ).dot( normal ) * winding };
        if ( toward > 0.0f )
        {
            low = math::bigger( low, -inset / toward );
        } else if ( toward < 0.0f )
        {
            high = math::smaller( high, -inset / toward );
        }
    }
}

constexpr f32 NEAR_PUSH{ 1.0f };
constexpr f32 FAR_PUSH{ 1.0f + static_cast< f32 >( DIST_EPSILON ) };

ATLAS_FORCE_INLINE bool world_solid_at( const vec3 &point, u32 mask )
{
    CTraceFilterWorldOnly filter{ };
    Ray_t                 ray{ };
    trace_t               trace{ };
    ray.Init( point, point + vec3{ 0.0f, 0.0f, static_cast< f32 >( DIST_EPSILON ) } );
    g_tf2.m_trace->TraceRay( ray, mask, &filter, &trace );
    return trace.startsolid;
}

bool splash::buried( const vec3 &explosion )
{
    return world_solid_at( explosion, MASK_RADIUS_DAMAGE );
}

constexpr f32 SWEEP_EPSILON{ 1.0f / 32.0f };

constexpr i32 WINDOW_SAMPLES{ 32 };
constexpr i32 ARC_SAMPLES{ 16 };
constexpr i32 SKIN_DEPTHS{ 8 };

bool splash::rounded_explosion( const seam_point_t &seam, const vec3 &end, const vec3 &direction, f32 step, vec3 &explosion )
{
    const vec3 offset{ end - seam.m_edge };
    const f32  along{ offset.dot( seam.m_slide ) };
    const vec3 across{ offset - seam.m_slide * along };
    const f32  gap{ across.length( ) };
    if ( along < seam.m_low || along > seam.m_high || gap <= 0.0f || gap >= SWEEP_EPSILON )
    {
        return false;
    }

    const vec3 normal{ across / gap };
    const vec3 turn{ seam.m_arc[ 0 ].cross( seam.m_arc[ 1 ] ) };
    const f32  closing{ -normal.dot( direction ) * step };
    if ( seam.m_arc[ 0 ].cross( normal ).dot( turn ) < 0.0f || normal.cross( seam.m_arc[ 1 ] ).dot( turn ) < 0.0f ||
         !( closing > SWEEP_EPSILON * 0.1f || ( closing > 0.0001f && gap < SWEEP_EPSILON * 0.9f ) ) )
    {
        return false;
    }

    const f32 back{ ( SWEEP_EPSILON - gap ) * step / closing };
    if ( back > step )
    {
        return false;
    }
    explosion = end - direction * back + normal;
    return true;
}

bool splash::rounded_window( const seam_point_t &seam, const vec3 &start, const vec3 &direction, f32 step, f32 &low, f32 &high )
{
    const vec3 offset{ start - seam.m_edge };
    const vec3 across{ offset - seam.m_slide * offset.dot( seam.m_slide ) };
    const vec3 drift{ direction - seam.m_slide * direction.dot( seam.m_slide ) };
    const f32  rate{ drift.length_sqr( ) };
    if ( rate <= 0.0f )
    {
        return false;
    }

    const f32  closest{ -across.dot( drift ) / rate };
    const vec3 nearest{ across + drift * closest };
    const f32  spread{ ( SWEEP_EPSILON * SWEEP_EPSILON - nearest.length_sqr( ) ) / rate };
    if ( spread <= 0.0f )
    {
        return false;
    }

    const f32 entry{ closest - math::sqrt( spread ) };
    const f32 width{ ( closest - entry ) / static_cast< f32 >( WINDOW_SAMPLES ) };
    i32       first{ -1 }, last{ -1 };
    for ( i32 at{ }; at < WINDOW_SAMPLES && ( first < 0 || last == at - 1 ); ++at )
    {
        vec3      explosion{ };
        const f32 distance{ entry + width * ( static_cast< f32 >( at ) + 0.5f ) };
        if ( rounded_explosion( seam, start + direction * distance, direction, step, explosion ) &&
             seam.m_wall.dot( explosion ) < seam.m_wall_distance )
        {
            first = first < 0 ? at : first;
            last  = at;
        }
    }
    if ( first < 0 )
    {
        return false;
    }

    low  = entry + width * static_cast< f32 >( first );
    high = entry + width * static_cast< f32 >( last + 1 );
    return true;
}

void splash::build_seams( i32 index )
{
    const vec3 reach{ FAR_PUSH, FAR_PUSH, FAR_PUSH };
    m_neighbour_ids.m_Size = 0;
    g_tf2.m_trace->GetBrushesInAABB( m_brushes[ index ].m_mins - reach, m_brushes[ index ].m_maxs + reach, &m_neighbour_ids,
                                     MASK_SOLID | MASK_RADIUS_DAMAGE );
    this->add_seam_regions( m_brushes[ index ], index, false );
}

void splash::build_prop_seams( prop_t &prop )
{
    geometry_t &world{ prop.m_world };
    world.m_seams_built = true;
    world.m_first_face  = m_faces.size( );
    world.m_face_count  = 0;
    const geometry_t *local{ this->model( prop.m_model, prop.m_collision ) };
    if ( !local )
    {
        return;
    }

    const matrix3x4 &transform{ prop.m_collideable->CollisionToWorldTransform( ) };
    const vec3       origin{ math::transform_point( vec3{ }, transform ) };
    for ( usize at{ local->m_first_face }; at < local->m_first_face + local->m_face_count; ++at )
    {
        const usize first{ m_faces[ at ].m_first }, count{ m_faces[ at ].m_count };
        const vec3  normal{ ( math::transform_point( m_faces[ at ].m_normal, transform ) - origin ).normalized( ) };
        if ( count > static_cast< usize >( SEAM_CLIP_CAPACITY ) )
        {
            continue;
        }
        vec3 polygon[ SEAM_CLIP_CAPACITY ]{ };
        for ( usize vertex{ }; vertex < count; ++vertex )
        {
            polygon[ vertex ] = math::transform_point( m_vertices[ first + vertex ], transform );
        }
        if ( !this->store_face( polygon, count, normal, world ) )
        {
            return;
        }
    }
    this->link_edges( world );

    const vec3 reach{ FAR_PUSH, FAR_PUSH, FAR_PUSH };
    m_neighbour_ids.m_Size = 0;
    g_tf2.m_trace->GetBrushesInAABB( prop.m_mins - reach, prop.m_maxs + reach, &m_neighbour_ids, MASK_SOLID | MASK_RADIUS_DAMAGE );
    this->add_seam_regions( world, -1, false );
    this->add_rounded_regions( world );
}

void splash::build_cell_seams( geometry_t &geometry, const vec3 &mins, const vec3 &maxs )
{
    const vec3 reach{ NEAR_PUSH, NEAR_PUSH, NEAR_PUSH };
    m_neighbour_ids.m_Size = 0;
    g_tf2.m_trace->GetBrushesInAABB( mins - reach, maxs + reach, &m_neighbour_ids, MASK_SOLID | MASK_RADIUS_DAMAGE );
    this->add_seam_regions( geometry, -1, true );
}

void splash::add_seam_regions( geometry_t &geometry, i32 owner, bool displacement )
{
    plane_range_t scratch{ };
    for ( i32 at{ }; at < m_neighbour_ids.m_Size; ++at )
    {
        this->planes( m_neighbour_ids.m_Memory.m_pMemory[ at ], scratch );
    }

    geometry.m_seams_built = true;
    geometry.m_first_seam  = m_seam_regions.size( );
    geometry.m_seam_count  = 0;

    const auto covered{ [ & ]( const vec3 &point )
    {
        for ( i32 at{ }; at < m_neighbour_ids.m_Size; ++at )
        {
            plane_range_t solid{ };
            const i32     other{ m_neighbour_ids.m_Memory.m_pMemory[ at ] };
            if ( other == owner || !this->planes( other, solid ) || !( solid.m_contents & MASK_SOLID ) )
            {
                continue;
            }
            bool inside{ true };
            for ( usize plane{ solid.m_first }; plane < solid.m_first + solid.m_count && inside; ++plane )
            {
                inside = m_brush_planes[ plane ].m_normal.dot( point ) < m_brush_planes[ plane ].m_distance;
            }
            if ( inside )
            {
                return true;
            }
        }
        return false;
    } };

    for ( i32 neighbour{ }; neighbour < m_neighbour_ids.m_Size; ++neighbour )
    {
        const i32     other{ m_neighbour_ids.m_Memory.m_pMemory[ neighbour ] };
        plane_range_t wall{ };
        if ( other == owner || !this->planes( other, wall ) || !( wall.m_contents & MASK_RADIUS_DAMAGE ) )
        {
            continue;
        }

        for ( usize face_index{ geometry.m_first_face }; face_index < geometry.m_first_face + geometry.m_face_count; ++face_index )
        {
            const face_t &face{ m_faces[ face_index ] };
            if ( face.m_count > static_cast< usize >( SEAM_CLIP_CAPACITY ) )
            {
                continue;
            }

            for ( usize binding{ wall.m_first }; binding < wall.m_first + wall.m_count; ++binding )
            {
                const brush_plane_t &crossed{ m_brush_planes[ binding ] };
                const f32            against{ crossed.m_normal.dot( face.m_normal ) };
                if ( against >= 0.0f || against < -0.99999f )
                {
                    continue;
                }

                vec3 polygon[ SEAM_CLIP_CAPACITY ]{ };
                atl::memcpy( polygon, m_vertices.data( ) + face.m_first, sizeof( vec3 ) * face.m_count );

                i32 count{ clip_polygon( polygon, static_cast< i32 >( face.m_count ), -crossed.m_normal, -crossed.m_distance ) };
                for ( usize side{ wall.m_first }; side < wall.m_first + wall.m_count && count >= 3; ++side )
                {
                    const brush_plane_t &plane{ m_brush_planes[ side ] };
                    const f32            toward{ plane.m_normal.dot( face.m_normal ) };
                    const f32            push{ toward > 0.0f && !displacement ? FAR_PUSH : NEAR_PUSH };
                    count = clip_polygon( polygon, count, plane.m_normal, plane.m_distance - push * toward );
                }
                if ( count < 3 )
                {
                    continue;
                }

                vec3 area{ }, centre{ };
                for ( i32 at{ }; at < count; ++at )
                {
                    centre += polygon[ at ];
                    if ( at >= 1 && at + 1 < count )
                    {
                        area += ( polygon[ at ] - polygon[ 0 ] ).cross( polygon[ at + 1 ] - polygon[ 0 ] );
                    }
                }
                centre /= static_cast< f32 >( count );

                const usize needed{ m_seam_vertices.size( ) + static_cast< usize >( count ) };
                if ( area.length_sqr( ) < 0.00000001f ||
                     covered( centre + face.m_normal * static_cast< f32 >( DIST_EPSILON ) ) ||
                     ( needed > m_seam_vertices.capacity( ) && !m_seam_vertices.reserve( math::bigger( needed, m_seam_vertices.capacity( ) * 2 ) ) ) )
                {
                    continue;
                }

                seam_region_t *region{ m_seam_regions.emplace_back( ) };
                if ( !region )
                {
                    return;
                }
                region->m_first         = m_seam_vertices.size( );
                region->m_count         = static_cast< usize >( count );
                region->m_normal        = face.m_normal;
                region->m_wall          = crossed.m_normal;
                region->m_wall_distance = crossed.m_distance;
                region->m_displacement  = displacement;
                for ( i32 at{ }; at < count; ++at )
                {
                    m_seam_vertices.push_back( polygon[ at ] );
                }
                ++geometry.m_seam_count;
            }
        }
    }
}

void splash::add_rounded_regions( geometry_t &geometry )
{
    for ( usize face_index{ geometry.m_first_face }; face_index < geometry.m_first_face + geometry.m_face_count; ++face_index )
    {
        const face_t &face{ m_faces[ face_index ] };
        for ( usize at{ }; at < face.m_count; ++at )
        {
            const edge_t &edge{ m_edges[ face.m_first + at ] };
            if ( edge.m_kind != 2 || edge.m_neighbor < face_index )
            {
                continue;
            }

            const vec3 &a{ m_vertices[ face.m_first + at ] }, &b{ m_vertices[ face.m_first + ( at + 1 ) % face.m_count ] };
            const f32   length{ a.dist_to( b ) };
            const vec3  slide{ ( b - a ) / length };
            const vec3 &normal{ face.m_normal }, &other{ m_faces[ edge.m_neighbor ].m_normal };
            const f32   cone{ math::acos( normal.dot( other ) ) };
            const vec3  turn{ ( other - normal * normal.dot( other ) ).normalized( ) };
            const auto  turned{ [ & ]( f32 sample )
            {
                const f32 angle{ cone * sample / static_cast< f32 >( ARC_SAMPLES ) };
                return normal * math::cos( angle ) + turn * math::sin( angle );
            } };

            for ( i32 neighbour{ }; neighbour < m_neighbour_ids.m_Size; ++neighbour )
            {
                plane_range_t wall{ };
                if ( !this->planes( m_neighbour_ids.m_Memory.m_pMemory[ neighbour ], wall ) || !( wall.m_contents & MASK_RADIUS_DAMAGE ) )
                {
                    continue;
                }

                i32 first{ }, count{ }, run{ };
                f32 low{ }, high{ }, run_low{ }, run_high{ };
                for ( i32 sample{ }; sample < ARC_SAMPLES; ++sample )
                {
                    const vec3 push{ turned( static_cast< f32 >( sample ) + 0.5f ) * ( 1.0f + SWEEP_EPSILON ) };
                    f32        from{ 0.0f }, to{ length };
                    for ( usize plane{ wall.m_first }; plane < wall.m_first + wall.m_count && from < to; ++plane )
                    {
                        const brush_plane_t &side{ m_brush_planes[ plane ] };
                        const f32            rate{ side.m_normal.dot( slide ) };
                        const f32            depth{ side.m_normal.dot( a + push ) - side.m_distance };
                        if ( rate > 0.0f )
                        {
                            to = math::smaller( to, -depth / rate );
                        } else if ( rate < 0.0f )
                        {
                            from = math::bigger( from, -depth / rate );
                        } else if ( depth >= 0.0f )
                        {
                            to = from;
                        }
                    }

                    if ( from >= to )
                    {
                        run = 0;
                        continue;
                    }
                    if ( run && math::bigger( from, run_low ) < math::smaller( to, run_high ) )
                    {
                        run_low  = math::bigger( from, run_low );
                        run_high = math::smaller( to, run_high );
                        ++run;
                    } else
                    {
                        run      = 1;
                        run_low  = from;
                        run_high = to;
                    }
                    if ( run > count )
                    {
                        first = sample - run + 1;
                        count = run;
                        low   = run_low;
                        high  = run_high;
                    }
                }
                if ( !count )
                {
                    continue;
                }

                const vec3    middle{ a + slide * ( ( low + high ) * 0.5f ) };
                const vec3    push{ turned( static_cast< f32 >( first ) + static_cast< f32 >( count ) * 0.5f ) };
                brush_plane_t crossed{ };
                f32           entry{ -1.0f };
                for ( usize plane{ wall.m_first }; plane < wall.m_first + wall.m_count; ++plane )
                {
                    const brush_plane_t &side{ m_brush_planes[ plane ] };
                    const f32            outside{ side.m_normal.dot( middle ) - side.m_distance };
                    const f32            into{ -side.m_normal.dot( push ) };
                    if ( outside > 0.0f && into > 0.0f && outside / into > entry )
                    {
                        entry   = outside / into;
                        crossed = side;
                    }
                }

                const usize needed{ m_seam_vertices.size( ) + 2 };
                if ( entry < 0.0f ||
                     ( needed > m_seam_vertices.capacity( ) && !m_seam_vertices.reserve( math::bigger( needed, m_seam_vertices.capacity( ) * 2 ) ) ) )
                {
                    continue;
                }

                seam_region_t *region{ m_seam_regions.emplace_back( ) };
                if ( !region )
                {
                    return;
                }
                region->m_first         = m_seam_vertices.size( );
                region->m_count         = 2;
                region->m_normal        = push;
                region->m_wall          = crossed.m_normal;
                region->m_wall_distance = crossed.m_distance;
                region->m_rounded       = true;
                region->m_arc[ 0 ]      = turned( static_cast< f32 >( first ) );
                region->m_arc[ 1 ]      = turned( static_cast< f32 >( first + count ) );
                m_seam_vertices.push_back( a + slide * low );
                m_seam_vertices.push_back( a + slide * high );
                ++geometry.m_seam_count;
            }
        }
    }
}

i32 splash::seams( const splash_target_t &target, const vec3 &shoot_pos, f32 radius, f32 step, seam_point_t *out, i32 capacity )
{
    const vec3 centre{ ( target.m_mins + target.m_maxs ) * 0.5f };
    const f32  reach{ radius + FAR_PUSH };
    const vec3 extent{ reach, reach, reach };
    i32        count{ };

    const auto slot{ [ & ]( const vec3 &point, const vec3 &normal, f32 distance, bool blocked )
    {
        i32  insert{ count };
        bool duplicate{ };
        for ( i32 prior{ }; prior < count; ++prior )
        {
            duplicate |= out[ prior ].m_position.dist_to_sqr( point ) < FAR_PUSH * FAR_PUSH && out[ prior ].m_normal.dot( normal ) > 0.99999f;
            if ( insert == count && ( blocked < out[ prior ].m_blocked || ( blocked == out[ prior ].m_blocked && distance < out[ prior ].m_distance ) ) )
            {
                insert = prior;
            }
        }
        return duplicate ? capacity : insert;
    } };

    const auto ranked{ [ & ]( const vec3 &point, const vec3 &normal, f32 distance, const vec3 &aim, bool &blocked )
    {
        const i32 insert{ slot( point, normal, distance, false ) };
        if ( insert == capacity )
        {
            return insert;
        }

        CTraceFilterWorldOnly filter{ };
        Ray_t                 ray{ };
        trace_t               trace{ };
        ray.Init( aim, shoot_pos );
        g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );
        blocked = trace.startsolid || trace.fraction < 1.0f;
        return blocked ? slot( point, normal, distance, true ) : insert;
    } };
    const auto store{ [ & ]( i32 insert, const seam_point_t &seam )
    {
        count = math::smaller( count + 1, capacity );
        for ( i32 move{ count - 1 }; move > insert; --move )
        {
            out[ move ] = out[ move - 1 ];
        }
        out[ insert ] = seam;
    } };

    const auto consider{ [ & ]( const seam_region_t &region )
    {
        const vec3 *polygon{ m_seam_vertices.data( ) + region.m_first };
        if ( ( shoot_pos - polygon[ 0 ] ).dot( region.m_normal ) <= 0.0f )
        {
            return;
        }

        vec3       point{ nearest_in_polygon( centre, polygon, region.m_count, region.m_normal ) };
        const f32  along{ region.m_wall.dot( region.m_normal ) };
        const vec3 across{ region.m_wall - region.m_normal * along };
        if ( const f32 slope{ across.length_sqr( ) }; slope > 0.0f )
        {
            point += across * ( ( region.m_wall_distance - along * NEAR_PUSH * 0.5f - region.m_wall.dot( point ) ) / slope );
            point = nearest_in_polygon( point, polygon, region.m_count, region.m_normal );
        }

        const vec3 blast{ point + region.m_normal };
        if ( blast.dist_to_sqr( math::nearest_on_box( target.m_mins, target.m_maxs, blast ) ) > radius * radius )
        {
            return;
        }

        const f32  distance{ blast.dist_to_sqr( centre ) };
        const vec3 aim{ point + region.m_normal * ( region.m_displacement ? 0.0f : static_cast< f32 >( DIST_EPSILON ) ) };
        bool       blocked{ };
        const i32  insert{ ranked( point, region.m_normal, distance, aim, blocked ) };
        if ( insert == capacity )
        {
            return;
        }

        const f32 push{ region.m_displacement ? NEAR_PUSH : FAR_PUSH };
        if ( !splash::reaches( point + region.m_normal * push, radius, target ) ||
             world_solid_at( point + region.m_normal * static_cast< f32 >( DIST_EPSILON ), MASK_SOLID ) ||
             !world_solid_at( point + region.m_normal * NEAR_PUSH, MASK_RADIUS_DAMAGE ) ||
             ( !region.m_displacement && !world_solid_at( point + region.m_normal * FAR_PUSH, MASK_RADIUS_DAMAGE ) ) )
        {
            return;
        }

        const vec3 slide{ region.m_wall.cross( region.m_normal ).normalized( ) };
        f32        low{ }, high{ };
        polygon_span( point, slide, polygon, region.m_count, region.m_normal, low, high );
        seam_point_t seam{ point, region.m_normal, distance, region.m_wall, region.m_wall_distance, slide, low, high, region.m_displacement };
        seam.m_blocked = blocked;
        store( insert, seam );
    } };

    const auto consider_rounded{ [ & ]( const seam_region_t &region )
    {
        const vec3 *span{ m_seam_vertices.data( ) + region.m_first };
        const vec3  point{ math::closest_on_edge( centre, span[ 0 ], span[ 1 ] ) };
        if ( point.dist_to_sqr( math::nearest_on_box( target.m_mins, target.m_maxs, point ) ) > reach * reach ||
             ( ( shoot_pos - point ).dot( region.m_arc[ 0 ] ) <= 0.0f && ( shoot_pos - point ).dot( region.m_arc[ 1 ] ) <= 0.0f ) )
        {
            return;
        }

        seam_point_t base{ };
        base.m_wall          = region.m_wall;
        base.m_wall_distance = region.m_wall_distance;
        base.m_slide         = ( span[ 1 ] - span[ 0 ] ).normalized( );
        base.m_rounded       = true;
        base.m_arc[ 0 ]      = region.m_arc[ 0 ];
        base.m_arc[ 1 ]      = region.m_arc[ 1 ];

        const f32  length{ span[ 0 ].dist_to( span[ 1 ] ) };
        const vec3 toward{ ( ( span[ 0 ] + span[ 1 ] ) * 0.5f - shoot_pos ).normalized( ) };
        const f32  run{ toward.dot( base.m_slide ) };
        const f32  drift{ ( toward - base.m_slide * run ).length( ) };
        if ( drift <= 0.0f )
        {
            return;
        }
        const f32 inset{ math::smaller( SWEEP_EPSILON * math::abs( run ) / drift, length ) };
        const f32 first{ run > 0.0f ? inset : 0.0f }, last{ run > 0.0f ? length : length - inset };
        const f32 candidates[]{ math::clamp( ( centre - span[ 0 ] ).dot( base.m_slide ), first, last ), first, last };

        seam_point_t seam{ };
        vec3         direction{ };
        i32          kind{ };
        const auto   pass{ [ & ]( f32 along )
        {
            seam_point_t trial{ base };
            trial.m_edge = span[ 0 ] + base.m_slide * along;
            trial.m_low  = -along;
            trial.m_high = length - along;

            const vec3 side{ base.m_slide.cross( trial.m_edge - shoot_pos ).normalized( ) };
            const vec3 outward{ side.dot( region.m_normal ) < 0.0f ? -side : side };
            vec3       best{ };
            f32        low{ }, high{ };
            for ( i32 depth{ 1 }; depth < SKIN_DEPTHS; ++depth )
            {
                const vec3 passing{ trial.m_edge + outward * ( SWEEP_EPSILON * static_cast< f32 >( depth ) / static_cast< f32 >( SKIN_DEPTHS ) ) };
                const vec3 heading{ ( passing - shoot_pos ).normalized( ) };
                f32        from{ }, to{ };
                if ( splash::rounded_window( trial, shoot_pos, heading, step, from, to ) && to - from > high - low )
                {
                    best = heading;
                    low  = from;
                    high = to;
                }
            }
            if ( high <= low )
            {
                return;
            }

            CTraceFilterWorldOnly filter{ };
            Ray_t                 ray{ };
            trace_t               world{ };
            vec3                  explosion{ };
            ray.Init( shoot_pos, shoot_pos + best * ( high + step ) );
            g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &world );
            const f32 wall{ world.fraction * ( high + step ) };
            const i32 found{ world.fraction < 1.0f && splash::rounded_explosion( trial, world.endpos, best, step, explosion ) &&
                                     trial.m_wall.dot( explosion ) < trial.m_wall_distance
                                 ? 2
                                 : 1 };
            if ( world.startsolid || ( found == 1 && wall < low ) || found <= kind )
            {
                return;
            }
            trial.m_position = found == 2 ? world.endpos : shoot_pos + best * ( ( low + math::smaller( high, wall ) ) * 0.5f );
            seam             = trial;
            direction        = best;
            kind             = found;
        } };
        for ( const f32 along : candidates )
        {
            pass( along );
        }

        vec3 blast{ };
        if ( !kind || !splash::rounded_explosion( seam, seam.m_position, direction, step, blast ) ||
             blast.dist_to_sqr( math::nearest_on_box( target.m_mins, target.m_maxs, blast ) ) > radius * radius )
        {
            return;
        }

        const vec3 offset{ seam.m_position - seam.m_edge };
        const f32  along{ offset.dot( seam.m_slide ) };
        seam.m_edge += seam.m_slide * along;
        seam.m_low -= along;
        seam.m_high -= along;
        seam.m_normal   = ( offset - seam.m_slide * along ).normalized( );
        seam.m_distance = blast.dist_to_sqr( centre );
        const i32 insert{ ranked( seam.m_position, seam.m_normal, seam.m_distance, seam.m_position, seam.m_blocked ) };
        if ( insert == capacity || !splash::reaches( blast, radius, target ) || world_solid_at( seam.m_position, MASK_SOLID ) ||
             !world_solid_at( blast, MASK_RADIUS_DAMAGE ) )
        {
            return;
        }
        store( insert, seam );
    } };

    m_brush_ids.m_Size = 0;
    g_tf2.m_trace->GetBrushesInAABB( target.m_mins - extent, target.m_maxs + extent, &m_brush_ids, MASK_SOLID );
    for ( i32 at{ }; at < m_brush_ids.m_Size; ++at )
    {
        const i32   index{ m_brush_ids.m_Memory.m_pMemory[ at ] };
        geometry_t *geometry{ this->brush( index ) };
        if ( !geometry )
        {
            continue;
        }
        if ( !geometry->m_seams_built )
        {
            this->build_seams( index );
        }
        for ( usize seam{ geometry->m_first_seam }; seam < geometry->m_first_seam + geometry->m_seam_count; ++seam )
        {
            consider( m_seam_regions[ seam ] );
        }
    }

    struct box_t
    {
        i32  m_contents{ };
        vec3 m_mins{ }, m_maxs{ };
    };
    box_t boxes[ 64 ]{ };
    i32   box_count{ };
    for ( i32 at{ }; at < m_brush_ids.m_Size && box_count < 64; ++at )
    {
        plane_range_t range{ };
        if ( !this->planes( m_brush_ids.m_Memory.m_pMemory[ at ], range ) || range.m_count != 6 )
        {
            continue;
        }
        box_t box{ range.m_contents };
        u32   sides{ };
        for ( usize plane{ range.m_first }; plane < range.m_first + range.m_count; ++plane )
        {
            const brush_plane_t &side{ m_brush_planes[ plane ] };
            for ( usize axis{ }; axis < 3; ++axis )
            {
                if ( side.m_normal[ axis ] == 1.0f )
                {
                    box.m_maxs[ axis ] = side.m_distance;
                    sides |= 1u << ( axis * 2 + 1 );
                } else if ( side.m_normal[ axis ] == -1.0f )
                {
                    box.m_mins[ axis ] = -side.m_distance;
                    sides |= 1u << ( axis * 2 );
                }
            }
        }
        if ( sides == 0x3Fu )
        {
            boxes[ box_count++ ] = box;
        }
    }

    const auto consider_plane{ [ & ]( const vec3 &end, const vec3 &normal, const vec3 &wall, f32 wall_distance, f32 coordinate, const box_t &wall_box, f32 low,
                                      f32 high, bool flush, bool diagonal )
    {
        const f32 magnitude{ math::abs( coordinate ) };
        const f32 half_step{ ( __builtin_bit_cast( f32, __builtin_bit_cast( u32, magnitude ) + 1u ) - magnitude ) * 0.5f };
        const f32 front{ wall.dot( shoot_pos ) - wall_distance };
        if ( front <= 0.0f || front * static_cast< f32 >( DIST_EPSILON ) > end.dist_to( shoot_pos ) * half_step || ( shoot_pos - end ).dot( normal ) <= 0.0f )
        {
            return;
        }

        const vec3 blast{ end + normal };
        if ( blast.dist_to_sqr( math::nearest_on_box( target.m_mins, target.m_maxs, blast ) ) > radius * radius )
        {
            return;
        }

        const f32 distance{ blast.dist_to_sqr( centre ) };
        bool      blocked{ };
        const i32 insert{ ranked( end, normal, distance, end + wall * ( half_step * 2.0f ), blocked ) };
        if ( insert == capacity || !splash::reaches( blast, radius, target ) )
        {
            return;
        }
        seam_point_t seam{ end, normal, distance, wall, wall_distance, { }, low, high };
        seam.m_plane     = true;
        seam.m_flush     = flush;
        seam.m_diagonal  = diagonal;
        seam.m_blocked   = blocked;
        seam.m_wall_mins = wall_box.m_mins;
        seam.m_wall_maxs = wall_box.m_maxs;
        store( insert, seam );
    } };

    for ( i32 wall_at{ }; wall_at < box_count; ++wall_at )
    {
        const box_t &wall_box{ boxes[ wall_at ] };
        if ( !( wall_box.m_contents & MASK_RADIUS_DAMAGE ) )
        {
            continue;
        }

        for ( usize a{ }; a < 3; ++a )
        {
            for ( i32 side{ }; side < 2; ++side )
            {
                const f32 coordinate{ side ? wall_box.m_maxs[ a ] : wall_box.m_mins[ a ] };
                vec3      wall{ };
                wall[ a ] = side ? 1.0f : -1.0f;
                for ( i32 other_at{ }; other_at < box_count; ++other_at )
                {
                    const box_t &other{ boxes[ other_at ] };
                    const bool   diagonal{ ( side ? other.m_mins[ a ] : other.m_maxs[ a ] ) == coordinate };
                    if ( other_at == wall_at || !( other.m_contents & MASK_SOLID ) ||
                         ( ( side ? other.m_maxs[ a ] : other.m_mins[ a ] ) != coordinate && !diagonal ) )
                    {
                        continue;
                    }

                    for ( usize b{ }; b < 3; ++b )
                    {
                        if ( b == a )
                        {
                            continue;
                        }

                        const usize c{ 3 - a - b };
                        for ( i32 above{ }; above < 2; ++above )
                        {
                            const f32 shared{ above ? wall_box.m_maxs[ b ] : wall_box.m_mins[ b ] };
                            const f32 inward{ above ? -1.0f : 1.0f };
                            const f32 hit_face{ above ? other.m_mins[ b ] : other.m_maxs[ b ] };
                            const f32 burst{ hit_face + inward * ( 1.0f + static_cast< f32 >( DIST_EPSILON ) ) };
                            const f32 low{ math::bigger( wall_box.m_mins[ c ], other.m_mins[ c ] ) };
                            const f32 high{ math::smaller( wall_box.m_maxs[ c ], other.m_maxs[ c ] ) };
                            if ( low >= high || ( hit_face != shared && ( !diagonal || burst < wall_box.m_mins[ b ] || burst > wall_box.m_maxs[ b ] ) ) )
                            {
                                continue;
                            }

                            vec3 end{ }, normal{ };
                            end[ a ]    = coordinate;
                            end[ b ]    = hit_face + inward * static_cast< f32 >( DIST_EPSILON );
                            end[ c ]    = math::clamp( centre[ c ], low, high );
                            normal[ b ] = inward;
                            if ( hit_face != shared )
                            {
                                const f32 run{ ( shoot_pos[ b ] - hit_face ) * inward };
                                for ( i32 upper{ }; upper < 2 && run > 0.0f; ++upper )
                                {
                                    const f32 edge{ upper ? high : low };
                                    const f32 rise{ ( upper ? edge - shoot_pos[ c ] : shoot_pos[ c ] - edge ) / run };
                                    if ( edge != ( upper ? wall_box.m_maxs[ c ] : wall_box.m_mins[ c ] ) || rise <= 0.0f )
                                    {
                                        continue;
                                    }
                                    end[ c ] = edge + ( upper ? -2.0f : 2.0f ) * static_cast< f32 >( DIST_EPSILON ) * rise;
                                    consider_plane( end, normal, wall, side ? coordinate : -coordinate, coordinate, wall_box, 0.0f, 0.0f, false, true );
                                }
                                continue;
                            }

                            consider_plane( end, normal, wall, side ? coordinate : -coordinate, coordinate, wall_box, low - end[ c ], high - end[ c ], !diagonal,
                                            diagonal );
                            if ( diagonal )
                            {
                                continue;
                            }

                            for ( i32 far{ }; far < 2; ++far )
                            {
                                const f32 face{ far ? other.m_maxs[ c ] : other.m_mins[ c ] };
                                const f32 out{ far ? 1.0f : -1.0f };
                                if ( face + out * FAR_PUSH <= wall_box.m_mins[ c ] || face + out * FAR_PUSH >= wall_box.m_maxs[ c ] )
                                {
                                    continue;
                                }
                                vec3 corner{ }, outward{ };
                                corner[ a ]  = coordinate;
                                corner[ b ]  = shared + inward * static_cast< f32 >( DIST_EPSILON ) * 0.5f;
                                corner[ c ]  = face + out * static_cast< f32 >( DIST_EPSILON );
                                outward[ c ] = out;
                                consider_plane( corner, outward, wall, side ? coordinate : -coordinate, coordinate, wall_box, 0.0f, 0.0f, false, false );
                            }
                        }
                    }
                }
            }
        }
    }

    const vec3 mins{ target.m_mins - extent }, maxs{ target.m_maxs + extent };
    for ( i32 x{ math::cell_axis( mins.x ) }; x <= math::cell_axis( maxs.x ); ++x )
    {
        for ( i32 y{ math::cell_axis( mins.y ) }; y <= math::cell_axis( maxs.y ); ++y )
        {
            for ( i32 z{ math::cell_axis( mins.z ) }; z <= math::cell_axis( maxs.z ); ++z )
            {
                cell_t *cell{ this->cell( x, y, z ) };
                if ( !cell )
                {
                    continue;
                }
                geometry_t &geometry{ cell->m_geometry };
                const vec3  cell_mins{ x * 256.0f, y * 256.0f, z * 256.0f };
                const vec3  cell_maxs{ cell_mins + vec3( 256.0f, 256.0f, 256.0f ) };
                if ( geometry.m_owned && !geometry.m_sampled )
                {
                    geometry.m_first_face = m_faces.size( );
                    geometry.m_face_count = 0;
                    geometry.m_sampled    = this->cache_mesh( geometry.m_owned, geometry, &cell_mins, &cell_maxs );
                }
                if ( geometry.m_sampled )
                {
                    if ( !geometry.m_seams_built )
                    {
                        this->build_cell_seams( geometry, cell_mins, cell_maxs );
                    }
                    for ( usize seam{ geometry.m_first_seam }; seam < geometry.m_first_seam + geometry.m_seam_count; ++seam )
                    {
                        consider( m_seam_regions[ seam ] );
                    }
                }

                for ( prop_t &prop : cell->m_props )
                {
                    if ( !math::overlaps( prop.m_mins, prop.m_maxs, mins, maxs ) || x != math::cell_axis( math::bigger( prop.m_mins.x, mins.x ) ) ||
                         y != math::cell_axis( math::bigger( prop.m_mins.y, mins.y ) ) || z != math::cell_axis( math::bigger( prop.m_mins.z, mins.z ) ) )
                    {
                        continue;
                    }
                    if ( !prop.m_world.m_seams_built )
                    {
                        this->build_prop_seams( prop );
                    }
                    for ( usize seam{ prop.m_world.m_first_seam }; seam < prop.m_world.m_first_seam + prop.m_world.m_seam_count; ++seam )
                    {
                        if ( m_seam_regions[ seam ].m_rounded )
                        {
                            consider_rounded( m_seam_regions[ seam ] );
                        } else
                        {
                            consider( m_seam_regions[ seam ] );
                        }
                    }
                }
            }
        }
    }
    return count;
}

splash::geometry_t *splash::model( const model_t *model, const vcollide_t *collision, projectile_work_t *work )
{
    model_cache_t *cached{ };
    for ( model_cache_t &entry : m_models )
    {
        if ( entry.m_model == model )
        {
            cached = &entry;
            break;
        }
    }
    if ( !cached )
    {
        cached = m_models.emplace_back( );
        if ( !cached )
        {
            return nullptr;
        }
        cached->m_model = model;
    }
    geometry_t &geometry{ cached->m_geometry };
    if ( geometry.m_attempted && geometry.m_source == collision &&
         geometry.m_solid_count == collision->solidCount && geometry.m_first_solid == collision->solids[ 0 ] )
    {
        return geometry.m_ready ? &geometry : nullptr;
    }
    if ( work && !work->prepare( ) )
    {
        return nullptr;
    }
    geometry.m_first_face  = m_faces.size( );
    geometry.m_face_count  = 0;
    geometry.m_ready       = false;
    geometry.m_attempted   = true;
    geometry.m_source      = collision;
    geometry.m_first_solid = collision->solids[ 0 ];
    geometry.m_solid_count = collision->solidCount;
    CPhysConvex *convexes[ 1024 ]{ };
    for ( i32 solid{ }; solid < collision->solidCount; ++solid )
    {
        const i32 count{ g_tf2.m_physics_collision->GetConvexesUsedInCollideable( collision->solids[ solid ], convexes, 1024 ) };
        if ( count == 1024 )
        {
            return nullptr; // the query may have truncated a model; don't cache a partial shape
        }
        for ( i32 at{ }; at < count; ++at )
        {
            if ( !this->cache_convex( convexes[ at ], geometry ) )
            {
                return nullptr;
            }
        }
    }
    this->link_edges( geometry );
    this->index_faces( geometry );
    geometry.m_ready = true;
    return &geometry;
}

splash::cell_t *splash::cell( i32 x, i32 y, i32 z, projectile_work_t *work )
{
    const u64 key{ ( static_cast< u64 >( static_cast< u16 >( x ) ) << 32 ) |
                   ( static_cast< u64 >( static_cast< u16 >( y ) ) << 16 ) | static_cast< u16 >( z ) };
    usize     low{ }, high{ m_cells.size( ) };
    while ( low < high )
    {
        const usize middle{ ( low + high ) / 2 };
        if ( m_cells[ middle ].m_key < key )
        {
            low = middle + 1;
        } else
        {
            high = middle;
        }
    }
    if ( low == m_cells.size( ) || m_cells[ low ].m_key != key )
    {
        cell_t *added{ m_cells.emplace_back( ) };
        if ( !added )
        {
            return nullptr;
        }
        added->m_key = key;
        for ( usize at{ m_cells.size( ) - 1 }; at > low; --at )
        {
            cell_t temporary{ detail::move( m_cells[ at - 1 ] ) };
            m_cells[ at - 1 ] = detail::move( m_cells[ at ] );
            m_cells[ at ]     = detail::move( temporary );
        }
    }
    geometry_t &geometry{ m_cells[ low ].m_geometry };
    if ( !geometry.m_attempted )
    {
        if ( work && !work->prepare( ) )
        {
            return nullptr;
        }
        geometry.m_attempted  = true;
        geometry.m_first_face = m_faces.size( );
        geometry.m_face_count = 0;
        const vec3 mins{ x * 256.0f, y * 256.0f, z * 256.0f };
        const vec3 maxs{ mins + vec3( 256.0f, 256.0f, 256.0f ) };
        geometry.m_owned = g_tf2.m_trace->GetCollidableFromDisplacementsInAABB( mins, maxs );
        geometry.m_ready = true; // empty cells are cached too
    }
    if ( !m_cells[ low ].m_props_ready )
    {
        if ( work && !work->prepare( ) )
        {
            return nullptr;
        }
        const vec3 mins{ x * 256.0f, y * 256.0f, z * 256.0f };
        const vec3 maxs{ mins + vec3( 256.0f, 256.0f, 256.0f ) };
        m_cells[ low ].m_props.clear( );
        m_prop_query.m_Size = 0;
        g_tf2.m_static_prop_mgr->GetAllStaticPropsInAABB( mins, maxs, &m_prop_query );
        for ( i32 at{ }; at < m_prop_query.m_Size; ++at )
        {
            ICollideable     *collideable{ m_prop_query.m_Memory.m_pMemory[ at ] };
            const model_t    *model{ collideable->GetCollisionModel( ) };
            const vcollide_t *collision{ model ? g_tf2.m_model_info->GetVCollide( model ) : nullptr };
            if ( collideable->GetSolid( ) == SOLID_NONE || ( collideable->GetSolidFlags( ) & FSOLID_NOT_SOLID ) ||
                 !collision || !collision->solidCount )
            {
                continue;
            }
            prop_t prop{ collideable, model, collision, vec3( FLT_MAX, FLT_MAX, FLT_MAX ), vec3( -FLT_MAX, -FLT_MAX, -FLT_MAX ) };
            for ( i32 solid{ }; solid < collision->solidCount; ++solid )
            {
                vec3 solid_mins{ }, solid_maxs{ };
                g_tf2.m_physics_collision->CollideGetAABB( &solid_mins, &solid_maxs, collision->solids[ solid ],
                                                           collideable->GetCollisionOrigin( ), collideable->GetCollisionAngles( ) );
                prop.m_mins = math::lower( prop.m_mins, solid_mins );
                prop.m_maxs = math::upper( prop.m_maxs, solid_maxs );
            }
            if ( !m_cells[ low ].m_props.push_back( prop ) )
            {
                return nullptr;
            }
        }
        m_cells[ low ].m_props_ready = true;
    }
    return geometry.m_ready ? &m_cells[ low ] : nullptr;
}

void splash::add_point( const vec3 &point, const vec3 &normal, const splash_target_t &target, f32 radius, splash_points_t &out,
                        usize face, i32 transform, bool edge, usize edge_index )
{
    const vec3 blast{ point + normal };
    if ( blast.dist_to_sqr( math::nearest_on_box( target.m_mins, target.m_maxs, blast ) ) > ( radius + out.m_margin - 1.0f ) * ( radius + out.m_margin - 1.0f ) )
    {
        return;
    }
    const f32       distance{ math::smaller( blast.dist_to_sqr( target.m_origin ), blast.dist_to_sqr( ( target.m_mins + target.m_maxs ) * 0.5f ) ) };
    i32            &count{ out.m_count };
    const i32       limit{ out.m_limit };
    splash_point_t *points{ out.m_points };
    const auto retain_seam{ [ & ]( const splash_point_t &candidate )
    {
        if ( !candidate.m_edge || !out.m_seam_limit )
        {
            return;
        }
        const f32 priority{ out.m_seams_from_shooter ? candidate.m_position.dist_to_sqr( out.m_shoot_pos ) : candidate.m_distance };
        i32 insert{ out.m_seam_count };
        for ( i32 at{ }; at < out.m_seam_count; ++at )
        {
            splash_point_t &other{ out.m_seams[ at ] };
            if ( candidate.m_position.dist_to_sqr( other.m_position ) < out.m_spacing * out.m_spacing &&
                 ( out.m_seams_from_shooter || candidate.m_normal.dot( other.m_normal ) > 0.99999f ) )
            {
                if ( out.m_seams_from_shooter && candidate.m_position == other.m_position )
                {
                    const vec3 from_shooter{ out.m_shoot_pos - candidate.m_position };
                    const f32 facing{ candidate.m_normal.dot( from_shooter ) };
                    const f32 other_facing{ other.m_normal.dot( from_shooter ) };
                    const bool tied_normal{ candidate.m_normal.x != other.m_normal.x
                                                ? candidate.m_normal.x > other.m_normal.x
                                                : candidate.m_normal.y != other.m_normal.y
                                                      ? candidate.m_normal.y > other.m_normal.y
                                                      : candidate.m_normal.z > other.m_normal.z };
                    if ( facing > other_facing || ( facing == other_facing && tied_normal ) )
                    {
                        other = candidate;
                    }
                }
                return;
            }
            const f32 other_priority{ out.m_seams_from_shooter ? other.m_position.dist_to_sqr( out.m_shoot_pos ) : other.m_distance };
            if ( insert == out.m_seam_count && priority < other_priority )
            {
                insert = at;
            }
        }
        if ( insert >= out.m_seam_limit )
        {
            return;
        }
        out.m_seam_count = math::smaller( out.m_seam_count + 1, out.m_seam_limit );
        for ( i32 at{ out.m_seam_count - 1 }; at > insert; --at )
        {
            out.m_seams[ at ] = out.m_seams[ at - 1 ];
        }
        out.m_seams[ insert ] = candidate;
    } };
    if ( count == limit && distance >= points[ count - 1 ].m_distance )
    {
        splash_point_t candidate{ point, normal, distance, face, transform };
        candidate.m_edge = edge;
        candidate.m_edge_index = edge_index;
        retain_seam( candidate );
        return;
    }
    i32 insert{ count };
    for ( i32 at{ }; at < count; ++at )
    {
        const splash_point_t &other{ points[ at ] };
        if ( point.dist_to_sqr( other.m_position ) < out.m_spacing * out.m_spacing && normal.dot( other.m_normal ) > 0.99999f )
        {
            return;
        }
        if ( insert == count && distance < other.m_distance )
        {
            insert = at;
        }
    }
    if ( insert == limit )
    {
        return;
    }
    if ( count == limit )
    {
        retain_seam( points[ count - 1 ] );
    }
    count = math::smaller( count + 1, limit );
    for ( i32 at{ count - 1 }; at > insert; --at )
    {
        points[ at ] = points[ at - 1 ];
    }
    points[ insert ] = splash_point_t{ point, normal, distance, face, transform };
    points[ insert ].m_edge = edge;
    points[ insert ].m_edge_index = edge_index;
}

void splash::local_bounds( const splash_target_t &target, const matrix3x4 *transform, vec3 &mins, vec3 &maxs )
{
    mins = target.m_mins;
    maxs = target.m_maxs;
    if ( !transform )
    {
        return;
    }
    mins = vec3( FLT_MAX, FLT_MAX, FLT_MAX );
    maxs = -mins;
    for ( i32 corner{ }; corner < 8; ++corner )
    {
        vec3 point{ corner & 1 ? target.m_maxs.x : target.m_mins.x,
                    corner & 2 ? target.m_maxs.y : target.m_mins.y,
                    corner & 4 ? target.m_maxs.z : target.m_mins.z };
        math::vector_itransform( point, *transform, point );
        mins = math::lower( mins, point );
        maxs = math::upper( maxs, point );
    }
}

void splash::sample( const geometry_t &geometry, const matrix3x4 *transform, const splash_target_t &target,
                     const vec3 &shoot_pos, f32 radius, bool arcing, splash_points_t &out, i32 transform_index,
                     splash_point_t *raw, usize *raw_count ) const
{
    const auto emit{ [ & ]( const vec3 &point, const vec3 &normal, usize face, bool edge = false, usize edge_index = static_cast< usize >( -1 ) )
    {
        if ( raw )
        {
            raw[ ( *raw_count )++ ] = splash_point_t{ point, normal, 0.0f, face, transform_index };
            raw[ *raw_count - 1 ].m_edge = edge;
            raw[ *raw_count - 1 ].m_edge_index = edge_index;
        } else
        {
            add_point( point, normal, target, radius, out, face, transform_index, edge, edge_index );
        }
    } };
    const vec3 expansion{ radius + out.m_margin, radius + out.m_margin, radius + out.m_margin };
    const vec3 low{ target.m_mins - expansion }, high{ target.m_maxs + expansion };
    vec3       local_goals[]{ target.m_origin, ( target.m_mins + target.m_maxs ) * 0.5f, shoot_pos };
    if ( out.m_sticky )
    {
        local_goals[ 1 ] = target.m_eye; // BodyTarget(false), rather than another distance-only seed
    }
    vec3       target_low{ target.m_mins }, target_high{ target.m_maxs };
    if ( transform )
    {
        for ( vec3 &goal : local_goals )
        {
            math::vector_itransform( goal, *transform, goal );
        }
    }
    local_bounds( target, transform, target_low, target_high );
    for ( usize face_index{ geometry.m_first_face }; face_index < geometry.m_first_face + geometry.m_face_count; ++face_index )
    {
        const face_t &face{ m_faces[ face_index ] };
        const vec3   *vertices{ m_vertices.data( ) + face.m_first };
        const vec3   *goals{ local_goals };
        if ( !transform && !math::overlaps( face.m_mins, face.m_maxs, low, high ) )
        {
            continue;
        }
        const vec3 gap{ math::bigger( 0.0f, math::bigger( face.m_mins.x - target_high.x, target_low.x - face.m_maxs.x ) ),
                        math::bigger( 0.0f, math::bigger( face.m_mins.y - target_high.y, target_low.y - face.m_maxs.y ) ),
                        math::bigger( 0.0f, math::bigger( face.m_mins.z - target_high.z, target_low.z - face.m_maxs.z ) ) };
        if ( gap.length_sqr( ) > ( radius + out.m_margin ) * ( radius + out.m_margin ) )
        {
            continue;
        }
        vec3 normal{ face.m_normal };
        if ( transform )
        {
            const matrix3x4 &matrix{ *transform };
            normal = vec3( matrix[ 0 ][ 0 ] * normal.x + matrix[ 0 ][ 1 ] * normal.y + matrix[ 0 ][ 2 ] * normal.z,
                           matrix[ 1 ][ 0 ] * normal.x + matrix[ 1 ][ 1 ] * normal.y + matrix[ 1 ][ 2 ] * normal.z,
                           matrix[ 2 ][ 0 ] * normal.x + matrix[ 2 ][ 1 ] * normal.y + matrix[ 2 ][ 2 ] * normal.z );
        }
        const bool front{ arcing || ( goals[ 2 ] - vertices[ 0 ] ).dot( face.m_normal ) > 0.0f };
        if ( !front )
        {
            continue;
        }
        vec3 anchor{ };
        for ( i32 goal_index{ }; goal_index < 3; ++goal_index )
        {
            if ( goal_index == 1 && !m_budget.m_vertices && !out.m_sticky )
            {
                continue;
            }
            const vec3 &goal{ goals[ goal_index ] };
            const vec3  projected{ goal - face.m_normal * ( goal - vertices[ 0 ] ).dot( face.m_normal ) };
            vec3        nearest{ };
            f32         distance{ FLT_MAX }, sign{ };
            bool        inside{ true };
            for ( usize at{ }; at < face.m_count; ++at )
            {
                const vec3 &a{ vertices[ at ] };
                const vec3 &b{ vertices[ ( at + 1 ) % face.m_count ] };
                const f32   side{ ( b - a ).cross( projected - a ).dot( face.m_normal ) };
                if ( math::abs( side ) > 0.0001f )
                {
                    inside &= sign == 0.0f || side * sign >= 0.0f;
                    sign = side;
                }
                vec3      edge{ math::closest_on_edge( projected, a, b ) };
                const f32 gap{ edge.dist_to_sqr( projected ) };
                if ( gap < distance )
                {
                    nearest  = edge;
                    distance = gap;
                }
                if ( ( goal_index == 0 || ( goal_index == 1 && out.m_sticky ) ) && m_boundary[ face.m_first + at ] )
                {
                    const usize edge_index{ face.m_first + at };
                    vec3 vertex{ a };
                    if ( transform )
                    {
                        edge   = math::transform_point( edge, *transform );
                        vertex = math::transform_point( vertex, *transform );
                    }
                    emit( edge, normal, face_index, true, edge_index );
                    if ( goal_index == 0 && edge_index < m_edges.size( ) &&
                         m_edges[ edge_index ].m_kind == 2 &&
                         m_edges[ edge_index ].m_neighbor < m_faces.size( ) )
                    {
                        vec3 seam_normal{ face.m_normal + m_faces[ m_edges[ edge_index ].m_neighbor ].m_normal };
                        if ( seam_normal.normalize( ) > 0.001f )
                        {
                            if ( transform )
                            {
                                const matrix3x4 &matrix{ *transform };
                                seam_normal = vec3( matrix[ 0 ][ 0 ] * seam_normal.x + matrix[ 0 ][ 1 ] * seam_normal.y + matrix[ 0 ][ 2 ] * seam_normal.z,
                                                    matrix[ 1 ][ 0 ] * seam_normal.x + matrix[ 1 ][ 1 ] * seam_normal.y + matrix[ 1 ][ 2 ] * seam_normal.z,
                                                    matrix[ 2 ][ 0 ] * seam_normal.x + matrix[ 2 ][ 1 ] * seam_normal.y + matrix[ 2 ][ 2 ] * seam_normal.z );
                                seam_normal.normalize( );
                            }
                            emit( edge, seam_normal, face_index, true, edge_index );
                        }
                    }
                    if ( goal_index == 0 && m_budget.m_vertices && m_boundary[ face.m_first + ( at + face.m_count - 1 ) % face.m_count ] )
                    {
                        emit( vertex, normal, face_index, true, edge_index );
                    }
                }
            }
            vec3 point{ inside ? projected : nearest };
            if ( transform )
            {
                point = math::transform_point( point, *transform );
            }
            if ( goal_index == 0 )
            {
                anchor = point;
            } else if ( goal_index == 2 )
            {
                const vec3 along{ point - anchor };
                const f32  length{ along.length( ) };
                if ( length > radius * 0.5f )
                {
                    point = anchor + along * ( radius * 0.5f / length );
                }
            }
            emit( point, normal, face_index );
        }
    }
}

const vec3 *splash::directions( i32 count )
{
    if ( count != m_direction_count )
    {
        m_direction_count = count;
        for ( i32 at{ }; at < count; ++at )
        {
            const f32 z{ 1.0f - 2.0f * ( static_cast< f32 >( at ) + 0.5f ) / static_cast< f32 >( count ) };
            const f32 r{ math::sqrt( 1.0f - z * z ) };
            const f32 angle{ 2.39996323f * static_cast< f32 >( at ) };
            m_directions[ at ] = vec3( math::cos( angle ) * r, math::sin( angle ) * r, z ).normalized( );
        }
    }
    return m_directions;
}

void splash::sphere( const splash_target_t &target, f32 radius, i32 command, splash_points_t &out, bool interleave )
{

    const i32              count{ m_budget.m_sphere_points };
    const vec3            *samples{ this->directions( count ) };
    const vec3             center{ ( target.m_mins + target.m_maxs ) * 0.5f };
    const f32              reach{ radius + out.m_margin + ( target.m_maxs - target.m_mins ).length( ) * 0.5f };
    const f32              yaw{ static_cast< f32 >( command & 1023 ) * 2.39996323f };
    const f32              pitch{ static_cast< f32 >( command & 1023 ) * 1.41421356f };
    const f32              cy{ math::cos( yaw ) }, sy{ math::sin( yaw ) }, cp{ math::cos( pitch ) }, sp{ math::sin( pitch ) };
    CTraceFilterProjectile filter{ };
    filter.m_pSkipTarget = target.m_entity;
    i32 retained{ }, air{ };
    for ( i32 at{ }; at < count && out.m_count < splash_points_t::CAPACITY; ++at )
    {
        const vec3 &sample{ samples[ ( at * 13 ) % count ] };
        const vec3  tilted{ sample.x * cp + sample.z * sp, sample.y, sample.z * cp - sample.x * sp };
        const vec3  direction{ tilted.x * cy - tilted.y * sy, tilted.x * sy + tilted.y * cy, tilted.z };
        Ray_t       ray{ };
        trace_t     trace{ };
        ray.Init( center, center + direction.normalized( ) * reach );
        g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );
        if ( trace.startsolid || trace.allsolid || ( trace.surface.flags & SURF_SKY ) )
        {
            continue;
        }
        const auto emit{ [ & ]( splash_point_t point )
        {
            const vec3 blast{ point.m_position + point.m_normal };
            if ( out.m_count == splash_points_t::CAPACITY )
            {
                return;
            }
            if ( blast.dist_to_sqr( math::nearest_on_box( target.m_mins, target.m_maxs, blast ) ) >
                 ( radius + out.m_margin - 1.0f ) * ( radius + out.m_margin - 1.0f ) )
            {
                return;
            }
            for ( i32 prior{ }; prior < out.m_count; ++prior )
            {
                const splash_point_t &other{ out.m_points[ prior ] };
                if ( point.m_air == other.m_air && point.m_position.dist_to_sqr( other.m_position ) < out.m_spacing * out.m_spacing &&
                     ( point.m_air || point.m_normal.dot( other.m_normal ) > 0.99999f ) )
                {
                    return;
                }
            }
            point.m_distance = math::smaller( blast.dist_to_sqr( target.m_origin ), blast.dist_to_sqr( center ) );
            const i32 insert{ interleave ? math::smaller( retained * 2 + 1, out.m_count ) : out.m_count };
            for ( i32 move{ out.m_count }; move > insert; --move )
            {
                out.m_points[ move ] = out.m_points[ move - 1 ];
            }
            out.m_points[ insert ] = point;
            ++out.m_count;
            ++retained;
            air += point.m_air;
        } };
        if ( trace.fraction < 1.0f )
        {
            emit( splash_point_t{ trace.endpos, trace.plane.normal } );
        }
        if ( air < out.m_air_limit && trace.fraction * reach > radius * 0.5f + out.m_spacing )
        {
            splash_point_t point{ };
            point.m_position = center + direction.normalized( ) * ( radius * 0.5f );
            point.m_air      = true;
            emit( point );
        }
    }

}

vec3 splash::project( const face_t &face, const vec3 &point ) const
{
    const vec3 *vertices{ m_vertices.data( ) + face.m_first };
    vec3        best{ };
    f32         distance{ FLT_MAX };
    for ( usize at{ 1 }; at + 1 < face.m_count; ++at )
    {
        const vec3 projected{ math::closest_on_triangle( point, vertices[ 0 ], vertices[ at ], vertices[ at + 1 ] ) };
        const f32  gap{ projected.dist_to_sqr( point ) };
        if ( gap < distance )
        {
            best     = projected;
            distance = gap;
        }
    }
    return best;
}

void splash::refine( const splash_point_t &seed, const splash_target_t &target, const vec3 &shoot_pos,
                     f32 radius, f32 step, i32 count, splash_points_t &out ) const
{
    if ( seed.m_face >= m_faces.size( ) )
    {
        return;
    }
    const face_t    &face{ m_faces[ seed.m_face ] };
    const matrix3x4 *transform{ seed.m_transform >= 0 ? &m_transforms[ seed.m_transform ].m_matrix : nullptr };
    if ( seed.m_edge_index < m_edges.size( ) )
    {
        const usize edge{ seed.m_edge_index };
        const vec3 &a{ m_vertices[ edge ] }, &b{ m_vertices[ face.m_first + ( edge - face.m_first + 1 ) % face.m_count ] };
        vec3 local_seed{ seed.m_position };
        if ( transform )
        {
            math::vector_itransform( local_seed, *transform, local_seed );
        }
        const vec3 direction{ ( b - a ).normalized( ) };
        for ( i32 at{ }; at < count; ++at )
        {
            const bool adjacent{ at % 4 == 3 && m_edges[ edge ].m_neighbor < m_faces.size( ) };
            const usize face_index{ adjacent ? m_edges[ edge ].m_neighbor : seed.m_face };
            const face_t &incident{ m_faces[ face_index ] };
            vec3 point{ local_seed };
            if ( at % 4 < 2 )
            {
                const f32 distance{ at < 4 ? step : step * 0.5f };
                point = math::closest_on_edge( local_seed + direction * ( at % 2 ? -distance : distance ), a, b );
            } else
            {
                vec3 center{ };
                for ( usize vertex{ incident.m_first }; vertex < incident.m_first + incident.m_count; ++vertex )
                {
                    center += m_vertices[ vertex ];
                }
                center /= static_cast< f32 >( incident.m_count );
                const vec3 inward{ ( center - local_seed ).normalized( ) };
                // the collision epsilon is 1/32; exact shared endpoints do not identify a unique impact face;
                point = this->project( incident, local_seed + inward * ( at < 4 ? 0.0625f : step * 0.5f ) );
            }
            vec3 normal{ incident.m_normal };
            if ( transform )
            {
                point = math::transform_point( point, *transform );
                const matrix3x4 &matrix{ *transform };
                normal = vec3( matrix[ 0 ][ 0 ] * normal.x + matrix[ 0 ][ 1 ] * normal.y + matrix[ 0 ][ 2 ] * normal.z,
                               matrix[ 1 ][ 0 ] * normal.x + matrix[ 1 ][ 1 ] * normal.y + matrix[ 1 ][ 2 ] * normal.z,
                               matrix[ 2 ][ 0 ] * normal.x + matrix[ 2 ][ 1 ] * normal.y + matrix[ 2 ][ 2 ] * normal.z );
            }
            add_point( point, normal, target, radius, out, face_index, seed.m_transform, true,
                       adjacent ? static_cast< usize >( -1 ) : edge );
        }
    } else
    {
        vec3             u{ shoot_pos - seed.m_position };
        u -= seed.m_normal * u.dot( seed.m_normal );
        if ( u.normalize( ) < 0.001f )
        {
            u = seed.m_normal.cross( math::abs( seed.m_normal.z ) < 0.9f ? vec3( 0, 0, 1 ) : vec3( 1, 0, 0 ) ).normalized( );
        }
        const vec3 v{ seed.m_normal.cross( u ) };
        for ( i32 at{ }; at < count; ++at )
        {
            const f32 angle{ 2.39996323f * static_cast< f32 >( at ) };
            const f32 distance{ step * math::sqrt( ( static_cast< f32 >( at ) + 0.5f ) / static_cast< f32 >( count ) ) };
            vec3      point{ seed.m_position + ( u * math::cos( angle ) + v * math::sin( angle ) ) * distance };
            if ( transform )
            {
                math::vector_itransform( point, *transform, point );
            }
            point = this->project( face, point );
            if ( transform )
            {
                point = math::transform_point( point, *transform );
            }
            if ( point.dist_to_sqr( seed.m_position ) > 0.0001f )
            {
                add_point( point, seed.m_normal, target, radius, out, seed.m_face, seed.m_transform );
            }
        }
    }
}

void splash::sample_batch( const splash_target_t &target, const vec3 &shoot_pos, f32 radius, bool arcing, splash_points_t &out )
{

    usize capacity{ };
    for ( sample_work_t &work : m_sample_work )
    {
        work.m_first = capacity;
        work.m_count = 0;
        const geometry_t &geometry{ work.m_geometry };
        if ( geometry.m_face_count )
        {
            const face_t &first{ m_faces[ geometry.m_first_face ] };
            const face_t &last{ m_faces[ geometry.m_first_face + geometry.m_face_count - 1 ] };
            const usize   vertices{ last.m_first + last.m_count - first.m_first };
            capacity += ( out.m_sticky ? 4 : 3 ) * vertices + 3 * geometry.m_face_count;
        }
    }

    const i32 jobs{ math::smaller( g_thread_pool.concurrency( ),
                                   static_cast< i32 >( math::smaller( m_sample_work.size( ), capacity / 4096 ) ) ) };
    if ( jobs < 2 || !m_generated.resize( math::bigger( capacity, m_generated.size( ) ) ) )
    {
        for ( const sample_work_t &work : m_sample_work )
        {
            this->sample( work.m_geometry, work.m_transformed ? &work.m_matrix : nullptr,
                          target, shoot_pos, radius, arcing, out, work.m_transform );
        }
        return;
    }

    splash_target_t worker_target{ target };
    worker_target.m_entity = nullptr;
    struct batch_t
    {
        splash                *m_owner;
        const splash_target_t *m_target;
        const vec3            *m_shoot;
        splash_points_t       *m_out;
        f32                    m_radius;
        bool                   m_arcing;
        i32                    m_jobs;
    } batch{ this, &worker_target, &shoot_pos, &out, radius, arcing, jobs };
    const auto run{ +[]( void *context, i32 index, i32 worker )
    {
        auto       &batch{ *static_cast< batch_t * >( context ) };
        splash     &owner{ *batch.m_owner };
        const usize count{ owner.m_sample_work.size( ) };
        const usize first{ count * index / batch.m_jobs };
        const usize last{ count * ( index + 1 ) / batch.m_jobs };
        for ( usize at{ first }; at < last; ++at )
        {
            sample_work_t &work{ owner.m_sample_work[ at ] };
            owner.sample( work.m_geometry, work.m_transformed ? &work.m_matrix : nullptr,
                          *batch.m_target, *batch.m_shoot, batch.m_radius, batch.m_arcing, *batch.m_out,
                          work.m_transform, owner.m_generated.data( ) + work.m_first, &work.m_count );
        }
    } };
    if ( !g_thread_pool.run( run, &batch, jobs ) )
    {
        for ( i32 at{ }; at < jobs; ++at )
        {
            run( &batch, at, 0 );
        }
    }
    {
        for ( const sample_work_t &work : m_sample_work )
        {
            for ( usize at{ }; at < work.m_count; ++at )
            {
                const splash_point_t &point{ m_generated[ work.m_first + at ] };
                add_point( point.m_position, point.m_normal, target, radius, out, point.m_face, point.m_transform, point.m_edge, point.m_edge_index );
            }
        }
    }
}

void splash::collect( const splash_target_t &target, const vec3 &shoot_pos, f32 radius, bool arcing, splash_points_t &out, projectile_work_t *work )
{

    m_sample_work.clear( );
    const auto enqueue{ [ & ]( const geometry_t &geometry, const matrix3x4 *matrix, i32 transform_index = -1 )
    {
        if ( !m_budget.m_vertices )
        {
            this->sample( geometry, matrix, target, shoot_pos, radius, arcing, out, transform_index );
            return;
        }
        vec3 low{ }, high{ };
        local_bounds( target, matrix, low, high );
        const f32 reach{ radius + out.m_margin + 0.03125f };
        const vec3 extent{ reach, reach, reach };
        low -= extent;
        high += extent;
        for ( usize first{ }; first < geometry.m_face_count; first += 64 )
        {
            if ( geometry.m_block_count )
            {
                const face_block_t &block{ m_face_blocks[ geometry.m_first_block + first / 64 ] };
                if ( !math::overlaps( block.m_mins, block.m_maxs, low, high ) )
                {
                    continue;
                }
            }
            sample_work_t *work{ m_sample_work.emplace_back( ) };
            if ( !work )
            {
                this->sample_batch( target, shoot_pos, radius, arcing, out );
                m_sample_work.clear( );
                geometry_t remaining{ geometry };
                remaining.m_first_face += first;
                remaining.m_face_count -= first;
                this->sample( remaining, matrix, target, shoot_pos, radius, arcing, out, transform_index );
                return;
            }
            work->m_geometry.m_first_face = geometry.m_first_face + first;
            work->m_geometry.m_face_count = math::smaller( geometry.m_face_count - first, usize{ 64 } );
            work->m_transformed           = matrix != nullptr;
            work->m_transform             = transform_index;
            if ( matrix )
            {
                atl::memcpy( work->m_matrix, *matrix, sizeof( matrix3x4 ) );
            }
        }
    } };
    const vec3 extent{ radius + out.m_margin, radius + out.m_margin, radius + out.m_margin };
    const vec3 mins{ target.m_mins - extent }, maxs{ target.m_maxs + extent };
    m_brush_ids.m_Size = 0;
    g_tf2.m_trace->GetBrushesInAABB( mins, maxs, &m_brush_ids, MASK_SOLID );
    for ( i32 at{ }; at < m_brush_ids.m_Size; ++at )
    {
        if ( work && !work->available( ) )
        {
            return;
        }
        if ( const geometry_t *geometry{ this->brush( m_brush_ids.m_Memory.m_pMemory[ at ], work ) } )
        {
            enqueue( *geometry, nullptr );
        }
    }
    const auto sample_collideable{ [ & ]( ICollideable *collideable, const model_t *model, const vcollide_t *collision )
    {
        const geometry_t *geometry{ this->model( model, collision, work ) };
        if ( !geometry )
        {
            return;
        }
        if ( !m_budget.m_local_depth )
        {
            enqueue( *geometry, &collideable->CollisionToWorldTransform( ) );
            return;
        }
        const i32    index{ static_cast< i32 >( m_transforms.size( ) ) };
        transform_t *transform{ m_transforms.emplace_back( ) };
        if ( !transform )
        {
            return;
        }
        atl::memcpy( transform->m_matrix, collideable->CollisionToWorldTransform( ), sizeof( matrix3x4 ) );
        enqueue( *geometry, &transform->m_matrix, index );
    } };
    for ( i32 x{ math::cell_axis( mins.x ) }; x <= math::cell_axis( maxs.x ); ++x )
    {
        for ( i32 y{ math::cell_axis( mins.y ) }; y <= math::cell_axis( maxs.y ); ++y )
        {
            for ( i32 z{ math::cell_axis( mins.z ) }; z <= math::cell_axis( maxs.z ); ++z )
            {
                if ( work && !work->available( ) )
                {
                    return;
                }
                cell_t *cell{ this->cell( x, y, z, work ) };
                if ( !cell )
                {
                    continue;
                }
                geometry_t &geometry{ cell->m_geometry };
                if ( geometry.m_owned && !geometry.m_sampled )
                {
                    if ( work && !work->prepare( ) )
                    {
                        return;
                    }
                    const vec3 cell_mins{ x * 256.0f, y * 256.0f, z * 256.0f };
                    const vec3 cell_maxs{ cell_mins + vec3( 256.0f, 256.0f, 256.0f ) };
                    geometry.m_first_face = m_faces.size( );
                    geometry.m_face_count = 0;
                    geometry.m_sampled    = this->cache_mesh( geometry.m_owned, geometry, &cell_mins, &cell_maxs );
                }
                enqueue( geometry, nullptr );
                for ( const prop_t &prop : cell->m_props )
                {
                    if ( work && !work->available( ) )
                    {
                        return;
                    }
                    if ( math::overlaps( prop.m_mins, prop.m_maxs, mins, maxs ) &&
                         x == math::cell_axis( math::bigger( prop.m_mins.x, mins.x ) ) &&
                         y == math::cell_axis( math::bigger( prop.m_mins.y, mins.y ) ) &&
                         z == math::cell_axis( math::bigger( prop.m_mins.z, mins.z ) ) )
                    {
                        sample_collideable( prop.m_collideable, prop.m_model, prop.m_collision );
                    }
                }
            }
        }
    }
    splash_entities entities{ };
    g_tf2.m_trace->EnumerateEntities( mins, maxs, &entities );
    CTraceFilterProjectile filter{ };
    for ( i32 at{ }; at < entities.m_count; ++at )
    {
        if ( work && !work->available( ) )
        {
            return;
        }
        IHandleEntity *handle{ entities.m_handles[ at ] };
        if ( !filter.ShouldHitEntity( handle, MASK_SOLID ) )
        {
            continue;
        }
        ICollideable *collideable{ g_tf2.m_trace->GetCollideable( handle ) };
        if ( !collideable || collideable->GetSolid( ) == SOLID_NONE || ( collideable->GetSolidFlags( ) & FSOLID_NOT_SOLID ) )
        {
            continue;
        }
        const model_t *model{ collideable->GetCollisionModel( ) };
        if ( !model )
        {
            continue;
        }
        const vcollide_t *collision{ g_tf2.m_model_info->GetVCollide( model ) };
        if ( !collision || !collision->solidCount )
        {
            continue;
        }
        sample_collideable( collideable, model, collision );
    }
    this->sample_batch( target, shoot_pos, radius, arcing, out );
}

const splash::query_t &splash::query( const vec3 &mins, const vec3 &maxs )
{
    const i32 command{ g_cl.m_cmd ? g_cl.m_cmd->command_number : -1 };
    if ( command != m_query_command || g_tf2.m_globals->framecount != m_query_frame )
    {
        m_query_count = m_query_next = 0;
        m_query_command              = command;
        m_query_frame                = g_tf2.m_globals->framecount;
    }
    for ( i32 at{ }; at < m_query_count; ++at )
    {
        const query_t &query{ m_queries[ at ] };
        if ( mins.x >= query.m_mins.x && mins.y >= query.m_mins.y && mins.z >= query.m_mins.z &&
             maxs.x <= query.m_maxs.x && maxs.y <= query.m_maxs.y && maxs.z <= query.m_maxs.z )
        {
            return query;
        }
    }
    query_t &query{ m_queries[ m_query_next ] };
    m_query_next  = ( m_query_next + 1 ) % 8;
    m_query_count = math::smaller( m_query_count + 1, 8 );
    const vec3 margin{ 32.0f, 32.0f, 32.0f };
    query.m_mins = mins - margin;
    query.m_maxs = maxs + margin;
    splash_entities entities{ };
    g_tf2.m_trace->EnumerateEntities( query.m_mins, query.m_maxs, &entities );
    query.m_count    = entities.m_count;
    query.m_overflow = entities.m_overflow;
    atl::memcpy( query.m_handles, entities.m_handles, sizeof( IHandleEntity * ) * query.m_count );
    return query;
}

bool splash::sweep( const vec3 &from, const vec3 &to, const vec3 &angles, const CPhysCollide *projectile,
                    const vec3 &mins, const vec3 &maxs, trace_t &trace, CTraceFilterProjectile &filter, projectile_work_t *work )
{
    trace_t best{ };
    best.fraction = 1.0f;
    best.endpos   = to;
    const auto cast{ [ & ]( const CPhysCollide *shape, const vec3 &origin, const vec3 &rotation, C_BaseEntity *entity )
    {
        if ( best.startsolid || best.allsolid )
        {
            return;
        }

        if ( work && !work->exact( ) )
        {
            return;
        }
        trace_t hit{ };

        g_tf2.m_physics_collision->TraceCollide( from, to, projectile, angles, shape, origin, rotation, &hit );
        if ( hit.startsolid || hit.allsolid || hit.fraction < best.fraction )
        {
            best        = hit;
            best.m_pEnt = entity;
        }
    } };
    if ( work && !work->available( ) )
    {
        return false;
    }
    const query_t &entities{ this->query( mins, maxs ) };
    if ( entities.m_overflow )
    {
        return false;
    }
    for ( i32 at{ }; at < entities.m_count; ++at )
    {
        if ( work && !work->available( ) )
        {
            return false;
        }
        IHandleEntity *handle{ entities.m_handles[ at ] };
        if ( !filter.ShouldHitEntity( handle, MASK_SOLID ) )
        {
            continue;
        }
        C_BaseEntity *entity{ entity_from_handle( handle ) };
        if ( entity->GetClientClass( )->GetClassID( ) == ETFClassID::CTFPlayer )
        {
            // VPhysicsUpdate handles player hits with a ray and then reflects velocity;
            Ray_t   ray{ };
            trace_t hit{ };
            ray.Init( from, to );
            g_tf2.m_trace->ClipRayToEntity( ray, MASK_SHOT, handle, &hit );
            if ( !best.startsolid && !best.allsolid && ( hit.startsolid || hit.allsolid || hit.fraction < best.fraction ) )
            {
                best        = hit;
                best.m_pEnt = entity;
            }
            continue;
        }
        ICollideable *collideable{ g_tf2.m_trace->GetCollideable( handle ) };
        if ( !collideable || collideable->GetSolid( ) == SOLID_NONE || ( collideable->GetSolidFlags( ) & FSOLID_NOT_SOLID ) )
        {
            continue;
        }
        const model_t    *model{ collideable->GetCollisionModel( ) };
        const vcollide_t *collision{ model ? g_tf2.m_model_info->GetVCollide( model ) : nullptr };
        if ( !collision || !collision->solidCount )
        {
            Ray_t   ray{ };
            trace_t hit{ };
            ray.Init( from, to, mins - math::lower( from, to ), maxs - math::upper( from, to ) );
            g_tf2.m_trace->ClipRayToEntity( ray, MASK_SOLID, handle, &hit );
            if ( hit.fraction < 1.0f || hit.startsolid || hit.allsolid )
            {
                return false;
            }
            continue;
        }
        for ( i32 solid{ }; solid < collision->solidCount; ++solid )
        {
            cast( collision->solids[ solid ], collideable->GetCollisionOrigin( ), collideable->GetCollisionAngles( ),
                  entity );
        }
    }

    if ( work && work->m_exhausted )
    {
        return false;
    }
    best.surface = trace.surface;
    trace        = best;
    return true;
}

void splash::clear( )
{
    ++m_generation;
    m_query_count = m_query_next = 0;
    m_query_command = m_query_frame = -1;
    m_sample_work.reset( );
    m_generated.reset( );
    m_transforms.reset( );
    for ( geometry_t &geometry : m_brushes )
    {
        if ( geometry.m_owned )
        {
            g_tf2.m_physics_collision->DestroyCollide( geometry.m_owned );
        }
    }
    for ( cell_t &cell : m_cells )
    {
        if ( cell.m_geometry.m_owned )
        {
            g_tf2.m_physics_collision->DestroyCollide( cell.m_geometry.m_owned );
        }
    }
    m_brushes.reset( );
    m_models.reset( );
    m_cells.reset( );
    m_faces.reset( );
    m_face_blocks.reset( );
    m_vertices.reset( );
    m_boundary.reset( );
    m_edges.reset( );
    m_edge_order.reset( );
    m_face_vertices.reset( );
    m_plane_ranges.reset( );
    m_brush_planes.reset( );
    m_seam_regions.reset( );
    m_seam_vertices.reset( );
    m_brush_ids.Purge( );
    m_neighbour_ids.Purge( );
    m_planes.Purge( );
    m_prop_query.Purge( );
}
