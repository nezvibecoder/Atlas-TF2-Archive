#pragma once

#include "link.hpp"
#include "move_sim.hpp"
#include "nospread.hpp"
#include "proj_sim.hpp"
#include "splash.hpp"

namespace ballistics
{
    struct intercept_t
    {
        vec3 m_angle{ };
        f32  m_time{ };
    };
} // namespace ballistics

struct server_tick_projection_t;
struct rapid_fire_plan_t;

enum bbox_pos : i32
{
    bbox_pos_top    = 0,
    bbox_pos_mid    = 1,
    bbox_pos_bottom = 2,
};

struct ragebot_target_t
{
    C_BaseEntity *m_entity{ nullptr };       // The entity to aim at.
    vec3          m_aim_origin{ };           // The origin of the aim point.
    vec3          m_aim_angle{ };            // The angle used for the aim point.
    bool          m_should_predict{ false }; // Should prediction code run on this entity?
    f32           m_fov_to{ FLT_MAX };       // The FOV to the aimpoint we're aiming at.
    bool          m_got_hitpoint{ false };   // Has an aim point actually been acquired on this entity?
    ETFClassID    m_class_id{ };             // Entity class ID.
    i32           m_tick_count{ 0 };         // The tick count to set for this target.
    bool          m_steered{ false };        // The spread search placed the salvo, so nospread must leave the angle alone.
    i32           m_arrival{ 0 };            // Walk tick the projectile reaches the aim point, counted from launch.
    i32           m_hitbox{ 0 };             // The hitscan aimpoint/hitbox the ragebot went for. This field is only filled/used by hitscan weapons.
    usize         m_bbox_point{ 0 };         // The bbox point the ragebot went for. This field is only filled/used by projectile weapons.
};

enum class weapon_type : i32
{
    invalid_weapon    = -1, // A weapon not valid for ragebot.
    hitscan_weapon    = 0,  // A hitscan weapon.
    projectile_weapon = 1,  // A projectile weapon, differentiate for if prediction code should run.
};

struct ragebot_weapon_data_t
{
    // These fields are always filled on all weapons.
    weapon_type m_weapon_type{ weapon_type::invalid_weapon };  // The current weapon type.
    bool        m_fires_on_attack_release{ false };            // Does this weapon fire when we release attack? (charge weapons like the Huntsman)
    bool        m_should_fire_always_set_attack_bits{ false }; // Should the current weapon always set attack bits regardless of weapon timers?

    // These fields are only filled out if m_weapon_type is == weapon_type::projectile_weapon.
    f32  m_projectile_speed{ FLT_MAX }; // The forward launch speed the weapon reports (charge state included).
    f32  m_launch_speed{ FLT_MAX };     // Magnitude of the launch velocity: forward speed plus up kick, capped by vphysics.
    f32  m_up_kick{ 0.0f };             // Upward velocity grenade weapons add on top of the aim direction.
    f32  m_muzzle_forward{ 0.0f };      // How far ahead of the eye the projectile spawns, along the view.
    bool m_is_vphysics{ false };        // Does the projectile simulate through the physics engine? (pipes, stickies, cannonballs)
    bool m_splashes{ false };           // Does the projectile deal splash damage? (drives aiming at feet on grounded targets)
    bool m_is_sticky{ false };
    f32  m_gravity{ };
    f32  m_gravity_mod{ FLT_MAX };          // Fraction of sv_gravity applied to the projectile; zero means it flies straight.
    bool m_has_life_time{ false };          // Is the current weapon's projectiles limited by life time?
    f32  m_projectile_life_time{ FLT_MAX }; // How long will the projectile exist for before it despawns?
    bool m_has_hull_size{ false };          // Does the current weapon have a hull size?
    vec3 m_hull_mins{ };                    // The AA/mins of the projectile hull.
    vec3 m_hull_maxs{ };                    // The BB/maxs of the projectile hull.
    f32  m_blast_radius{ 0.0f };            // How far the splash reaches; the splashbot gathers its points inside it.
    f32  m_blast_falloff{ 0.5f };
    f32  m_arm_time{ 0.0f };     // Seconds a sticky needs before it can detonate; zero for everything else.
    f32  m_charge_time{ -1.0f }; // Seconds of charge the solve launches with; negative reads the weapon's own.
    f32  m_launch_spread{ 0.0f };

