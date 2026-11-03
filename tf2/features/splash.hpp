#pragma once

#include "link.hpp"
#include "proj_sim.hpp"


struct splash_budget_t
{
    i32  m_candidates;
    i32  m_flights;
    i32  m_aim_passes;
    i32  m_air_samples;
    i32  m_local_points;
    i32  m_local_depth;
    i32  m_local_reserve;
    bool m_vertices;
    bool m_rocket_refinement;
    i32  m_sphere_points;
    i32  m_damage_checks;
};

struct splash_air_sample_t
{
    vec3 m_position{ };
    f32  m_time{ };
};

struct splash_air_samples_t
{
    static constexpr i32 CAPACITY{ 8 };
    splash_air_sample_t  m_samples[ CAPACITY ]{ };
    i32                  m_count{ };
};

struct splash_target_t
{
    C_BaseEntity *m_entity{ };
    vec3          m_origin{ };
    vec3          m_mins{ };
    vec3          m_maxs{ };
    vec3          m_eye{ };
};

struct splash_point_t
{
    vec3  m_position{ };
    vec3  m_normal{ };
    f32   m_distance{ };
    usize m_face{ static_cast< usize >( -1 ) };
    i32   m_transform{ -1 };
    bool  m_tested{ };
    bool  m_air{ };
    bool  m_edge{ };
    usize m_edge_index{ static_cast< usize >( -1 ) };
};

struct seam_point_t
{
    vec3 m_position{ };
    vec3 m_normal{ };
    f32  m_distance{ };
    vec3 m_wall{ };
    f32  m_wall_distance{ };
    vec3 m_slide{ };
    f32  m_low{ };
    f32  m_high{ };
    bool m_displacement{ };
    bool m_rounded{ };
    vec3 m_edge{ };
    vec3 m_arc[ 2 ]{ };
    bool m_blocked{ };
    bool m_plane{ };
    bool m_flush{ };
    bool m_diagonal{ };
    vec3 m_wall_mins{ };
    vec3 m_wall_maxs{ };
};

struct splash_points_t
{
    static constexpr i32 CAPACITY{ 160 };
    splash_point_t       m_points[ CAPACITY ]{ };
    i32                  m_count{ };
    i32                  m_limit{ 64 };
    f32                  m_spacing{ 0.01f };
    f32                  m_margin{ 1.0f };
    i32                  m_air_limit{ };
    bool                 m_sticky{ };
    splash_point_t       m_seams[ 16 ]{ };
    i32                  m_seam_count{ };
    i32                  m_seam_limit{ };
    bool                 m_seams_from_shooter{ };
    vec3                 m_shoot_pos{ };
};

class splash
{
public:
    static const splash_budget_t &quality( );
    void                          begin( );
    void                          sphere( const splash_target_t &target, f32 radius, i32 command, splash_points_t &out, bool interleave );
    void                          refine( const splash_point_t &seed, const splash_target_t &target, const vec3 &shoot_pos,
                                          f32 radius, f32 step, i32 count, splash_points_t &out ) const;
    void                          collect( const splash_target_t &target, const vec3 &shoot_pos, f32 radius, bool arcing, splash_points_t &out, projectile_work_t *work = nullptr );
    i32                           seams( const splash_target_t &target, const vec3 &shoot_pos, f32 radius, f32 step, seam_point_t *out, i32 capacity );
    void                          clear( );
    u32                           generation( ) const
    {
        return m_generation;
    }

    // the projectile's own shape against entities; the world and static props collide in vphysics
    bool sweep( const vec3 &from, const vec3 &to, const vec3 &angles, const CPhysCollide *projectile,
                const vec3 &mins, const vec3 &maxs, trace_t &trace, CTraceFilterProjectile &filter, projectile_work_t *work = nullptr );

