#pragma once

#include "gui/particles.hpp"
#include "gui/watermark.hpp"

#include "util/string.hpp"
#include "util/windefs.hpp"

// @ the menu tree binds straight to these members, so their addresses have to outlive every
// page. Everything the game does not read yet is carried over from the state the previous
// menu held inline, so the pages keep the values they had
class config
{
public:
    enum class esp_target_t : u8
    {
        enemy,
        team,
        local,
        friends,
        atlas,
        count,
    };

    enum class esp_element_t : usize
    {
        box,
        name,
        distance,
        health,
        weapon,
        flags,
        skeleton,
        tag,
        count,
    };

    enum class esp_anchor_t : u8
    {
        top,
        bottom,
        left,
        right,
    };

    enum class esp_align_t : u8
    {
        start,
        center,
        end,
    };

    enum class cham_entity_t : usize
    {
        players,
        buildings,
        world,
        count,
    };

    static constexpr usize ESP_TARGET_COUNT{ static_cast< usize >( esp_target_t::count ) };
    static constexpr usize ESP_ELEMENT_COUNT{ static_cast< usize >( esp_element_t::count ) };
    static constexpr usize CHAM_ENTITY_COUNT{ static_cast< usize >( cham_entity_t::count ) };

    enum : usize
    {
        CFG_PROFILE_SCOUT    = 0,
        CFG_PROFILE_SOLDIER  = 1,
        CFG_PROFILE_PYRO     = 2,
        CFG_PROFILE_DEMOMAN  = 3,
        CFG_PROFILE_HEAVY    = 4,
        CFG_PROFILE_ENGINEER = 5,
        CFG_PROFILE_MEDIC    = 6,
        CFG_PROFILE_SNIPER   = 7,
        CFG_PROFILE_SPY      = 8,

        CFG_PROFILE_COUNT,
    };

    enum : usize
    {
        TARGET_VISUAL_OFF,
        TARGET_VISUAL_BRACKETS,
        TARGET_VISUAL_ORBIT,
        TARGET_VISUAL_COUNT,
    };

    enum : usize
    {
        HEALTH_STYLE_SOLID,
        HEALTH_STYLE_GRADIENT,
        HEALTH_STYLE_HEALTH,
        HEALTH_STYLE_COUNT,
    };

    enum : usize
    {
        RJUMP_HEIGHT,
        RJUMP_IMPULSE,
        RJUMP_PRIORITY_COUNT,
    };

    enum : usize
    {
        PICKUP_HEALTH,
        PICKUP_AMMO,
        PICKUP_INTEL,
        PICKUP_COUNT,
    };

    enum : usize
    {
        WORLD_PICKUPS,
        WORLD_INTEL,
        WORLD_PROJECTILES,
        WORLD_NPCS,
        WORLD_COUNT,
    };

    struct cham_profile_t
    {
        usize m_visible{ };
        usize m_visible_overlay{ };
        usize m_invisible{ };
        usize m_invisible_overlay{ };

        color_t m_visible_color{ 0x9C, 0xA8, 0xB0, 0x6F };
        color_t m_occluded_color{ 245, 190, 40 };
        color_t m_visible_overlay_color{ 25, 200, 255 };
        color_t m_occluded_overlay_color{ 255, 255, 255 };

        bool off( ) const
        {
            return !this->m_visible && !this->m_visible_overlay && !this->m_invisible &&
                   !this->m_invisible_overlay;
        }
    };

    struct esp_profile_t
    {
        bool           m_elements[ ESP_ELEMENT_COUNT ]{ };
        cham_profile_t m_chams{ };
        u8             m_order[ ESP_ELEMENT_COUNT ]{ 0, 1, 2, 3, 4, 5, 6, 7 };

        esp_anchor_t m_anchors[ ESP_ELEMENT_COUNT ]{
            esp_anchor_t::top, esp_anchor_t::top, esp_anchor_t::bottom, esp_anchor_t::left,
            esp_anchor_t::bottom, esp_anchor_t::right, esp_anchor_t::top,
            esp_anchor_t::top
        };

        esp_align_t m_aligns[ ESP_ELEMENT_COUNT ]{
            esp_align_t::center, esp_align_t::center, esp_align_t::center, esp_align_t::center,
            esp_align_t::center, esp_align_t::start, esp_align_t::center,
            esp_align_t::center
        };

        color_t m_box_color{ 255, 255, 255 };
        color_t m_name_color{ 255, 255, 255 };
        color_t m_distance_color{ 255, 255, 255 };
        usize   m_health_style{ HEALTH_STYLE_SOLID };
        color_t m_health_color{ 0, 200, 32 };
        color_t m_health_low_color{ 200, 0, 32 };
        color_t m_overheal_color{ 0, 190, 255 };
        i32     m_health_divisions{ };
        color_t m_weapon_color{ 220, 220, 220 };
        color_t m_skeleton_color{ 255, 255, 255 };
    };

    enum : usize
    {
        PROJECTILE_TEAM_ENEMY,
        PROJECTILE_TEAM_TEAMMATE,

        PROJECTILE_TEAM_COUNT,
    };

    struct esp_t
    {
        esp_profile_t m_profiles[ ESP_TARGET_COUNT ]{
            { { true, true, true, true, true, true, false, true }, { 8, 3, 8, 0 } },
            { { false, true, false, true } },
            { { false, false, false, false, true }, { 1 } },
            { { true, true, true, true, true, true, false, true }, { 8, 3, 8, 0 } },
            { { true, true, true, true, true, true, false, true }, { 8, 3, 8, 0 } }
        };

        usize m_target_group{ };
        usize m_edited{ };

        esp_profile_t &edited( )
        {
            return this->m_profiles[ this->m_edited ];
        }

        i32 m_dormant_time{ 3 };

        usize m_avatars{ }; // render::AVATAR_NONE / AVATAR_SQUARE / AVATAR_CIRCLE

        usize   m_target_style{ TARGET_VISUAL_OFF };
        bool    m_target_chevron{ };
        bool    m_target_animate{ true };
        color_t m_target_color{ 110, 231, 236 };

        bool m_building_elements[ 5 ]{ true, true, true, true, true };
        bool m_projectile_elements[ 4 ]{ true, true, true, true };
        bool m_pickup_elements[ 4 ]{ true, true, true, true };
        bool m_intel_elements[ 4 ]{ true, true, true, true };
        bool m_npc_elements[ 3 ]{ true, true, true };

        bool m_projectile_teams[ PROJECTILE_TEAM_COUNT ]{ true, false };

        bool shows_projectile( bool teammate ) const
        {
            return this->m_projectile_teams[ teammate ? PROJECTILE_TEAM_TEAMMATE : PROJECTILE_TEAM_ENEMY ];
        }

        bool m_draw_teammates_conditionally{ false };
    };

    enum : usize
    {
        HITMARKER_NORMAL,
        HITMARKER_CRITICAL,
        HITMARKER_KILL,
        HITMARKER_EVENT_COUNT,
    };