    f32 m_range{ 0.0f };
};

// a skeleton the scan may shoot at
struct scan_pose_t
{
    const matrix3x4 *m_bones{ nullptr };
    vec3             m_origin{ };
    i32              m_tick{ };
    f32              m_scale{ 1.0f };
    vec3             m_mins{ };
    vec3             m_maxs{ };
};

inline constexpr i32 MASK_SIZE{ 64 };
inline constexpr i32 KERNEL_CAPACITY{ 512 };
inline constexpr i32 MAX_PROJECTILE_TARGETS{ 6 };

struct kernel_entry_t
{
    f32 m_x{ };
    f32 m_y{ };
    f32 m_weight{ };
};

struct salvo_kernel_t
{
    kernel_entry_t m_entries[ KERNEL_CAPACITY ]{ };
    i32            m_count{ };
    f32            m_weight{ }; // sum of the entries
    f32            m_spread{ }; // tangent half width one unit of an entry stands for
    f32            m_range{ };  // how far the server traces a pellet
    bool           m_is_prior{ };
};

enum cell_state_t : u8
{
    cell_unknown,
    cell_miss,    // no hitbox on the ray, or one the partition never hands the trace
    cell_blocked, // obstructed or unchecked after the trace budget is exhausted
    cell_hit,
};

struct salvo_mask_t
{
    struct rect_t
    {
        f32  m_low_u{ };
        f32  m_low_v{ };
        f32  m_high_u{ };
        f32  m_high_v{ };
        vec3 m_extreme[ 4 ]{ }; // the corner reaching each bound, in the order the bounds are listed
        bool m_valid{ };
    };

    vec3 m_eye{ };
    vec3 m_centre{ };
    vec3 m_forward{ };
    vec3 m_right{ };
    vec3 m_up{ };
    f32  m_cell{ };
    f32  m_distance{ };
    f32  m_range{ };

    const mstudiohitboxset_t *m_set{ };
    const scan_pose_t        *m_pose{ };
    C_BaseEntity             *m_player{ };
    i32                       m_hitboxes{ };

    rect_t m_rects[ HITBOX_MAX ]{ };
    vec3   m_hull_mins{ };
    vec3   m_hull_maxs{ };

    cell_state_t m_cells[ MASK_SIZE * MASK_SIZE ]{ };
    i32          m_traces{ };
    i32          m_trace_limit{ };
    f32          m_open_hits{ }; // best unobstructed yield among the placements actually evaluated
};

struct salvo_score_t
{
    f32 m_hits{ };      // expected pellets on the target
    f32 m_open_hits{ }; // pellets that intersect the target before world occlusion
    f32 m_mean_i{ };    // where the landing pellets centre, in cells
    f32 m_mean_j{ };
};

struct shot_draw_t
{
    vec3   m_corners[ HITBOX_MAX ][ 8 ]{ };
    bool   m_valid[ HITBOX_MAX ]{ };
    i32    m_boxes{ };
    i32    m_aimed{ -1 };
    vec3   m_shoot_pos{ };
    vec3   m_aim_point{ };
    double m_time{ -1.0 };
};

struct shot_box_t
{
    matrix3x4 m_bone{ };
    vec3      m_mins{ };
    vec3      m_maxs{ };
};

inline constexpr i32 ARROW_GRID_SIZE{ 24 };

enum arrow_cell_t : u8
{
    arrow_cell_miss,
    arrow_cell_body,
    arrow_cell_head,
};

