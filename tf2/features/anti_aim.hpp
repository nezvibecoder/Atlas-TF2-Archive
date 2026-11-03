#include "link.hpp"

#include "bone_setup.hpp"

constexpr i32 AUTO_YAW_BINS{ 24 };

struct anti_aim_target_t
{
    C_TFPlayer *m_player{ nullptr };
    vec3        m_angle_to{ };
    vec3        m_shoot_pos{ };
    f32         m_fov_to{ };
    bool        m_is_threat{ false };
    bool        m_sees_head{ false };
};

struct head_scan_t
{
    matrix3x4                     m_bones[ MAX_STUDIO_BONES ]{ };
    const mstudiohitboxset_t     *m_set{ nullptr };
    const mstudiobbox_t          *m_head{ nullptr };
    const mstudioposeparamdesc_t *m_aim_yaw{ nullptr };
    vec3                          m_origin{ };
    vec3                          m_points[ HEAD_SCAN_POINTS ]{ };
    vec3                          m_shown_points[ HEAD_SCAN_POINTS ]{ };
    vec3                          m_front_eye{ };
    f32                           m_head_yaw{ };
    f32                           m_shown_yaw{ };
    f32                           m_goal_feet{ };
    f32                           m_current_feet{ };
    bool                          m_moving{ false };
    i32                           m_exposed[ AUTO_YAW_BINS ]{ };
    i32                           m_shown_exposed[ AUTO_YAW_BINS ]{ };
};

struct yaw_score_t
{
    i32 m_real_exposed{ };
    i32 m_overlap{ };
    i32 m_fake_exposed{ };
};

class anti_aim
{
public:
    void think( );

    void paint( );
    bool build_shown_bones( matrix3x4 *bones, i32 &bone_count );

    bool using_fake_angles( );
    void reset( );

    void on_va_lock( vec3 &angle );
    void on_move_sent( i32 command );

private:
    bool should_run( );

    void perform_min_walk( );

    void handle_legit( );
    void handle_rage( );

    void fetch_targets( );
    bool should_hide_head( );
    bool uses_fake_yaw( );

    void handle_real( );
    void handle_fake( );
    void handle_pitch( );

    void update_rage_states( );
    void calc_wish_angles( );

    f32 get_real_base_yaw( );
    f32 get_fake_base_yaw( );
    f32 get_jitter_additive( bool fake );

    f32 real_command_yaw( );
    f32 fake_command_yaw( );

    bool        scan_head( );
    void        count_exposure( const matrix3x4 *bones, const vec3 *points, f32 built_yaw, i32 *exposed );
    f32         settled_head_yaw( f32 real_yaw, f32 fake_yaw );
    i32         count_overlap( f32 head_yaw, f32 fake_yaw );
    i32         count_shot_overlap( const vec3 &eye, f32 head_yaw, f32 fake_yaw );
    yaw_score_t score_yaws( f32 real_yaw, f32 fake_yaw );

    f32 determine_best_real_yaw( f32 fake_yaw );
    f32 determine_best_real_yaw( );
    f32 determine_best_fake_yaw( f32 real_yaw );

    void draw_hitboxes( CStudioHdr *studio, const pose_snapshot_t &snapshot, bool fake = false );

    config::anti_aim_profile_t m_profile{ };

    f32 m_wish_real_yaw{ 0.0f };
    f32 m_wish_fake_yaw{ 0.0f };
    f32 m_wish_pitch{ 0.0f };

    bool m_jitter_flip{ false };

    bool m_rage_mode{ false };

    f32 m_fake_update_time{ 0.0f };
    f32 m_real_update_time{ 0.0f };

    f32 m_spin_yaw{ 0.0f };

    bool m_cycle_fake{ false };
    bool m_cycle_real{ false };

    bool m_using_fake_yaw{ false };

    i32 m_last_choked{ 0 };
    i32 m_planned_choke{ 0 };

    vec3 m_camera_angles{ };
    vec3 m_sent_angles{ };

    atl::vector< anti_aim_target_t > m_targets{ };

    head_scan_t m_head_scan{ };
};

extern anti_aim g_anti_aim;