    enum : usize
    {
        HITMARKER_WORLD,
        HITMARKER_2D,
        HITMARKER_MODE_COUNT,
    };

    struct hitmarker_t
    {
        bool    m_enabled{ };
        bool    m_show_damage{ true };
        usize   m_render_mode{ HITMARKER_WORLD };
        color_t m_color{ 220, 235, 240 };

        usize   m_edited_event{ };
        bool    m_override_color[ HITMARKER_EVENT_COUNT ]{ false, true, true };
        color_t m_event_color[ HITMARKER_EVENT_COUNT ]{
            { 220, 235, 240 }, { 255, 205, 80 }, { 255, 95, 90 }
        };
    };

    enum : usize
    {
        LOCAL_MODEL_PLAYER    = 0,
        LOCAL_MODEL_VIEWMODEL = 1,
        LOCAL_MODEL_WEAPON    = 2,

        LOCAL_MODEL_COUNT,
    };

    enum : usize
    {
        ENEMY_MODEL_PLAYER  = 0,
        ENEMY_MODEL_LAGCOMP = 1,

        ENEMY_MODEL_COUNT,
    };

    enum : usize
    {
        BACKTRACK_OLDEST = 0,
        BACKTRACK_ALL    = 1,

        BACKTRACK_COUNT,
    };

    struct chams_t
    {
        cham_profile_t m_buildings{ 7, 3, 2, 0 };
        cham_profile_t m_projectiles{ 7, 3, 2, 0 };
        cham_profile_t m_pickups{ };
        cham_profile_t m_intel{ };
        cham_profile_t m_npcs{ };

        usize m_viewmodel{ };
        usize m_viewmodel_overlay{ };
        usize m_weapon{ };
        usize m_weapon_overlay{ };

        color_t m_viewmodel_color{ 25, 200, 255 };
        color_t m_viewmodel_overlay_color{ 255, 255, 255 };
        color_t m_weapon_color{ 245, 190, 40 };
        color_t m_weapon_overlay_color{ 255, 255, 255 };

        usize m_local_model{ };
        usize m_enemy_model{ };

        cham_profile_t m_backtrack{ };
        usize          m_backtrack_records{ BACKTRACK_OLDEST };
        color_t        m_backtrack_color{ 245, 190, 40 };
        color_t        m_backtrack_overlay_color{ 255, 255, 255 };
    };

    struct glow_profile_t
    {
        i32     m_stencil{ };
        i32     m_blur{ };
        color_t m_color{ 220, 45, 40 };

        bool off( ) const
        {
            return this->m_stencil <= 0 && this->m_blur <= 0;
        }
    };

    struct glow_t
    {
        glow_profile_t m_players[ ESP_TARGET_COUNT ]{ { 0, 1, { 220, 45, 40 } },
                                                      { 0, 1, { 60, 140, 245 } },
                                                      { 0, 1, { 0x91, 0xC2, 0xD9, 0xFF } },
                                                      { 0, 1, { 100, 215, 110 } },
                                                      { 0, 1, { 75, 105, 215 } } };
        glow_profile_t m_buildings{ 0, 0, { 220, 45, 40 } };
        glow_profile_t m_projectiles{ 0, 0, { 245, 190, 40 } };
        glow_profile_t m_pickups{ 0, 0, { 120, 220, 120 } };
        glow_profile_t m_intel{ 0, 0, { 245, 190, 40 } };
        glow_profile_t m_npcs{ 0, 0, { 170, 90, 230 } };
        glow_profile_t m_backtrack{ 0, 0, { 245, 190, 40 } };
        glow_profile_t m_viewmodel{ 0, 0, { 25, 200, 255 } };
        glow_profile_t m_weapon{ 0, 0, { 245, 190, 40 } };
    };

    enum : usize
    {
        WEATHER_OFF  = 0,
        WEATHER_RAIN = 1,
        WEATHER_SNOW = 2,

        WEATHER_COUNT,
    };

    struct weather_t
    {
        usize m_type{ WEATHER_OFF };

        i32 m_density{ 100 };
        i32 m_wind_speed{ 0 };
        i32 m_wind_direction{ 0 };
    };

    enum : usize
    {
        TRACER_LINES     = 0,
        TRACER_BEAMS     = 1,
        TRACER_MERASMUS  = 2,
        TRACER_BIG_NASTY = 3,
        TRACER_RAYGUN    = 4,
        TRACER_MACHINA   = 5,
        TRACER_LIGHTNING = 6,

        TRACER_STYLE_COUNT,
    };

    enum : usize
    {
        IMPACT_STYLE_BOX    = 0,
        IMPACT_STYLE_CIRCLE = 1,

        IMPACT_STYLE_COUNT,
    };

    struct tracers_t
    {
        bool  m_enabled{ false };
        usize m_style{ TRACER_LINES };
        bool  m_local_only{ true };

        color_t m_local_color{ 239, 75, 72 };
        color_t m_local_impact_color{ 239, 75, 72 };
        color_t m_entity_color{ 60, 140, 245 };
        color_t m_entity_impact_color{ 60, 140, 245 };

        i32   m_duration{ 800 };
        bool  m_impact_marker{ true };
        usize m_impact_style{ IMPACT_STYLE_CIRCLE };

        bool drawn_by_us( ) const
        {
            return this->m_style == TRACER_LINES || this->m_style == TRACER_BEAMS;
        }
    };

    struct arrows_t
    {
        bool    m_enabled{ false };
        color_t m_color{ 239, 75, 72 };
        i32     m_radius{ 180 }; // px out from the crosshair
        i32     m_size{ 16 };    // px, half the chevron height
        bool    m_distance{ true };
    };

    struct viewmodel_t
    {
        bool m_enabled{ false };
        i32  m_forward{ };
        i32  m_right{ };
        i32  m_up{ };

        i32 m_bob{ 100 }; // percent of the walk bob the game asks for
        i32 m_sway{ 0 };  // percent of the turn sway the game ships switched off

        bool m_aim{ false };
        i32  m_aim_swing{ 120 }; // ms the model takes to swing onto the aim, and back off it

        bool m_show_in_scope{ false };
    };

    struct skybox_t
    {
        usize m_index{ }; // 0 leaves the map's own sky alone
    };

    struct world_t
    {
        bool    m_modulate{ false };
        color_t m_color{ 110, 120, 145 };
        bool    m_skybox{ true };
        bool    m_props{ true };
    };

    enum : usize
    {
        REMOVAL_VIEWPUNCH          = 0,
        REMOVAL_TEAM_PUSH          = 1,
        REMOVAL_POST_FX            = 2,
        REMOVAL_MOTION_BLUR        = 3,
        REMOVAL_OVERLAYS           = 4,
        REMOVAL_FOOTSTEPS          = 5,
        REMOVAL_DISGUISES          = 6,
        REMOVAL_TAUNTS             = 7,
        REMOVAL_SCOPE_ZOOM         = 8,
        REMOVAL_SCOPE_OVERLAY      = 9,
        REMOVAL_TAUNT_RESTRICTIONS = 10,
        REMOVAL_MOTD               = 11,
        REMOVAL_EQUIP_REGIONS      = 12,
        REMOVAL_KART_CAGE          = 13,
        REMOVAL_KART_RESTRICTIONS  = 14,
        REMOVAL_ITEM_NOTIFICATIONS = 15,