struct arrow_grid_t
{
    vec3         m_origin{ };
    vec3         m_centre{ };
    vec3         m_right{ };
    vec3         m_up{ };
    vec3         m_reach{ };
    vec3         m_low{ };
    vec3         m_high{ };
    f32          m_cell{ };
    arrow_cell_t m_cells[ ARROW_GRID_SIZE * ARROW_GRID_SIZE ]{ };
    i32          m_depth[ ARROW_GRID_SIZE * ARROW_GRID_SIZE ]{ };
    i32          m_deepest{ };
};

struct launch_t
{
    enum class kind : u8
    {
        direct,
        splash
    };
    kind                 m_kind{ kind::direct };
    vec3                 m_angle{ };     // view angle to send, roll included
    vec3                 m_position{ };  // contact position from the simulated flight
    vec3                 m_aim_point{ }; // surface point or target-relative intercept/airburst offset
    f32                  m_time{ };      // flight time to the point
    i32                  m_arrival{ };   // walk tick the projectile is at the point
    bool                 m_high_arc{ };  // the high arc rather than the direct arc
    bool                 m_air{ };
    bool                 m_family{ }; // a sticky flight of the arc search, revalidated along its arc
    bool                 m_ticked{ };
    impact_t             m_impact{ };
    f32                  m_splash_score{ };
    splash_air_samples_t m_air_samples{ };
};

struct target_path_t
{
    vec3 m_points[ MAX_WALK_TICKS ]{ };
    i32  m_count{ };
    i32  m_ground_agreement_ticks{ -1 };
    vec3 m_bounds_origin{ }; // where the walk ends, so the box sits on the solved tick
    vec3 m_bounds_mins{ };
    vec3 m_bounds_maxs{ };
};

class ragebot
{
public:
    static constexpr i32 PATH_TRAVEL_FADE_TICKS{ 12 };

    struct path_pen_t
    {
        usize m_style{ };
        f64   m_clock{ };
        bool  m_outline{ };
    };

    void begin_render_frame( )
    {
        m_render_frame.fetch_add( 1 );
    }

    void              think( );
    void              on_entity_deleted( C_BaseEntity *entity, u32 handle );
    bool              cancel_reload( );
    void              maintain_managed_charge( ); // run this on createmove
    void              paint( ) const;
    void              render( ) const; // world-space visuals, from inside the 3D view
    static void       draw_path_segment( const vec3 &start, const vec3 &end, const color_t &value, bool separator = false, bool outline = false );
    static path_pen_t path_pen( usize style, bool outline );
    static void       draw_path_step( const path_pen_t &pen, const vec3 *points, i32 count, i32 at, const color_t &value );
    static f32        path_travel_alpha( i32 segment, double elapsed );

    bool aimed_this_command( ) const;

    bool aimed_projectile_this_command( ) const
    {
        return this->aimed_this_command( ) &&
               m_weapon_data.m_weapon_type == weapon_type::projectile_weapon;
    }

    bool hitscan_active( ) const
    {
        return m_weapon_data.m_weapon_type == weapon_type::hitscan_weapon;
    }

    bool evaluated_this_command( ) const;

    bool   visual_aim( vec3 &out ) const;
    bool   crosshair_position( vec2 &out ) const;
    u32    visual_target_handle( ) const;
    double visual_target_started( ) const
    {
        return this->m_visual_target_started;
    }
    i32 network_ticks( i32 hold ) const;
    i32 target_catch_up_ticks( C_TFPlayer *player, i32 hold ) const;

    bool schedule_rapid_fire( const rapid_fire_plan_t &plan, u32 &withheld ) const;

private:
    struct contact_t
    {
        vec3                 m_position{ };
        vec3                 m_target{ };
        f32                  m_time{ -1.0f };
        f32                  m_distance{ FLT_MAX };
        impact_t             m_impact{ };
        splash_air_samples_t m_air_samples{ };
    };