    static vec3 player_eye( C_TFPlayer *player, const vec3 &origin, i32 flags );
    static f32  arm_time( C_TFWeaponBase *weapon );
    static f32  air_radius( f32 radius, f32 age );
    static void select_air_times( const vec3 *path, const vec3 *targets, i32 first, i32 last,
                                  const vec3 &mins, const vec3 &maxs, f32 radius, f32 falloff, splash_air_samples_t &out );
    static bool grenade_origin( const vec3 &position, C_BaseEntity *projectile, vec3 &out, const splash_target_t *target = nullptr );
    static bool grenade_near_target( const vec3 &position, f32 radius, const splash_target_t &target );
    static i32  clip_sticky_player( const vec3 &start, vec3 *path, i32 count, const vec3 *targets,
                                    const vec3 &mins, const vec3 &maxs, impact_t &impact );
    static bool reaches( const vec3 &origin, f32 radius, const splash_target_t &target );
    static f32  falloff( const vec3 &origin, f32 radius, f32 edge_damage, const splash_target_t &target );
    static f32  damage( const vec3 &origin, f32 radius, f32 edge_damage, const splash_target_t &target );
    static bool can_attach( const trace_t &trace );
    static bool buried( const vec3 &explosion );
    static bool rounded_explosion( const seam_point_t &seam, const vec3 &end, const vec3 &direction, f32 step, vec3 &explosion );
    static bool rounded_window( const seam_point_t &seam, const vec3 &start, const vec3 &direction, f32 step, f32 &low, f32 &high );

private:
    struct face_t
    {
        usize m_first{ };
        usize m_count{ };
        vec3  m_normal{ };
        vec3  m_mins{ };
        vec3  m_maxs{ };
    };

    struct edge_t
    {
        usize m_face{ };
        usize m_neighbor{ static_cast< usize >( -1 ) };
        i32   m_kind{ }; // 0 open/unknown, 1 coplanar, 2 convex, 3 concave
    };

    struct edge_key_t
    {
        vec3  m_low{ }, m_high{ };
        usize m_index{ };
    };

    struct geometry_t
    {
        usize               m_first_face{ };
        usize               m_face_count{ };
        usize               m_first_block{ };
        usize               m_block_count{ };
        CPhysCollide       *m_owned{ };
        const vcollide_t   *m_source{ };
        const CPhysCollide *m_first_solid{ };
        i32                 m_solid_count{ };
        bool                m_ready{ };
        bool                m_attempted{ };
        bool                m_sampled{ };
        vec3                m_mins{ };
        vec3                m_maxs{ };
        usize               m_first_seam{ };
        usize               m_seam_count{ };
        bool                m_seams_built{ };
    };

    struct plane_range_t
    {
        usize m_first{ };
        usize m_count{ };
        i32   m_contents{ };
        bool  m_attempted{ };
        bool  m_ready{ };
    };

    struct seam_region_t
    {
        usize m_first{ };
        usize m_count{ };
        vec3  m_normal{ };
        vec3  m_wall{ };
        f32   m_wall_distance{ };
        bool  m_displacement{ };
        bool  m_rounded{ };
        vec3  m_arc[ 2 ]{ };
    };

    struct face_block_t
    {
        vec3 m_mins{ }, m_maxs{ };
    };

    struct model_cache_t
    {
        const model_t *m_model{ };
        geometry_t     m_geometry{ };
    };

    struct prop_t
    {
        ICollideable     *m_collideable{ };
        const model_t    *m_model{ };
        const vcollide_t *m_collision{ };
        vec3              m_mins{ };
        vec3              m_maxs{ };
        geometry_t        m_world{ };
    };

    struct cell_t
    {
        u64                   m_key{ };
        geometry_t            m_geometry{ };
        atl::vector< prop_t > m_props{ };
        bool                  m_props_ready{ };
    };