        REMOVAL_COUNT,
    };

    enum : usize
    {
        FOG_OFF    = 0,
        FOG_REMOVE = 1,
        FOG_CUSTOM = 2,

        FOG_COUNT,
    };

    struct fog_t
    {
        usize   m_mode{ FOG_OFF };
        i32     m_start{ 0 };
        i32     m_end{ 4000 };
        i32     m_density{ 100 };
        color_t m_color{ 150, 160, 172 };
        bool    m_skybox{ true };
    };

    struct post_process_t
    {
        bool m_enabled{ false };
        i32  m_exposure{ 0 };
        i32  m_contrast{ 110 };
        i32  m_saturation{ 125 };
        i32  m_vibrance{ 20 };
        i32  m_shadows{ -25 };
        i32  m_highlights{ 0 };
        i32  m_temperature{ -20 };
        i32  m_tint{ 0 };
        i32  m_vignette{ 0 };
        i32  m_bloom{ 30 };
        i32  m_bloom_threshold{ 60 };
        i32  m_motion_blur{ 0 };
    };

    enum : usize
    {
        NOISEMAKER_SPAM_ON_KILL = 0,
        NOISEMAKER_SPAM_ALWAYS  = 1,

        NOISEMAKER_SPAM_COUNT,
    };

    enum : usize
    {
        PROJECTILE_CAMERA_FOLLOW,
        PROJECTILE_CAMERA_IMPACT,
        PROJECTILE_CAMERA_LANDING,

        PROJECTILE_CAMERA_COUNT,
    };

    enum : usize
    {
        PROJECTILE_CAMERA_HUD_CROSSHAIR,
        PROJECTILE_CAMERA_HUD_BOMBER,

        PROJECTILE_CAMERA_HUD_COUNT,
    };

    struct misc_t
    {
        bool m_removals[ REMOVAL_COUNT ]{ };

        color_t m_accent{ 239, 75, 72 };
        color_t m_shadow{ 0, 0, 0, 72 };
        bool    m_menu_blur{ false };
        bool    m_snap_indicators{ false };
        bool    m_dock_editor{ true };
        point_t m_crit_indicator_position{ 12, 69 };
        point_t m_tick_indicator_position{ 12, 12 };
        usize   m_menu_particles{ gui::MENU_PARTICLES_HALLOWEEN };
        bool    m_menu_sounds{ true };
        i32     m_menu_sound_volume{ 100 };
        bool    m_draw_watermark{ true };
        i32     m_watermark_position{ ANCHOR_TOP_RIGHT };
        bool    m_spectator_list{ false };
        point_t m_spectator_list_position{ 12, 126 };
        point_t m_seed_indicator_position{ 12, 183 };
        bool    m_party_network{ false };
        bool    m_watchdog{ true };
        bool    m_watchdog_notify_party{ true };

        bool  m_noisemaker_spam{ false };
        usize m_noisemaker_spam_mode{ NOISEMAKER_SPAM_ALWAYS };

        bool m_bhop{ false };
        bool m_fast_stop{ false };

        bool   m_edge_jump{ false };
        bind_t m_edge_jump_key{ };

        bool   m_jump_bug{ false };
        bind_t m_jump_bug_key{ };
        bool   m_jump_bug_indicator{ false };

        bool   m_edge_bug{ false };
        bind_t m_edge_bug_key{ };

        bool   m_fan_jump{ false };
        bind_t m_fan_jump_key{ };

        bool m_taunt_spin{ false };
        i32  m_taunt_spin_speed{ 20 }; // dg per command, negative spins clockwise

        bool m_giant_weapon_sounds{ false };
        bool m_apply_to_alternate_weapons{ false };
        i32  m_weapon_pitch{ 100 }; // percent of the pitch each weapon sound asks for

        bool  m_auto_strafe{ false };
        usize m_auto_strafe_mode{ 1 }; // viewangles, wasd
        float m_auto_strafe_retrack{ 50.0f };

        bool m_fov_override{ false };
        i32  m_fov{ 100 };

        bool   m_thirdperson{ false };
        bind_t m_thirdperson_key{ VK_F5 };
        i32    m_thirdperson_distance{ 120 };
        i32    m_thirdperson_side{ 0 };
        i32    m_thirdperson_height{ 0 };
        bool   m_thirdperson_collision{ true };
        bool   m_thirdperson_smooth{ true };
        i32    m_thirdperson_smooth_time{ 200 };

        bool   m_freecam{ false };
        bind_t m_freecam_key{ };
        i32    m_freecam_speed{ 800 };

        bool    m_projectile_camera{ false };
        usize   m_projectile_camera_mode{ PROJECTILE_CAMERA_FOLLOW };
        usize   m_projectile_camera_hud{ PROJECTILE_CAMERA_HUD_CROSSHAIR };
        i32     m_projectile_camera_distance{ 150 };
        i32     m_projectile_camera_width{ 320 };
        point_t m_projectile_camera_position{ 12, 240 };

        bool m_aspect_ratio_changer{ false };
        f32  m_aspect_ratio_amt{ 1.2 };

        bool m_clean_screenshots{ true };
        bool m_clean_demos{ true };
    };

    enum : usize
    {
        // EU.
        QUEUE_REGION_NETHERLANDS = 0,
        QUEUE_REGION_GERMANY,
        QUEUE_REGION_UNITED_KINGDOM,
        QUEUE_REGION_FRANCE,
        QUEUE_REGION_SPAIN,
        QUEUE_REGION_SWEDEN,
        QUEUE_REGION_FINLAND,
        QUEUE_REGION_AUSTRIA,
        QUEUE_REGION_POLAND,

        // Asia.
        QUEUE_REGION_INDIA,
        QUEUE_REGION_UNITED_ARAB_EMIRATES,
        QUEUE_REGION_HONG_KONG,
        QUEUE_REGION_JAPAN,
        QUEUE_REGION_SOUTH_KOREA,
        QUEUE_REGION_SINGAPORE,
        QUEUE_REGION_GUAM,

        // North America.
        QUEUE_REGION_VIRGINIA,
        QUEUE_REGION_GEORGIA,
        QUEUE_REGION_ILLINOIS,
        QUEUE_REGION_TEXAS,
        QUEUE_REGION_CALIFORNIA,
        QUEUE_REGION_WASHINGTON,

        // South America.
        QUEUE_REGION_BRAZIL,
        QUEUE_REGION_PERU,
        QUEUE_REGION_CHILE,
        QUEUE_REGION_ARGENTINA,

        // Africa/Oceanic regions.
        QUEUE_REGION_SOUTH_AFRICA,
        QUEUE_REGION_AUSTRALIA,

        QUEUE_REGION_COUNT,
    };