    // one of the two sticky arcs toward a target, flat or lobbed, carried from command to command
    struct sticky_arc_t
    {
        f32  m_pitch{ };        // the pitch the next command flies first
        f32  m_yaw{ };          // correction to the vacuum yaw for the muzzle offset and the launch jitter
        f32  m_slope{ };        // range error per degree of pitch along the arc
        f32  m_lower{ -90.0f }; // the shot is flatter than this
        f32  m_upper{ 90.0f };  // and steeper than this
        f32  m_short_speed{ };  // launch speed below which the farthest flight cannot reach the target
        f32  m_short_by{ };     // how far short the farthest flight came down
        f32  m_speed{ };        // launch speed the pitch was found at
        f32  m_held_speed{ };   // launch speed the bounds and the rest hold for
        vec3 m_held_origin{ };  // and where the target stood
        bool m_seeded{ };
        bool m_rested{ };
    };

    struct sticky_flight_t
    {
        f32  m_error{ };   // along the bearing where the flight comes down through the target's centre height
        f32  m_lateral{ }; // across the bearing, positive to the left
        f32  m_quality{ }; // of the shot it found: splash score, or how near the box's middle it touched
        vec3 m_stop{ };    // where a blocked flight stopped
        bool m_flown{ };
        bool m_blocked{ };   // stopped short of the target before it came down to it
        bool m_ceiling{ };   // and that something faced down
        bool m_truncated{ }; // still above the target where the forecast ends
    };

    bool  fill_weapon_data( f32 charge_time = -1.0f );
    bool  should_run( );
    bool  holds_aim_key( );
    bool  do_prefetch_logic( );
    bool  is_hitbox_allowed_for_current_state( i32 hitbox ) const;
    usize get_best_priority_hitbox( C_TFPlayer *target ) const;

    void fetch_initial_targets( );
    void notify_bait( bool baited, bool overridden );
    bool get_best_target( ragebot_target_t &out );
    bool get_hitscan_aimpoint( ragebot_target_t &out );
    bool get_building_aimpoint( ragebot_target_t &out );

    bool get_projectile_target( ragebot_target_t &out );
    bool get_projectile_aimpoint( ragebot_target_t &out );
    bool walk_target( C_TFPlayer *player, move_sim_ctx_t &ctx, i32 &catch_up ) const;
    bool solve_walk( move_sim_ctx_t *ctx, i32 catch_up, ragebot_target_t &out );
    bool solve_sticky_walk( move_sim_ctx_t *ctx, i32 catch_up, ragebot_target_t &out );
    bool extend_sticky_walk( move_sim_ctx_t *ctx, i32 catch_up, f32 time = -1.0f );
    bool fly_sticky( const move_sim_ctx_t *ctx, i32 catch_up, const vec3 &angle, bool high_arc, sticky_flight_t &flight, launch_t &out );
    bool search_sticky_arc( move_sim_ctx_t *ctx, i32 catch_up, bool high_arc, i32 allowance, sticky_arc_t &arc, launch_t &out );
    bool solve_sticky_arcs( move_sim_ctx_t *ctx, i32 catch_up, sticky_arc_t *arcs, launch_t &out );
    bool charge_blocks_shot( move_sim_ctx_t *walks, const i32 *catch_ups, i32 walked );
    bool solve_box( const move_sim_ctx_t *ctx, const vec3 &shoot_pos, i32 catch_up, bool high_arc, ragebot_target_t &out );
    bool capture_arrow_pose( C_TFPlayer *player );
    i32  arrow_hitbox( const vec3 &entry, const vec3 &direction, f32 speed, const vec3 &forward, const vec3 &shift ) const;
    void build_arrow_grid( const move_sim_ctx_t *ctx, const vec3 &origin, const vec3 &direction, f32 speed, const vec3 &forward,
                           arrow_grid_t &grid ) const;
    bool arrow_safe_point( const move_sim_ctx_t *ctx, const vec3 &shoot_pos, i32 catch_up, bool high_arc, vec3 &offset );
    bool steer_arrow( const vec3 &offset, const move_sim_ctx_t *ctx, i32 catch_up, launch_t &launch );
    bool arrow_holds_safe_point( const move_sim_ctx_t *ctx, i32 catch_up, const launch_t &launch ) const;
    bool handle_splashbot( const move_sim_ctx_t *ctx, ragebot_target_t &tgt, const vec3 &shoot_pos, i32 catch_up, bool high_arc );
    bool solve_splash_point( const move_sim_ctx_t *ctx, const vec3 &shoot_pos, i32 catch_up, const splash_point_t &point, bool high_arc, launch_t &out );
    bool check_splash_damage( const move_sim_ctx_t *ctx, const vec3 &shoot_pos, i32 catch_up, launch_t &launch );
    bool solve_sticky_air( const move_sim_ctx_t *ctx, const vec3 &shoot_pos, i32 catch_up, const vec3 &offset, bool high_arc, launch_t &out );
    bool select_sticky_air( const move_sim_ctx_t *ctx, const vec3 &shoot_pos, i32 catch_up, const launch_t &launch, launch_t &out );
    bool revalidate_sticky( move_sim_ctx_t *ctx, i32 catch_up, ragebot_target_t &out );
    bool begin_sticky_candidate( );