    struct transform_t
    {
        matrix3x4 m_matrix{ };
    };
    struct query_t
    {
        vec3           m_mins{ };
        vec3           m_maxs{ };
        IHandleEntity *m_handles[ 256 ]{ };
        i32            m_count{ };
        bool           m_overflow{ };
    };
    const query_t &query( const vec3 &mins, const vec3 &maxs );
    struct sample_work_t
    {
        geometry_t m_geometry{ };
        matrix3x4  m_matrix{ };
        bool       m_transformed{ };
        i32        m_transform{ -1 };
        usize      m_first{ };
        usize      m_count{ };
    };
    void        sample_batch( const splash_target_t &target, const vec3 &shoot_pos, f32 radius, bool arcing, splash_points_t &out );
    bool        cache_convex( CPhysConvex *convex, geometry_t &out );
    bool        cache_mesh( const CPhysCollide *collide, geometry_t &out, const vec3 *cell_mins = nullptr, const vec3 *cell_maxs = nullptr );
    geometry_t *brush( i32 index, projectile_work_t *work = nullptr );
    bool        planes( i32 index, plane_range_t &out );
    void        build_seams( i32 index );
    void        build_cell_seams( geometry_t &geometry, const vec3 &mins, const vec3 &maxs );
    void        build_prop_seams( prop_t &prop );
    void        add_seam_regions( geometry_t &geometry, i32 owner, bool displacement );
    void        add_rounded_regions( geometry_t &geometry );
    geometry_t *model( const model_t *model, const vcollide_t *collision, projectile_work_t *work = nullptr );
    cell_t     *cell( i32 x, i32 y, i32 z, projectile_work_t *work = nullptr );
    void        sample( const geometry_t &geometry, const matrix3x4 *transform, const splash_target_t &target,
                        const vec3 &shoot_pos, f32 radius, bool arcing, splash_points_t &out, i32 transform_index = -1,
                        splash_point_t *raw = nullptr, usize *raw_count = nullptr ) const;
    bool        store_face( const vec3 *vertices, usize count, const vec3 &normal, geometry_t &out );
    void        link_edges( const geometry_t &geometry );
    void        index_faces( geometry_t &geometry );
    static void local_bounds( const splash_target_t &target, const matrix3x4 *transform, vec3 &mins, vec3 &maxs );
    static void add_point( const vec3 &point, const vec3 &normal, const splash_target_t &target, f32 radius, splash_points_t &out,
                           usize face = static_cast< usize >( -1 ), i32 transform = -1, bool edge = false,
                           usize edge_index = static_cast< usize >( -1 ) );
    const vec3 *directions( i32 count );
    vec3        project( const face_t &face, const vec3 &point ) const;

    atl::vector< geometry_t >     m_brushes{ };
    atl::vector< model_cache_t >  m_models{ };
    atl::vector< cell_t >         m_cells{ };
    atl::vector< face_t >         m_faces{ };
    atl::vector< face_block_t >   m_face_blocks{ };
    atl::vector< vec3 >           m_vertices{ };
    atl::vector< u8 >             m_boundary{ };
    atl::vector< edge_t >         m_edges{ };
    atl::vector< edge_key_t >     m_edge_order{ };
    atl::vector< vec3 >           m_face_vertices{ };
    CUtlVector< i32 >             m_brush_ids{ };
    CUtlVector< i32 >             m_neighbour_ids{ };
    CUtlVector< brush_plane_t >   m_planes{ };
    atl::vector< plane_range_t >  m_plane_ranges{ };
    atl::vector< brush_plane_t >  m_brush_planes{ };
    atl::vector< seam_region_t >  m_seam_regions{ };
    atl::vector< vec3 >           m_seam_vertices{ };
    CUtlVector< ICollideable * >  m_prop_query{ };
    atl::vector< transform_t >    m_transforms{ };
    atl::vector< sample_work_t >  m_sample_work{ };
    atl::vector< splash_point_t > m_generated{ };
    splash_budget_t               m_budget{ };
    vec3                          m_directions[ 64 ]{ };
    i32                           m_direction_count{ };
    query_t                       m_queries[ 8 ]{ };
    i32                           m_query_count{ };
    i32                           m_query_next{ };
    i32                           m_query_command{ -1 };
    i32                           m_query_frame{ -1 };
    u32                           m_generation{ };
};

extern splash g_splash;