    struct queue_t
    {
        constexpr queue_t( )
        {
            for ( usize region{ }; region < QUEUE_REGION_COUNT; ++region )
            {
                this->m_desired[ region ] = true;
            }
        }

        bool m_enabled{ false };
        i32  m_max_ping{ 200 };
        bool m_desired[ QUEUE_REGION_COUNT ]{ };
        bool m_delay_join{ false };
        i32  m_delay_join_time{ 60 };
    };

    enum : usize
    {
        RAPID_FIRE_ON_KEY  = 0,
        RAPID_FIRE_ON_FIRE = 1,

        RAPID_FIRE_COUNT,
    };

    struct exploits_t
    {
        bool m_seed_prediction{ true };
        bool m_seed_indicator{ false };

        bool   m_fake_latency{ false };
        bind_t m_fake_latency_key{ };
        i32    m_fake_latency_ms{ 200 };
        bool   m_extended_backtrack{ false };
        bool   m_ping_reducer{ false };
        i32    m_ping_reducer_cmdrate{ 1 };

        bool   m_rapid_fire{ false };
        bind_t m_rapid_fire_key{ VK_SHIFT };
        bool   m_anti_warp{ true };
        bool   m_indicator{ false };

        bool   m_warp{ false };
        bind_t m_warp_key{ VK_XBUTTON2 };
        i32    m_warp_ticks{ 4 };

        bool   m_speedhack{ false };
        bind_t m_speedhack_key{ VK_SHIFT };
        i32    m_speedhack_factor{ 5 };

        bind_t m_recharge_key{ 'R' };
        bool   m_passive_recharge{ false };
    };

    enum : usize
    {
        PATH_STYLE_LINE       = 0,
        PATH_STYLE_SEPARATORS = 1,
        PATH_STYLE_OUTLINED   = 2,
        PATH_STYLE_TIMELINE   = 3,
        PATH_STYLE_COMET      = 4,
        PATH_STYLE_FLOW       = 5,

        PATH_STYLE_COUNT,
    };

    enum : usize
    {
        PATH_BOUNDS_BOX     = 0,
        PATH_BOUNDS_CORNERS = 1,

        PATH_BOUNDS_COUNT,
    };

    struct projectile_path_t
    {
        bool    m_enabled{ false };
        usize   m_style{ PATH_STYLE_LINE };
        bool    m_outline{ false };
        f32     m_duration{ 2.0f };
        bool    m_animate{ false };
        color_t m_color{ 255, 255, 255, 200 };
        bool    m_impact_box{ false };
        color_t m_impact_color{ 235, 64, 52, 200 };
    };

    enum : usize
    {
        TRAJECTORY_STYLE_LINE,
        TRAJECTORY_STYLE_SEPARATORS,
        TRAJECTORY_STYLE_TIMELINE,
        TRAJECTORY_STYLE_COMET,
        TRAJECTORY_STYLE_FLOW,

        TRAJECTORY_STYLE_COUNT,
    };

    struct trajectory_t
    {
        bool    m_enabled{ false };
        usize   m_style{ TRAJECTORY_STYLE_TIMELINE };
        color_t m_color{ 255, 255, 255, 200 };
        bool    m_outline{ false };
        bool    m_gradient{ true };
        color_t m_gradient_color{ 239, 75, 72, 200 };
        bool    m_dim_occluded{ true };
        color_t m_marker_color{ 235, 64, 52, 200 };
        bool    m_splash{ true };
        bool    m_impact_box{ false };
        bool    m_flight_time{ true };
    };

    enum : usize
    {
        PROJECTILE_WARNING_BADGE,
        PROJECTILE_WARNING_TRAIL,
        PROJECTILE_WARNING_ZONE,
        PROJECTILE_WARNING_RADAR,
        PROJECTILE_WARNING_COMBINED,

        PROJECTILE_WARNING_STYLE_COUNT,
    };

    struct proj_warning_t
    {
        bool    m_enabled{ false };
        usize   m_style{ PROJECTILE_WARNING_COMBINED };
        color_t m_color{ 255, 138, 31 };
        color_t m_danger_color{ 255, 67, 67 };
        bool    m_only_threats{ false };
        bool    m_resting_stickies{ true };
    };

    enum : usize
    {
        TARGET_PLAYERS     = 0,
        TARGET_SENTRYGUNS  = 1,
        TARGET_DISPENSERS  = 2,
        TARGET_TELEPORTERS = 3,
        TARGET_PROJECTILES = 4,
        TARGET_ENTITIES    = 5,

        TARGET_COUNT,
    };

    enum : usize
    {
        IGNORE_CLOAKED     = 0,
        IGNORE_DISGUISED   = 1,
        IGNORE_FEIGNED     = 2,
        IGNORE_UBERED      = 3,
        IGNORE_BONKED      = 4,
        IGNORE_TAUNTING    = 5,
        IGNORE_VACCINATOR  = 6,
        IGNORE_FRIENDS     = 7,
        IGNORE_ATLAS_USERS = 8,
        IGNORE_PARTY       = 9,

        IGNORE_COUNT
    };

    enum : usize
    {
        PREFERENCE_AUTO_CHARGE         = 0,
        PREFERENCE_AUTO_REV            = 1,
        PREFERENCE_AUTO_SCOPE          = 2,
        PREFERENCE_NATASCHA_BONKED     = 3,
        PREFERENCE_FRIENDLY_FIRE       = 4,
        PREFERENCE_SNIPER_ONLY_ON_KILL = 5,
        PREFERENCE_TARGET_DORMANT      = 6,

        PREFERENCE_COUNT,
    };

    enum : usize
    {
        SPLASHBOT_INCLUDE = 0,
        SPLASHBOT_PREFER  = 1,

        SPLASHBOT_COUNT,
    };

    enum : usize
    {
        SPLASH_LIGHT,
        SPLASH_STANDARD,
        SPLASH_ULTRA,
        SPLASH_QUALITY_COUNT,
    };

    enum : usize
    {
        POSITION_ORIGINAL,
        POSITION_PREDICTED,
        POSITION_COUNT,
    };

    enum : usize
    {
        TARGET_CLASS_SCOUT    = 0,
        TARGET_CLASS_SOLDIER  = 1,
        TARGET_CLASS_PYRO     = 2,
        TARGET_CLASS_DEMOMAN  = 3,
        TARGET_CLASS_HEAVY    = 4,
        TARGET_CLASS_ENGINEER = 5,
        TARGET_CLASS_MEDIC    = 6,
        TARGET_CLASS_SNIPER   = 7,
        TARGET_CLASS_SPY      = 8,

        TARGET_CLASS_COUNT,
    };

    enum : usize
    {
        HITSCAN_BOX_PELVIS      = 0,
        HITSCAN_BOX_STOMACH     = 1,
        HITSCAN_BOX_LOWER_CHEST = 2,
        HITSCAN_BOX_CHEST       = 3,
        HITSCAN_BOX_UPPER_CHEST = 4,
        HITSCAN_BOX_HEAD        = 5,
        HITSCAN_BOX_ARMS        = 6,
        HITSCAN_BOX_LEGS        = 7,