    bool      gather_launch( const vec3 &angle, const move_sim_ctx_t *ctx, i32 catch_up, projectile_setup_t &setup ) const;
    bool      flight_time( const vec3 &delta, bool high_arc, f32 &time ) const;
    bool      solve_launch( const vec3 &from, const vec3 &point, bool high_arc, launch_t &out ) const;
    bool      solve_intercept( const vec3 &shoot_pos, const vec3 &offset, const move_sim_ctx_t *ctx, i32 catch_up, bool high_arc, launch_t &out, bool roots_checked = false ) const;
    i32       arrival_tick( f32 flight_time ) const;
    bool      verify_launch( const vec3 &point, const move_sim_ctx_t *ctx, i32 catch_up, bool splash, launch_t &launch );
    bool      refine_launch( const vec3 &point, const move_sim_ctx_t *ctx, i32 catch_up, bool splash, f32 roll,
                             launch_t &launch, launch_t *blocked = nullptr );
    bool      refine_rocket_seams( const move_sim_ctx_t *ctx, const vec3 &shoot_pos, i32 catch_up, const splash_target_t &estimate, launch_t &out );
    bool      confirm_rocket_seam( const move_sim_ctx_t *ctx, const vec3 &shoot_pos, i32 catch_up, const projectile_setup_t &setup,
                                   launch_t &launch, i32 &remaining_steps );
    i32       sticky_intercepts( const vec3 &shoot_pos, const vec3 &offset, const move_sim_ctx_t *ctx, i32 catch_up, ballistics::intercept_t *roots ) const;
    contact_t closest_approach( const vec3 &angle, const vec3 &point, const move_sim_ctx_t *ctx, i32 catch_up, bool splash, const launch_t &launch, const projectile_setup_t *gathered = nullptr );
    vec3      muzzle_towards( const vec3 &shoot_pos, const vec3 &point ) const;
    bool      eye_sees( const vec3 &point ) const;
    bool      rolls_allowed( const vec3 &target ) const;
    bool      quarter_roll_allowed( ) const;
    bool      straight_rocket( ) const;
    bool      aim_rocket( const vec3 &want, const move_sim_ctx_t *ctx, i32 catch_up, f32 roll, vec3 &angle ) const;
    void      take_aimpoint( const move_sim_ctx_t *ctx, i32 catch_up, const launch_t &launch, ragebot_target_t &out );
    void      keep_path( const move_sim_ctx_t *ctx, i32 first_tick, i32 through_tick );

    bool is_bbox_pos_allowed_for_current_state( i32 bbox_pos ) const;
    i32  priority_bbox_pos( const move_sim_ctx_t *ctx ) const;
    i32  aim_order( const move_sim_ctx_t *ctx, i32 *order ) const;
    i32  projectile_hitchance( const move_sim_ctx_t *ctx, i32 through_tick, f32 splash_tolerance = -1.0f ) const;
    i32  required_hitchance( i32 arrival ) const;
    i32  projectile_catch_up_ticks( C_TFPlayer *player ) const;
    i32  target_catch_up_epoch( C_TFPlayer *player, i32 server_epoch ) const;

    void cancel_charge( );
    bool cannon_fuse_holds( const ragebot_target_t &tgt ) const;

    void paint_target_path( ) const;
    void paint_fov_circle( ) const;
    void paint_shot( ) const;
    void paint_shot_mark( ) const;
    f32  shot_fade( ) const;
    void remember_shot( const mstudiohitboxset_t *set, const scan_pose_t &pose, i32 hitbox_count, i32 hitbox,
                        const vec3 &shoot_pos, const vec3 &aim_point );

    bool trace_hitbox( const mstudiohitboxset_t *set, const scan_pose_t &pose, i32 hitbox,
                       C_BaseEntity *player, const vec3 &shoot_pos, vec3 &out_pos, vec3 &out_ang,
                       bool &doubt );
    bool shot_registers( const mstudiohitboxset_t *set, const scan_pose_t &pose, C_BaseEntity *player,
                         const vec3 &shoot_pos, const vec3 &point ) const;

    bool scan_pose( const mstudiohitboxset_t *set, const scan_pose_t &pose, i32 hitbox_count,
                    i32 priority_box, C_TFPlayer *player, const vec3 &shoot_pos,
                    i32 declare_tick, bool newest, ragebot_target_t &out );

    void build_kernel( salvo_kernel_t &kernel ) const;
    i32  extreme_seeds( const salvo_mask_t &mask, vec3 *points, i32 *hitboxes ) const;

    bool target_state_allowed( C_TFPlayer *player, bool crossbow ) const;
    bool target_class_allowed( i32 player_class ) const;

    bool within_fov( const vec3 &shoot_pos, const vec3 &point ) const;
    bool find_target( ragebot_target_t &out );
    bool should_fire( bool ignore_timer = false );
    bool aimable_target_is_kill_condition( const ragebot_target_t &tgt );
    void aim_at_target( const ragebot_target_t &tgt );
    f32  shot_pellets( const rapid_fire_plan_t &plan, const salvo_kernel_t &kernel, i32 ticks, i32 extra, f32 &open ) const;
    bool enters_pose( const vec3 &from, const vec3 &end, vec3 &hit ) const;