        HITSCAN_COUNT
    };

    enum : usize
    {
        AIMPOINT_PRIORITY_PELVIS      = 0,
        AIMPOINT_PRIORITY_STOMACH     = 1,
        AIMPOINT_PRIORITY_LOWER_CHEST = 2,
        AIMPOINT_PRIORITY_CHEST       = 3,
        AIMPOINT_PRIORITY_UPPER_CHEST = 4,
        AIMPOINT_PRIORITY_HEAD        = 5,
        AIMPOINT_PRIORITY_AUTO        = 6,

        AIMPOINT_PRIORITY_COUNT,
    };

    struct ragebot_hitscan_profile_t
    {
        // These are configured per-class.
        usize m_aimpoint_priority{ AIMPOINT_PRIORITY_AUTO };
        bool  m_hitscan_boxes[ HITSCAN_COUNT ]{ true, true, true, true, true, true, false, false };

        bool m_hitscan_multipoint{ false };
        i32  m_hitscan_multipoint_scale{ 50 };
        i32  m_hitscan_hitchance{ 0 };
        bool m_hitscan_roll_angles{ false };

        bool m_use_history{ true };
    };

    enum : usize
    {
        BBOX_AIMPOINT_PRIORITY_TOP    = 0,
        BBOX_AIMPOINT_PRIORITY_MID    = 1,
        BBOX_AIMPOINT_PRIORITY_BOTTOM = 2,
        BBOX_AIMPOINT_PRIORITY_AUTO   = 3,

        BBOX_AIMPOINT_PRIORITY_COUNT,
    };

    enum : usize
    {
        PROJECTILE_BBOX_TOP    = 0,
        PROJECTILE_BBOX_MIDDLE = 1,
        PROJECTILE_BBOX_BOTTOM = 2,

        PROJECTILE_BBOX_COUNT,
    };

    struct ragebot_projectile_profile_t
    {
        usize m_aimpoint_priority{ BBOX_AIMPOINT_PRIORITY_AUTO }; // 0 = top, 1 = mid, 2 = bottom, 3 = auto
        bool  m_projectile_splashbot{ false };
        usize m_projectile_splashbot_mode{ SPLASHBOT_PREFER };

        // If the priority aimpoint fails, filter other bbox points here.
        bool m_projectile_aimpoints[ PROJECTILE_BBOX_COUNT ]{ true, true, true };

        i32   m_projectile_max_targets{ 3 };
        bool  m_projectile_auto_charge{ false };
        bool  m_projectile_roll_angles{ false };
        float m_projectile_max_sim_time{ 2.5f };
        i32   m_projectile_hitchance{ 0 };
        usize m_projectile_position_mode{ POSITION_ORIGINAL };
    };

    enum : usize
    {
        CHARGE_ON_STICKYBOMB  = 0,
        CHARGE_ON_SCOTTISH    = 1,
        CHARGE_ON_QUICKIEBOMB = 2,

        CHARGE_COUNT,
    };

    struct ragebot_t
    {
        // These are the global options. Do not move these.
        bool    m_enabled{ true };
        bind_t  m_activate_key{ VK_XBUTTON1 };
        i32     m_fov{ 180 };
        bool    m_draw_fov{ true };
        bool    m_auto_shoot{ true };
        color_t m_fov_color{ 255, 255, 255, 90 };
        bool    m_draw_crosshair{ false };
        bool    m_targets[ TARGET_COUNT ]{ true, true, true, true, true, true };
        bool    m_ignores[ IGNORE_COUNT ]{ true, false, true, true, true, true, true, true, true, true };
        bool    m_preferences[ PREFERENCE_COUNT ]{ true, true, true, true };
        bool    m_prevent_bait{ false };
        bind_t  m_bait_override_key{ VK_XBUTTON2 };
        usize   m_splash_quality{ SPLASH_LIGHT };
        bool    m_target_classes[ TARGET_CLASS_COUNT ]{ true, true, true, true, true, true, true, true, true };

        // TODO: Move me to visuals!
        bool    m_projectile_draw_path{ false };
        color_t m_projectile_path_color{ 120, 200, 255, 200 };
        color_t m_projectile_path_low_confidence_color{ 255, 160, 64, 200 };
        usize   m_projectile_path_style{ PATH_STYLE_LINE };
        bool    m_projectile_path_outline{ false };
        bool    m_projectile_path_fade{ true };
        float   m_projectile_path_fade_time{ 1.0f };
        bool    m_projectile_path_animate{ false };
        bool    m_projectile_path_gradient{ false };
        color_t m_projectile_path_color_end{ 235, 64, 52, 200 };
        bool    m_projectile_path_bounds{ false };
        usize   m_projectile_path_bounds_style{ PATH_BOUNDS_BOX };
        color_t m_projectile_path_bounds_color{ 255, 255, 255, 160 };
        bool    m_hitscan_draw_hitboxes{ false };
        color_t m_hitscan_hitbox_color{ 255, 255, 255, 140 };
        color_t m_hitscan_target_color{ 239, 75, 72, 220 };
        bool    m_hitscan_draw_shot{ false };
        color_t m_hitscan_shot_color{ 120, 200, 255, 200 };
        float   m_hitscan_draw_duration{ 2.0f };

        bool m_auto_charge_sticky_targets[ CHARGE_COUNT ]{ true, false, true };
        bool m_huntsman_wait_for_kill{ true };

        ragebot_hitscan_profile_t    m_hitscan_profiles[ CFG_PROFILE_COUNT ]{ };
        ragebot_projectile_profile_t m_projectile_profiles[ CFG_PROFILE_COUNT ]{ };
    };

    struct melee_t
    {
        bool   m_enabled{ false };
        bind_t m_activate_key{ VK_XBUTTON1 };
        bool   m_auto_swing{ true };
        bool   m_auto_backstab{ false };
        bool   m_swing_prediction{ true };
        bool   m_always_crit{ false };
    };

    struct rjump_t
    {
        bool   m_enabled{ false };
        bind_t m_key{ };
        usize  m_priority{ RJUMP_HEIGHT };
        bool   m_land_shot{ true };
        bool   m_pogo{ false };
        bool   m_pogo_crouch{ true };
    };

    struct lagcomp_t
    {
        bool m_draw_records{ false };
        bool m_extrapolate{ false };
    };

    struct resolver_t
    {
        bool m_enabled{ false };
    };

    struct crits_t
    {
        bool   m_enabled{ false };
        bind_t m_key{ };
        bool   m_force{ true };
        bool   m_avoid{ false };
        bool   m_indicator{ false };
#if defined( ATLAS_DEV )
        bool m_debug{ false };
#endif
    };

    struct pills_t
    {
        bool  m_seed_spin{ false };
        usize m_crit_priority{ 0 }; // keep the rotation fix, fire the crit
    };