    atl::vector< ragebot_target_t > m_targets{ };
    ragebot_weapon_data_t           m_weapon_data{ };
    projectile_setup_t              m_launch_setup{ };
    move_sim_ctx_t                  m_selected_walk{ };
    i32                             m_selected_catch_up{ };
    static constexpr i32            FLIGHT_CACHE_SIZE{ 4 };
    struct flight_cache_t
    {
        vec3     m_angle{ };
        bool     m_splash{ };
        vec3     m_start{ };
        vec3     m_path[ MAX_WALK_TICKS ]{ };
        impact_t m_impact{ };
        i32      m_count{ };
    };
    flight_cache_t  m_flights[ FLIGHT_CACHE_SIZE ]{ };
    i32             m_flight_count{ };
    i32             m_flight_next{ };
    i32             m_splash_simulations[ 2 ]{ };
    splash_points_t m_low_splash_points{ };
    bool            m_splash_started{ };
    struct sticky_search_t
    {
        i32            m_target{ -1 };
        i32            m_weapon{ -1 };
        i32            m_command{ -1 };
        u32            m_generation{ };
        u32            m_settings{ };
        vec3           m_eye{ };
        vec3           m_origin{ };
        vec3           m_mins{ };
        vec3           m_maxs{ };
        f32            m_seed_speed{ };
        f32            m_gravity{ };
        f32            m_radius{ };
        f32            m_arm{ };
        i32            m_horizon{ };
        sticky_arc_t   m_arcs[ 2 ]{ };       // flat and lobbed at the charge held
        sticky_arc_t   m_fresh_arcs[ 2 ]{ }; // the same at a fresh charge, for the cancel
        i32            m_stage{ };
        i32            m_discovery_command{ };
        i32            m_cursor{ };
        i32            m_roll{ };
        i32            m_refine_passes{ };
        i32            m_air_tick{ };
        vec3           m_point{ };
        vec3           m_angle{ };
        bool           m_seed{ };
        bool           m_high_arc{ };
        splash_point_t m_refine_seed{ };
        f32            m_refine_score{ -1.0f };
        i32            m_refine_tick{ };
    };
    sticky_search_t           m_sticky_searches[ MAX_PROJECTILE_TARGETS ]{ };
    sticky_search_t          *m_sticky_search{ };
    mutable projectile_work_t m_sticky_work{ };
    atl::atomic< i32 >        m_render_frame{ };
    i32                       m_sticky_target{ };
    i32                       m_candidate_index{ };
    bool                      m_resume_seed{ };
    bool                      m_sticky_deferred{ };
    bool                      m_sticky_probe{ };
    i32                       m_candidate_first_flight{ };
    launch_t                  m_candidate_launch{ };
    vec3                      m_sticky_path[ MAX_WALK_TICKS ]{ };
    launch_t                  m_selected_launch{ };
    i32                       m_selected_sticky_target{ -1 };
    i32                       m_selected_sticky_weapon{ -1 };
    u32                       m_selected_sticky_generation{ };
    u32                       m_selected_sticky_settings{ };
    bool                      m_pending_sticky{ };
    bool                      m_bait_notified{ };
    bool                      m_bait_overridden{ };
    vec3                      m_view_angles{ };
    vec3                      m_visual_point{ };
    double                    m_visual_time{ };
    bool                      m_visual_live{ };
    u32                       m_visual_target{ INVALID_EHANDLE_INDEX };
    double                    m_visual_target_started{ };
    double                    m_visual_target_time{ };
    i32                       m_aimed_sequence{ -1 };
    vec3                      m_shot_eye{ };
    vec3                      m_shot_point{ };
    vec3                      m_shot_velocity{ };
    C_BaseEntity             *m_shot_target{ };
    shot_box_t                m_shot_boxes[ HITBOX_MAX ]{ };
    i32                       m_shot_box_count{ };
    vec3                      m_shot_hull_mins{ };
    vec3                      m_shot_hull_maxs{ };
    bool                      m_charge_managed{ };   // the pending weapon charge is ours, not the user's
    bool                      m_charge_starting{ };  // our press has not reached PrimaryAttack yet
    bool                      m_charge_discarded{ }; // a cancel binned the charge but the netvars still show it until the ack lands
    i32                       m_charge_weapon{ };    // entindex the managed charge belongs to; a weapon change ends management
    bool                      m_projectile_solved{ };
    bool                      m_reload_probe{ };
    i32                       m_charge_reselect{ }; // entindex of the launcher a cancel holstered, taken back out on the very next command
    target_path_t             m_path{ };
    target_path_t             m_fired_path{ };
    double                    m_path_time{ };
    double                    m_path_hold_until{ };
    double                    m_fired_path_time{ };
    salvo_kernel_t            m_kernel{ };
    salvo_kernel_t            m_rolled_kernel{ }; // m_kernel turned by the roll of the pose being scanned
    shot_draw_t               m_shot_draw{ };
    shot_draw_t               m_pending_draw{ };
    scan_pose_t               m_arrow_pose{ };
    const mstudiohitboxset_t *m_arrow_set{ };
    matrix3x4                 m_arrow_bones[ MAX_STUDIO_BONES ]{ };
    arrow_grid_t              m_arrow_grid{ };
    i32                       m_evaluated_sequence{ -1 };

    config::ragebot_hitscan_profile_t    m_hitscan_profile{ };
    config::ragebot_projectile_profile_t m_projectile_profile{ };
};

extern ragebot g_ragebot;