    struct auto_det_t
    {
        bool   m_enabled{ false };
        bind_t m_key{ };
        i32    m_radius_scale{ 90 };
        i32    m_min_targets{ 1 };
        bool   m_avoid_self{ true };
        bool   m_target_buildings{ true };
        bool   m_dormant{ false };
    };

    struct air_blast_t
    {
        enum : usize
        {
            PUSH_OFF,
            PUSH_MELEE,
            PUSH_ALL,
            PUSH_COUNT
        };

        bool   m_enabled{ false };
        bind_t m_key{ };
        bool   m_aim{ true };
        bool   m_redirect{ false };
        bool   m_prefer_crits{ true };
        bool   m_extinguish{ false };
        usize  m_push{ PUSH_OFF };
    };

    struct auto_vacc_t
    {
        enum : usize
        {
            REACTIVITY_CONSERVATIVE,
            REACTIVITY_BALANCED,
            REACTIVITY_REACTIVE,
            REACTIVITY_COUNT,
        };

        bool   m_enabled{ false };
        bool   m_auto_pop{ true };
        usize  m_idle_resist{ 0 };
        usize  m_reactivity{ REACTIVITY_BALANCED };
        bind_t m_request_bullet_key{ };
        bind_t m_request_blast_key{ };
        bind_t m_request_fire_key{ };
    };

    static constexpr usize SKIN_WEAPON_COUNT{ 64 };
    static constexpr usize SKIN_WEARABLE_COUNT{ 32 };
    static constexpr usize SKIN_WEARABLE_SLOTS{ 4 };

    struct skin_t
    {
        usize m_model{ };
        usize m_paintkit{ };
        usize m_wear{ };
        i32   m_seed{ }; // 0 keeps the item's own pattern
        bool  m_australium{ };
        bool  m_festivized{ };
        usize m_unusual{ };
        usize m_killstreak{ };
        usize m_sheen{ };
        usize m_killstreaker{ };

        bool operator==( const skin_t &other ) const = default;

        bool off( ) const
        {
            return *this == skin_t{ };
        }
    };

    struct skin_entry_t
    {
        i32    m_definition{ -1 };
        skin_t m_skin{ };

        bool operator==( const skin_entry_t &other ) const = default;
    };

    struct wearable_skin_t
    {
        usize m_unusual{ };

        bool operator==( const wearable_skin_t &other ) const = default;
    };

    struct wearable_entry_t
    {
        i32             m_definition{ -1 };
        wearable_skin_t m_skin{ };

        bool operator==( const wearable_entry_t &other ) const = default;
    };

    struct spells_t
    {
        bool    m_pumpkin_bombs{ };
        bool    m_voices_from_below{ };
        bool    m_green_flames{ };
        usize   m_footprints{ };
        bool    m_footprint_custom{ };
        color_t m_footprint_color{ 245, 190, 40 };
        usize   m_jingle{ };
        bool    m_replace_step{ };
        bool    m_full_moon{ };

        bool halloween( ) const
        {
            return this->m_pumpkin_bombs || this->m_voices_from_below || this->m_green_flames ||
                   this->m_footprints != 0;
        }
    };

    enum : usize
    {
        RAGDOLL_TARGET_ENEMIES   = 0,
        RAGDOLL_TARGET_TEAMMATES = 1,
        RAGDOLL_TARGET_COUNT     = 2,
    };

    struct ragdolls_t
    {
        bool m_enabled{ };
        bool m_targets[ RAGDOLL_TARGET_COUNT ]{ true, false };
        bool m_gold{ };
        bool m_ice{ };
        bool m_dissolve{ };
        bool m_burning{ };
        bool m_electrocuted{ };
        bool m_ash{ };

        bool any( ) const
        {
            return this->m_gold || this->m_ice || this->m_dissolve || this->m_burning || this->m_electrocuted ||
                   this->m_ash;
        }
    };

    struct skins_t
    {
        bool m_enabled{ false };
        bool m_shared{ false };

        skin_t       m_weapon_edit{ };
        skin_entry_t m_weapons[ SKIN_WEAPON_COUNT ]{ };

        bool    m_sheen_custom{ };
        color_t m_sheen_color{ 245, 190, 40 };

        usize            m_wearable_slot{ };
        wearable_skin_t  m_wearable_edit{ };
        wearable_entry_t m_wearables[ SKIN_WEARABLE_COUNT ]{ };

        spells_t   m_spells{ };
        ragdolls_t m_ragdolls{ };
    };

    enum : usize
    {
        RADIO_INDICATOR_COMPACT  = 0,
        RADIO_INDICATOR_DETAILED = 1,

        RADIO_INDICATOR_COUNT,
    };

    struct radio_t
    {
        bool   m_enabled{ false };
        bind_t m_key{ };
        usize  m_station{ };
        i32    m_volume{ 50 };
        bool   m_scale_to_game_volume{ true };

        bool    m_indicator{ false };
        usize   m_indicator_style{ RADIO_INDICATOR_COMPACT };
        point_t m_indicator_position{ 12, 263 };

        bool m_duck_on_voice{ false };
        bool m_duck_on_self{ true };
        bool m_duck_on_others{ true };
        i32  m_duck_level{ 30 };

        bool m_mute_unfocused{ false };
        i32  m_crossfade{ 400 };

        bind_t m_next_key{ };
        bind_t m_previous_key{ };
        bind_t m_louder_key{ };
        bind_t m_quieter_key{ };
    };

    struct soundboard_t
    {
        bind_t m_key{ };
        bool   m_loop{ false };
        i32    m_volume{ 100 };
    };

    struct recorder_t
    {
        bool   m_enabled{ false };
        bind_t m_record_key{ };
        bind_t m_replay_key{ };
        bool   m_replay_attacks{ true };
        bool   m_camera_follows{ true };

        bool    m_markers{ true };
        color_t m_marker_color{ 239, 75, 72 };
        i32     m_marker_distance{ 3000 };
        bool    m_route{ true };
        color_t m_route_color{ 239, 75, 72 };
        color_t m_traveled_color{ 120, 122, 130 };
        usize   m_route_style{ PATH_STYLE_COMET };
        bool    m_key_overlay{ true };
        point_t m_key_overlay_position{ 12, 320 };
    };

#if defined( ATLAS_DEV )
    struct debug_t
    {
        bool    m_server_hitboxes{ false };
        color_t m_server_hitbox_color{ 72, 160, 255, 255 };
        bool    m_client_hitboxes{ false };
        color_t m_client_hitbox_color{ 239, 75, 72, 255 };
    };
#endif // ATLAS_DEV

    enum : usize
    {
        STREAMER_MODE_HIDE_LOCAL,
        STREAMER_MODE_HIDE_ENEMY,
        STREAMER_MODE_HIDE_TEAMMATES,
        STREAMER_MODE_HIDE_STEAM_FRIENDS,
        STREAMER_MODE_HIDE_PARTY_MEMBERS,
        STREAMER_MODE_HIDE_ATLAS_USERS,
        STREAMER_MODE_HIDE_SENSITIVE_PANELS,

        STREAMER_MODE_HIDE_COUNT,
    };

    struct streamer_mode_t
    {
        bool m_enabled{ false };
        bool m_hide_targets[ STREAMER_MODE_HIDE_COUNT ];
        bool m_stream_proof{ false };
    };

    enum : usize
    {
        NOTIF_EVENT_VOTE_START        = 0,
        NOTIF_EVENT_VOTE_CAST         = 1,
        NOTIF_EVENT_CLASS_CHANGE      = 2,
        NOTIF_EVENT_DAMAGE            = 3,
        NOTIF_EVENT_PLAYER_CONNECT    = 4,
        NOTIF_EVENT_PLAYER_DISCONNECT = 5,
        //
        // Insert new notification types here to avoid breaking existing configs!
        //

        NOTIF_EVENT_COUNT,
    };

    struct events_t
    {
        bool m_enabled{ false };
        bool m_notify_targets[ NOTIF_EVENT_COUNT ]{ };
        bool m_vote_start_notify_party{ false };
    };

    enum : u8
    {
        MEDBOT_HEAL_ALL       = 0,
        MEDBOT_PREFER_FRIENDS = 1,
        MEDBOT_ONLY_FRIENDS   = 2,
        //
        // Insert new entries here to avoid breaking existing configs!
        //

        MEDBOT_COUNT,
    };

    struct medibot_t
    {
        bool   m_enabled{ false };
        bind_t m_activate_key{ VK_XBUTTON1 };
        usize  m_prefer_mode{ MEDBOT_PREFER_FRIENDS };

        bool m_auto_uber{ false };
        bool m_auto_kritz{ false };
        // bool m_auto_mvm_medic_shield{ false };
    };

    enum : usize
    {
        YAW_MODE_NONE    = 0,
        YAW_MODE_AUTO    = 1,
        YAW_MODE_STATIC  = 2,
        YAW_MODE_CYLICAL = 3,
        YAW_MODE_SPIN    = 4,

        YAW_MODE_COUNT,
    };

    enum : usize
    {
        REAL_PITCH_UP     = 0,
        REAL_PITCH_DOWN   = 1,
        REAL_PITCH_ZERO   = 2,
        REAL_PITCH_JITTER = 3,

        REAL_PITCH_COUNT,
    };

    enum : usize
    {
        FAKE_PITCH_OFF    = 0,
        FAKE_PITCH_UP     = 1,
        FAKE_PITCH_DOWN   = 2,
        FAKE_PITCH_JITTER = 3,

        FAKE_PITCH_COUNT,
    };

    enum : usize
    {
        YAW_BASE_NONE      = 0,
        YAW_BASE_EYE       = 1,
        YAW_BASE_AT_TARGET = 2,

        YAW_BASE_COUNT,
    };

    struct anti_aim_profile_t
    {
        usize m_yaw_base{ YAW_BASE_EYE };

        usize m_real_yaw_mode{ YAW_MODE_AUTO };
        usize m_fake_yaw_mode{ YAW_MODE_AUTO };
        usize m_real_pitch_mode{ REAL_PITCH_UP };
        usize m_fake_pitch_mode{ FAKE_PITCH_OFF };

        i32 m_static_real_angle{ 90 };
        i32 m_static_fake_angle{ -90 };

        i32 m_cylical_pos_1_real{ 45 };
        i32 m_cylical_pos_2_real{ 135 };

        i32 m_cylical_pos_1_fake{ -45 };
        i32 m_cylical_pos_2_fake{ -135 };

        f32 m_real_cycle_time{ 0.5f };
        f32 m_fake_cycle_time{ 0.5f };

        bool m_apply_jitter_fake{ false };
        bool m_apply_jitter_real{ false };
        i32  m_jitter_amount{ 5 };

        i32 m_spin_amount{ 7 };

        bool m_break_pitch{ false };
    };

    enum : usize
    {
        FAKELAG_MODE_STATIC   = 0,
        FAKELAG_MODE_ADAPTIVE = 1,
        FAKELAG_MODE_RANDOM   = 2,

        FAKELAG_MODE_COUNT,
    };

    struct fakelag_profile_t
    {
        usize m_fakelag_mode{ FAKELAG_MODE_ADAPTIVE };

        i32 m_static_amount{ 6 };
        i32 m_random_min{ 4 };
        i32 m_random_max{ 10 };

        bool m_adaptive_randomized_choke{ false };
    };

    enum : usize
    {
        AA_MODE_LEGIT = 0,
        AA_MODE_RAGE  = 1,

        AA_MODE_COUNT,
    };

    enum : usize
    {
        DRAW_MODE_HIGHLIGHT_HEAD = 0,
        DRAW_MODE_ONLY_HEAD      = 1,
        DRAW_MODE_SOLID          = 2,

        DRAW_MODE_COUNT,
    };

    struct anti_hack_t
    {
        bool m_anti_aim_enabled{ false };
        bool m_fakelag_enabled{ false };

        bind_t m_anti_aim_key{ VK_CAPITAL };
        bind_t m_fakelag_key{ VK_LSHIFT };

        usize m_aa_mode{ AA_MODE_LEGIT };
        bool  m_headshot_threats_only{ false };
        bool  m_anti_backstab{ false };

        anti_aim_profile_t m_aa_profiles[ config::CFG_PROFILE_COUNT ]{ };
        fakelag_profile_t  m_fl_profiles[ config::CFG_PROFILE_COUNT ]{ };

        bool  m_draw_real_hitboxes{ false };
        bool  m_draw_fake_hitboxes{ false };
        usize m_real_draw_mode{ DRAW_MODE_ONLY_HEAD };
        usize m_fake_draw_mode{ DRAW_MODE_HIGHLIGHT_HEAD };

        color_t m_real_base_color{ 255, 255, 255, 192 };
        color_t m_real_highlight_color{ 233, 0, 0, 255 };

        color_t m_fake_base_color{ 255, 255, 255, 192 };
        color_t m_fake_highlight_color{ 0, 233, 0, 255 };

        usize          m_fake_chams{ };
        color_t        m_fake_chams_color{ 255, 70, 70, 128 };
        glow_profile_t m_fake_glow{ 0, 0, { 255, 70, 70 } };
    };

    enum : u8
    {
        PLAYER_TAG_NONE,
        PLAYER_TAG_FRIEND,
        PLAYER_TAG_IGNORED,
        PLAYER_TAG_CHEATER,
    };

    static constexpr usize PLAYER_GROUP_COUNT{ 16 };

    struct player_group_t
    {
        u8      m_name[ 17 ]{ };
        color_t m_color{ };
    };

    struct player_tag_t
    {
        u32 m_account{ };
        u8  m_tag{ };
        u8  m_alias[ 33 ]{ };
        u16 m_groups{ };
    };

    u8 player_tag( u32 account ) const
    {
        if ( !account )
        {
            return PLAYER_TAG_NONE;
        }

        for ( const player_tag_t &entry : this->m_player_tags )
        {
            if ( entry.m_account == account )
            {
                return entry.m_tag;
            }
        }

        return PLAYER_TAG_NONE;
    }

    const char *player_alias( u32 account ) const
    {
        if ( !account )
        {
            return nullptr;
        }

        for ( const player_tag_t &entry : this->m_player_tags )
        {
            if ( entry.m_account == account && entry.m_alias[ 0 ] )
            {
                return reinterpret_cast< const char * >( entry.m_alias );
            }
        }

        return nullptr;
    }

    u16 player_groups( u32 account ) const
    {
        if ( !account )
        {
            return 0;
        }

        for ( const player_tag_t &entry : this->m_player_tags )
        {
            if ( entry.m_account == account )
            {
                return entry.m_groups;
            }
        }

        return 0;
    }

    player_tag_t *claim_player_entry( u32 account )
    {
        player_tag_t *empty{ };
        for ( player_tag_t &entry : this->m_player_tags )
        {
            if ( entry.m_account == account )
            {
                return &entry;
            }

            if ( !empty && !entry.m_account )
            {
                empty = &entry;
            }
        }

        if ( empty )
        {
            *empty = player_tag_t{ account };
        }

        return empty;
    }

    static void release_player_entry( player_tag_t &entry )
    {
        if ( !entry.m_tag && !entry.m_alias[ 0 ] && !entry.m_groups )
        {
            entry.m_account = 0;
        }
    }

    bool set_player_alias( u32 account, string_view alias )
    {
        if ( !account || alias.size( ) > 32 )
        {
            return false;
        }

        player_tag_t *entry{ this->claim_player_entry( account ) };
        if ( !entry )
        {
            return alias.empty( );
        }

        for ( u8 &value : entry->m_alias )
        {
            value = 0;
        }
        str::copy( reinterpret_cast< char * >( entry->m_alias ),
                   sizeof( entry->m_alias ), alias.data( ), alias.size( ) );
        release_player_entry( *entry );
        return true;
    }

    bool set_player_tag( u32 account, u8 tag )
    {
        if ( !account || tag > PLAYER_TAG_CHEATER )
        {
            return false;
        }

        player_tag_t *entry{ this->claim_player_entry( account ) };
        if ( !entry )
        {
            return !tag;
        }

        entry->m_tag = tag;
        release_player_entry( *entry );
        return true;
    }

    bool set_player_groups( u32 account, u16 groups )
    {
        if ( !account )
        {
            return false;
        }

        player_tag_t *entry{ this->claim_player_entry( account ) };
        if ( !entry )
        {
            return !groups;
        }

        entry->m_groups = groups;
        release_player_entry( *entry );
        return true;
    }

    bool rename_player_group( usize index, string_view name )
    {
        player_group_t &group{ this->m_player_groups[ index ] };
        if ( name.empty( ) || name.size( ) >= sizeof( group.m_name ) )
        {
            return false;
        }

        for ( u8 &value : group.m_name )
        {
            value = 0;
        }
        str::copy( reinterpret_cast< char * >( group.m_name ), sizeof( group.m_name ),
                   name.data( ), name.size( ) );
        return true;
    }

    usize add_player_group( string_view name )
    {
        for ( usize index{ }; index < PLAYER_GROUP_COUNT; ++index )
        {
            if ( this->m_player_groups[ index ].m_name[ 0 ] )
            {
                continue;
            }

            return this->rename_player_group( index, name ) ? index : PLAYER_GROUP_COUNT;
        }

        return PLAYER_GROUP_COUNT;
    }

    void remove_player_group( usize index )
    {
        this->m_player_groups[ index ] = player_group_t{ };

        const u16 bit{ static_cast< u16 >( 1u << index ) };
        for ( player_tag_t &entry : this->m_player_tags )
        {
            if ( entry.m_groups & bit )
            {
                entry.m_groups = static_cast< u16 >( entry.m_groups & ~bit );
                release_player_entry( entry );
            }
        }
    }

    bool load( const char *name = "default" );
    bool save( const char *name = "default" );

    cham_profile_t &previewed_chams( )
    {
        switch ( static_cast< cham_entity_t >( this->m_esp.m_target_group ) )
        {
            case cham_entity_t::buildings: return this->m_chams.m_buildings;
            case cham_entity_t::world:
                return this->m_esp.m_edited == WORLD_PICKUPS ? this->m_chams.m_pickups
                       : this->m_esp.m_edited == WORLD_INTEL ? this->m_chams.m_intel
                       : this->m_esp.m_edited == WORLD_NPCS  ? this->m_chams.m_npcs
                                                             : this->m_chams.m_projectiles;
            default: return this->m_esp.edited( ).m_chams;
        }
    }

    glow_profile_t &previewed_glow( )
    {
        switch ( static_cast< cham_entity_t >( this->m_esp.m_target_group ) )
        {
            case cham_entity_t::buildings: return this->m_glow.m_buildings;
            case cham_entity_t::world:
                return this->m_esp.m_edited == WORLD_PICKUPS ? this->m_glow.m_pickups
                       : this->m_esp.m_edited == WORLD_INTEL ? this->m_glow.m_intel
                       : this->m_esp.m_edited == WORLD_NPCS  ? this->m_glow.m_npcs
                                                             : this->m_glow.m_projectiles;
            default: return this->m_glow.m_players[ this->m_esp.m_edited ];
        }
    }

    esp_t             m_esp{ };
    hitmarker_t       m_hitmarker{ };
    chams_t           m_chams{ };
    glow_t            m_glow{ };
    weather_t         m_weather{ };
    tracers_t         m_tracers{ };
    arrows_t          m_arrows{ };
    skybox_t          m_skybox{ };
    world_t           m_world{ };
    fog_t             m_fog{ };
    post_process_t    m_post_process{ };
    viewmodel_t       m_viewmodel{ };
    misc_t            m_misc{ };
    queue_t           m_queue{ };
    exploits_t        m_exploits{ };
    ragebot_t         m_ragebot{ };
    melee_t           m_melee{ };
    rjump_t           m_rjump{ };
    lagcomp_t         m_lagcomp{ };
    resolver_t        m_resolver{ };
    crits_t           m_crits{ };
    pills_t           m_pills{ };
    auto_vacc_t       m_auto_vacc{ };
    auto_det_t        m_auto_det{ };
    air_blast_t       m_air_blast{ };
    skins_t           m_skins{ };
    radio_t           m_radio{ };
    soundboard_t      m_soundboard{ };
    events_t          m_events{ };
    medibot_t         m_medibot{ };
    anti_hack_t       m_anti_hack{ };
    player_tag_t      m_player_tags[ 128 ]{ };
    player_group_t    m_player_groups[ PLAYER_GROUP_COUNT ]{ };
    streamer_mode_t   m_streamer_mode{ };
    projectile_path_t m_projectile_path{ };
    trajectory_t      m_trajectory{ };
    proj_warning_t    m_projectile_warning{ };
    recorder_t        m_recorder{ };
#if defined( ATLAS_DEV )
    debug_t m_debug{ };
#endif // ATLAS_DEV
};

extern config g_cfg;

bool valid_config( config &value );
