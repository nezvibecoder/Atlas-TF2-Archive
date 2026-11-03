template< typename value_type >
inline constexpr i64 config_enum_maximum{ json::detail::is_same< value_type, config::esp_anchor_t > ? 3 : 2 };

inline constexpr u32 CONFIG_BIND_KEY_MAXIMUM{ 255 };

template< typename visitor >
static void config_fields( color_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "r" ).get( ), value.r );
    field( ATLAS_XORSTR( "g" ).get( ), value.g );
    field( ATLAS_XORSTR( "b" ).get( ), value.b );
    field( ATLAS_XORSTR( "a" ).get( ), value.a );
}

template< typename visitor >
static void config_fields( point_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "x" ).get( ), value.x );
    field( ATLAS_XORSTR( "y" ).get( ), value.y );
}

template< typename visitor >
static void config_fields( bind_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "key" ).get( ), value.m_key );
    field( ATLAS_XORSTR( "mode" ).get( ), value.m_mode );
}

template< typename visitor >
static void config_fields( config::cham_profile_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "visible" ).get( ), value.m_visible );
    field( ATLAS_XORSTR( "visible_overlay" ).get( ), value.m_visible_overlay );
    field( ATLAS_XORSTR( "invisible" ).get( ), value.m_invisible );
    field( ATLAS_XORSTR( "invisible_overlay" ).get( ), value.m_invisible_overlay );
    field( ATLAS_XORSTR( "visible_color" ).get( ), value.m_visible_color );
    field( ATLAS_XORSTR( "occluded_color" ).get( ), value.m_occluded_color );
    field( ATLAS_XORSTR( "visible_overlay_color" ).get( ), value.m_visible_overlay_color );
    field( ATLAS_XORSTR( "occluded_overlay_color" ).get( ), value.m_occluded_overlay_color );
}

template< typename visitor >
static void config_fields( config::esp_profile_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "elements" ).get( ), value.m_elements );
    field( ATLAS_XORSTR( "chams" ).get( ), value.m_chams );
    field( ATLAS_XORSTR( "order" ).get( ), value.m_order );
    field( ATLAS_XORSTR( "anchors" ).get( ), value.m_anchors );
    field( ATLAS_XORSTR( "aligns" ).get( ), value.m_aligns );
    field( ATLAS_XORSTR( "box_color" ).get( ), value.m_box_color );
    field( ATLAS_XORSTR( "name_color" ).get( ), value.m_name_color );
    field( ATLAS_XORSTR( "distance_color" ).get( ), value.m_distance_color );
    field( ATLAS_XORSTR( "health_style" ).get( ), value.m_health_style );
    field( ATLAS_XORSTR( "health_color" ).get( ), value.m_health_color );
    field( ATLAS_XORSTR( "health_low_color" ).get( ), value.m_health_low_color );
    field( ATLAS_XORSTR( "overheal_color" ).get( ), value.m_overheal_color );
    field( ATLAS_XORSTR( "health_divisions" ).get( ), value.m_health_divisions );
    field( ATLAS_XORSTR( "weapon_color" ).get( ), value.m_weapon_color );
    field( ATLAS_XORSTR( "skeleton_color" ).get( ), value.m_skeleton_color );
}

template< typename visitor >
static void config_fields( config::esp_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "profiles" ).get( ), value.m_profiles );
    field( ATLAS_XORSTR( "target_group" ).get( ), value.m_target_group );
    field( ATLAS_XORSTR( "edited" ).get( ), value.m_edited );
    field( ATLAS_XORSTR( "dormant_time" ).get( ), value.m_dormant_time );
    field( ATLAS_XORSTR( "avatars" ).get( ), value.m_avatars );
    field( ATLAS_XORSTR( "target_style" ).get( ), value.m_target_style );
    field( ATLAS_XORSTR( "target_chevron" ).get( ), value.m_target_chevron );
    field( ATLAS_XORSTR( "target_animate" ).get( ), value.m_target_animate );
    field( ATLAS_XORSTR( "target_color" ).get( ), value.m_target_color );
    field( ATLAS_XORSTR( "building_elements" ).get( ), value.m_building_elements );
    field( ATLAS_XORSTR( "projectile_elements" ).get( ), value.m_projectile_elements );
    field( ATLAS_XORSTR( "pickup_elements" ).get( ), value.m_pickup_elements );
    field( ATLAS_XORSTR( "intel_elements" ).get( ), value.m_intel_elements );
    field( ATLAS_XORSTR( "npc_elements" ).get( ), value.m_npc_elements );
    field( ATLAS_XORSTR( "projectile_teams" ).get( ), value.m_projectile_teams );
    field( ATLAS_XORSTR( "draw_teammates_conditionally" ).get( ), value.m_draw_teammates_conditionally );
}

template< typename visitor >
static void config_fields( config::hitmarker_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "show_damage" ).get( ), value.m_show_damage );
    field( ATLAS_XORSTR( "render_mode" ).get( ), value.m_render_mode );
    field( ATLAS_XORSTR( "color" ).get( ), value.m_color );
    field( ATLAS_XORSTR( "edited_event" ).get( ), value.m_edited_event );
    field( ATLAS_XORSTR( "override_color" ).get( ), value.m_override_color );
    field( ATLAS_XORSTR( "event_color" ).get( ), value.m_event_color );
}

template< typename visitor >
static void config_fields( config::chams_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "buildings" ).get( ), value.m_buildings );
    field( ATLAS_XORSTR( "projectiles" ).get( ), value.m_projectiles );
    field( ATLAS_XORSTR( "pickups" ).get( ), value.m_pickups );
    field( ATLAS_XORSTR( "intel" ).get( ), value.m_intel );
    field( ATLAS_XORSTR( "npcs" ).get( ), value.m_npcs );
    field( ATLAS_XORSTR( "viewmodel" ).get( ), value.m_viewmodel );
    field( ATLAS_XORSTR( "viewmodel_overlay" ).get( ), value.m_viewmodel_overlay );
    field( ATLAS_XORSTR( "weapon" ).get( ), value.m_weapon );
    field( ATLAS_XORSTR( "weapon_overlay" ).get( ), value.m_weapon_overlay );
    field( ATLAS_XORSTR( "viewmodel_color" ).get( ), value.m_viewmodel_color );
    field( ATLAS_XORSTR( "viewmodel_overlay_color" ).get( ), value.m_viewmodel_overlay_color );
    field( ATLAS_XORSTR( "weapon_color" ).get( ), value.m_weapon_color );
    field( ATLAS_XORSTR( "weapon_overlay_color" ).get( ), value.m_weapon_overlay_color );
    field( ATLAS_XORSTR( "local_model" ).get( ), value.m_local_model );
    field( ATLAS_XORSTR( "enemy_model" ).get( ), value.m_enemy_model );
    field( ATLAS_XORSTR( "backtrack" ).get( ), value.m_backtrack );
    field( ATLAS_XORSTR( "backtrack_records" ).get( ), value.m_backtrack_records );
    field( ATLAS_XORSTR( "backtrack_color" ).get( ), value.m_backtrack_color );
    field( ATLAS_XORSTR( "backtrack_overlay_color" ).get( ), value.m_backtrack_overlay_color );
}

template< typename visitor >
static void config_fields( config::glow_profile_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "stencil" ).get( ), value.m_stencil );
    field( ATLAS_XORSTR( "blur" ).get( ), value.m_blur );
    field( ATLAS_XORSTR( "color" ).get( ), value.m_color );
}

template< typename visitor >
static void config_fields( config::glow_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "players" ).get( ), value.m_players );
    field( ATLAS_XORSTR( "buildings" ).get( ), value.m_buildings );
    field( ATLAS_XORSTR( "projectiles" ).get( ), value.m_projectiles );
    field( ATLAS_XORSTR( "pickups" ).get( ), value.m_pickups );
    field( ATLAS_XORSTR( "intel" ).get( ), value.m_intel );
    field( ATLAS_XORSTR( "npcs" ).get( ), value.m_npcs );
    field( ATLAS_XORSTR( "backtrack" ).get( ), value.m_backtrack );
    field( ATLAS_XORSTR( "viewmodel" ).get( ), value.m_viewmodel );
    field( ATLAS_XORSTR( "weapon" ).get( ), value.m_weapon );
}

template< typename visitor >
static void config_fields( config::weather_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "type" ).get( ), value.m_type );
    field( ATLAS_XORSTR( "density" ).get( ), value.m_density );
    field( ATLAS_XORSTR( "wind_speed" ).get( ), value.m_wind_speed );
    field( ATLAS_XORSTR( "wind_direction" ).get( ), value.m_wind_direction );
}

template< typename visitor >
static void config_fields( config::tracers_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "style" ).get( ), value.m_style );
    field( ATLAS_XORSTR( "local_only" ).get( ), value.m_local_only );
    field( ATLAS_XORSTR( "local_color" ).get( ), value.m_local_color );
    field( ATLAS_XORSTR( "local_impact_color" ).get( ), value.m_local_impact_color );
    field( ATLAS_XORSTR( "entity_color" ).get( ), value.m_entity_color );
    field( ATLAS_XORSTR( "entity_impact_color" ).get( ), value.m_entity_impact_color );
    field( ATLAS_XORSTR( "duration" ).get( ), value.m_duration );
    field( ATLAS_XORSTR( "impact_marker" ).get( ), value.m_impact_marker );
    field( ATLAS_XORSTR( "impact_style" ).get( ), value.m_impact_style );
}

template< typename visitor >
static void config_fields( config::arrows_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "color" ).get( ), value.m_color );
    field( ATLAS_XORSTR( "radius" ).get( ), value.m_radius );
    field( ATLAS_XORSTR( "size" ).get( ), value.m_size );
    field( ATLAS_XORSTR( "distance" ).get( ), value.m_distance );
}

template< typename visitor >
static void config_fields( config::viewmodel_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "forward" ).get( ), value.m_forward );
    field( ATLAS_XORSTR( "right" ).get( ), value.m_right );
    field( ATLAS_XORSTR( "up" ).get( ), value.m_up );
    field( ATLAS_XORSTR( "bob" ).get( ), value.m_bob );
    field( ATLAS_XORSTR( "sway" ).get( ), value.m_sway );
    field( ATLAS_XORSTR( "aim" ).get( ), value.m_aim );
    field( ATLAS_XORSTR( "aim_swing" ).get( ), value.m_aim_swing );
}

template< typename visitor >
static void config_fields( config::skybox_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "index" ).get( ), value.m_index );
}

template< typename visitor >
static void config_fields( config::world_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "modulate" ).get( ), value.m_modulate );
    field( ATLAS_XORSTR( "color" ).get( ), value.m_color );
    field( ATLAS_XORSTR( "skybox" ).get( ), value.m_skybox );
    field( ATLAS_XORSTR( "props" ).get( ), value.m_props );
}

template< typename visitor >
static void config_fields( config::fog_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "mode" ).get( ), value.m_mode );
    field( ATLAS_XORSTR( "start" ).get( ), value.m_start );
    field( ATLAS_XORSTR( "end" ).get( ), value.m_end );
    field( ATLAS_XORSTR( "density" ).get( ), value.m_density );
    field( ATLAS_XORSTR( "color" ).get( ), value.m_color );
    field( ATLAS_XORSTR( "skybox" ).get( ), value.m_skybox );
}

template< typename visitor >
static void config_fields( config::post_process_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "exposure" ).get( ), value.m_exposure );
    field( ATLAS_XORSTR( "contrast" ).get( ), value.m_contrast );
    field( ATLAS_XORSTR( "saturation" ).get( ), value.m_saturation );
    field( ATLAS_XORSTR( "vibrance" ).get( ), value.m_vibrance );
    field( ATLAS_XORSTR( "shadows" ).get( ), value.m_shadows );
    field( ATLAS_XORSTR( "highlights" ).get( ), value.m_highlights );
    field( ATLAS_XORSTR( "temperature" ).get( ), value.m_temperature );
    field( ATLAS_XORSTR( "tint" ).get( ), value.m_tint );
    field( ATLAS_XORSTR( "vignette" ).get( ), value.m_vignette );
    field( ATLAS_XORSTR( "bloom" ).get( ), value.m_bloom );
    field( ATLAS_XORSTR( "bloom_threshold" ).get( ), value.m_bloom_threshold );
    field( ATLAS_XORSTR( "motion_blur" ).get( ), value.m_motion_blur );
}

template< typename visitor >
static void config_fields( config::misc_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "removals" ).get( ), value.m_removals );
    field( ATLAS_XORSTR( "accent" ).get( ), value.m_accent );
    field( ATLAS_XORSTR( "shadow" ).get( ), value.m_shadow );
    field( ATLAS_XORSTR( "menu_blur" ).get( ), value.m_menu_blur );
    field( ATLAS_XORSTR( "snap_indicators" ).get( ), value.m_snap_indicators );
    field( ATLAS_XORSTR( "dock_editor" ).get( ), value.m_dock_editor );
    field( ATLAS_XORSTR( "crit_indicator_pos" ).get( ), value.m_crit_indicator_position );
    field( ATLAS_XORSTR( "tick_indicator_pos" ).get( ), value.m_tick_indicator_position );
    field( ATLAS_XORSTR( "menu_particles" ).get( ), value.m_menu_particles );
    field( ATLAS_XORSTR( "menu_sounds" ).get( ), value.m_menu_sounds );
    field( ATLAS_XORSTR( "menu_sound_volume" ).get( ), value.m_menu_sound_volume );
    field( ATLAS_XORSTR( "draw_watermark" ).get( ), value.m_draw_watermark );
    field( ATLAS_XORSTR( "spectator_list" ).get( ), value.m_spectator_list );
    field( ATLAS_XORSTR( "spectator_list_position" ).get( ), value.m_spectator_list_position );
    field( ATLAS_XORSTR( "seed_indicator_pos" ).get( ), value.m_seed_indicator_position );
    field( ATLAS_XORSTR( "party_network" ).get( ), value.m_party_network );
    field( ATLAS_XORSTR( "watchdog" ).get( ), value.m_watchdog );
    field( ATLAS_XORSTR( "watchdog_post_to_party" ).get( ), value.m_watchdog_notify_party );
    field( ATLAS_XORSTR( "watermark_pos" ).get( ), value.m_watermark_position );
    field( ATLAS_XORSTR( "bhop" ).get( ), value.m_bhop );
    field( ATLAS_XORSTR( "fast_stop" ).get( ), value.m_fast_stop );
    field( ATLAS_XORSTR( "edge_jump" ).get( ), value.m_edge_jump );
    field( ATLAS_XORSTR( "edge_jump_key" ).get( ), value.m_edge_jump_key );
    field( ATLAS_XORSTR( "jump_bug" ).get( ), value.m_jump_bug );
    field( ATLAS_XORSTR( "jump_bug_key" ).get( ), value.m_jump_bug_key );
    field( ATLAS_XORSTR( "jump_bug_indicator" ).get( ), value.m_jump_bug_indicator );
    field( ATLAS_XORSTR( "edge_bug" ).get( ), value.m_edge_bug );
    field( ATLAS_XORSTR( "edge_bug_key" ).get( ), value.m_edge_bug_key );
    field( ATLAS_XORSTR( "fan_jump" ).get( ), value.m_fan_jump );
    field( ATLAS_XORSTR( "fan_jump_key" ).get( ), value.m_fan_jump_key );
    field( ATLAS_XORSTR( "taunt_spin" ).get( ), value.m_taunt_spin );
    field( ATLAS_XORSTR( "taunt_spin_speed" ).get( ), value.m_taunt_spin_speed );
    field( ATLAS_XORSTR( "giant_weapon_sounds" ).get( ), value.m_giant_weapon_sounds );
    field( ATLAS_XORSTR( "apply_to_alternative" ).get( ), value.m_apply_to_alternate_weapons );
    field( ATLAS_XORSTR( "weapon_pitch" ).get( ), value.m_weapon_pitch );
    field( ATLAS_XORSTR( "auto_strafe" ).get( ), value.m_auto_strafe );
    field( ATLAS_XORSTR( "auto_strafe_mode" ).get( ), value.m_auto_strafe_mode );
    field( ATLAS_XORSTR( "auto_strafe_retrack" ).get( ), value.m_auto_strafe_retrack );
    field( ATLAS_XORSTR( "fov_override" ).get( ), value.m_fov_override );
    field( ATLAS_XORSTR( "fov" ).get( ), value.m_fov );
    field( ATLAS_XORSTR( "thirdperson" ).get( ), value.m_thirdperson );
    field( ATLAS_XORSTR( "thirdperson_key" ).get( ), value.m_thirdperson_key );
    field( ATLAS_XORSTR( "thirdperson_distance" ).get( ), value.m_thirdperson_distance );
    field( ATLAS_XORSTR( "thirdperson_side" ).get( ), value.m_thirdperson_side );
    field( ATLAS_XORSTR( "thirdperson_height" ).get( ), value.m_thirdperson_height );
    field( ATLAS_XORSTR( "thirdperson_collision" ).get( ), value.m_thirdperson_collision );
    field( ATLAS_XORSTR( "thirdperson_smooth" ).get( ), value.m_thirdperson_smooth );
    field( ATLAS_XORSTR( "thirdperson_smooth_time" ).get( ), value.m_thirdperson_smooth_time );
    field( ATLAS_XORSTR( "freecam" ).get( ), value.m_freecam );
    field( ATLAS_XORSTR( "freecam_key" ).get( ), value.m_freecam_key );
    field( ATLAS_XORSTR( "freecam_speed" ).get( ), value.m_freecam_speed );
    field( ATLAS_XORSTR( "projectile_camera" ).get( ), value.m_projectile_camera );
    field( ATLAS_XORSTR( "projectile_camera_mode" ).get( ), value.m_projectile_camera_mode );
    field( ATLAS_XORSTR( "projectile_camera_hud" ).get( ), value.m_projectile_camera_hud );
    field( ATLAS_XORSTR( "projectile_camera_distance" ).get( ), value.m_projectile_camera_distance );
    field( ATLAS_XORSTR( "projectile_camera_width" ).get( ), value.m_projectile_camera_width );
    field( ATLAS_XORSTR( "projectile_camera_pos" ).get( ), value.m_projectile_camera_position );
    field( ATLAS_XORSTR( "aspect_ratio_changer" ).get( ), value.m_aspect_ratio_changer );
    field( ATLAS_XORSTR( "aspect_ratio_amt" ).get( ), value.m_aspect_ratio_amt );
    field( ATLAS_XORSTR( "noisemaker_spam" ).get( ), value.m_noisemaker_spam );
    field( ATLAS_XORSTR( "noisemaker_spam_mode" ).get( ), value.m_noisemaker_spam_mode );
    field( ATLAS_XORSTR( "clean_screenshots" ).get( ), value.m_clean_screenshots );
    field( ATLAS_XORSTR( "clean_demos" ).get( ), value.m_clean_demos );
}

template< typename visitor >
static void config_fields( config::queue_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "max_ping" ).get( ), value.m_max_ping );
    field( ATLAS_XORSTR( "desired" ).get( ), value.m_desired );
    field( ATLAS_XORSTR( "delay_join" ).get( ), value.m_delay_join );
    field( ATLAS_XORSTR( "delay_join_time" ).get( ), value.m_delay_join_time );
}

template< typename visitor >
static void config_fields( config::exploits_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "seed_prediction" ).get( ), value.m_seed_prediction );
    field( ATLAS_XORSTR( "seed_indicator" ).get( ), value.m_seed_indicator );
    field( ATLAS_XORSTR( "fake_latency" ).get( ), value.m_fake_latency );
    field( ATLAS_XORSTR( "fake_latency_key" ).get( ), value.m_fake_latency_key );
    field( ATLAS_XORSTR( "fake_latency_ms" ).get( ), value.m_fake_latency_ms );
    field( ATLAS_XORSTR( "ping_reducer" ).get( ), value.m_ping_reducer );
    field( ATLAS_XORSTR( "ping_reducer_cmdrate" ).get( ), value.m_ping_reducer_cmdrate );
    field( ATLAS_XORSTR( "rapid_fire" ).get( ), value.m_rapid_fire );
    field( ATLAS_XORSTR( "rapid_fire_key" ).get( ), value.m_rapid_fire_key );
    field( ATLAS_XORSTR( "anti_warp" ).get( ), value.m_anti_warp );
    field( ATLAS_XORSTR( "indicator" ).get( ), value.m_indicator );
    field( ATLAS_XORSTR( "warp" ).get( ), value.m_warp );
    field( ATLAS_XORSTR( "warp_key" ).get( ), value.m_warp_key );
    field( ATLAS_XORSTR( "warp_ticks" ).get( ), value.m_warp_ticks );
    field( ATLAS_XORSTR( "recharge_key" ).get( ), value.m_recharge_key );
    field( ATLAS_XORSTR( "passive_recharge" ).get( ), value.m_passive_recharge );
}

template< typename visitor >
static void config_fields( config::projectile_path_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "style" ).get( ), value.m_style );
    field( ATLAS_XORSTR( "outline" ).get( ), value.m_outline );
    field( ATLAS_XORSTR( "duration" ).get( ), value.m_duration );
    field( ATLAS_XORSTR( "animate" ).get( ), value.m_animate );
    field( ATLAS_XORSTR( "color" ).get( ), value.m_color );
    field( ATLAS_XORSTR( "impact_box" ).get( ), value.m_impact_box );
    field( ATLAS_XORSTR( "impact_color" ).get( ), value.m_impact_color );
}

template< typename visitor >
static void config_fields( config::trajectory_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "style" ).get( ), value.m_style );
    field( ATLAS_XORSTR( "color" ).get( ), value.m_color );
    field( ATLAS_XORSTR( "outline" ).get( ), value.m_outline );
    field( ATLAS_XORSTR( "gradient" ).get( ), value.m_gradient );
    field( ATLAS_XORSTR( "gradient_color" ).get( ), value.m_gradient_color );
    field( ATLAS_XORSTR( "dim_occluded" ).get( ), value.m_dim_occluded );
    field( ATLAS_XORSTR( "marker_color" ).get( ), value.m_marker_color );
    field( ATLAS_XORSTR( "splash" ).get( ), value.m_splash );
    field( ATLAS_XORSTR( "impact_box" ).get( ), value.m_impact_box );
    field( ATLAS_XORSTR( "flight_time" ).get( ), value.m_flight_time );
}

template< typename visitor >
static void config_fields( config::proj_warning_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "style" ).get( ), value.m_style );
    field( ATLAS_XORSTR( "color" ).get( ), value.m_color );
    field( ATLAS_XORSTR( "danger_color" ).get( ), value.m_danger_color );
    field( ATLAS_XORSTR( "only_threats" ).get( ), value.m_only_threats );
    field( ATLAS_XORSTR( "resting_stickies" ).get( ), value.m_resting_stickies );
}

template< typename visitor >
static void config_fields( config::ragebot_hitscan_profile_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "hitscan_aimpoint_priority" ).get( ), value.m_aimpoint_priority );
    field( ATLAS_XORSTR( "hitscan_boxes" ).get( ), value.m_hitscan_boxes );
    field( ATLAS_XORSTR( "hitscan_multipoint" ).get( ), value.m_hitscan_multipoint );
    field( ATLAS_XORSTR( "hitscan_multipoint_scale" ).get( ), value.m_hitscan_multipoint_scale );
    field( ATLAS_XORSTR( "hitscan_hitchance" ).get( ), value.m_hitscan_hitchance );
    field( ATLAS_XORSTR( "hitscan_roll_angle" ).get( ), value.m_hitscan_roll_angles );
    field( ATLAS_XORSTR( "use_history" ).get( ), value.m_use_history );
}

template< typename visitor >
static void config_fields( config::ragebot_projectile_profile_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "projectile_aimpoint_priority" ).get( ), value.m_aimpoint_priority );
    field( ATLAS_XORSTR( "projectile_splashbot" ).get( ), value.m_projectile_splashbot );
    field( ATLAS_XORSTR( "projectile_splashbot_mode" ).get( ), value.m_projectile_splashbot_mode );
    field( ATLAS_XORSTR( "projectile_aimpoints" ).get( ), value.m_projectile_aimpoints );
    field( ATLAS_XORSTR( "projectile_max_targets" ).get( ), value.m_projectile_max_targets );
    field( ATLAS_XORSTR( "projectile_auto_charge" ).get( ), value.m_projectile_auto_charge );
    field( ATLAS_XORSTR( "projectile_roll_angle" ).get( ), value.m_projectile_roll_angles );
    field( ATLAS_XORSTR( "projectile_max_sim_time" ).get( ), value.m_projectile_max_sim_time );
    field( ATLAS_XORSTR( "projectile_hitchance" ).get( ), value.m_projectile_hitchance );
    field( ATLAS_XORSTR( "projectile_position_mode" ).get( ), value.m_projectile_position_mode );
}

template< typename visitor >
static void config_fields( config::ragebot_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "activate_key" ).get( ), value.m_activate_key );
    field( ATLAS_XORSTR( "fov" ).get( ), value.m_fov );
    field( ATLAS_XORSTR( "draw_fov" ).get( ), value.m_draw_fov );
    field( ATLAS_XORSTR( "fov_color" ).get( ), value.m_fov_color );
    field( ATLAS_XORSTR( "draw_crosshair" ).get( ), value.m_draw_crosshair );
    field( ATLAS_XORSTR( "auto_shoot" ).get( ), value.m_auto_shoot );
    field( ATLAS_XORSTR( "ragebot_targets" ).get( ), value.m_targets );
    field( ATLAS_XORSTR( "ragebot_ignores" ).get( ), value.m_ignores );
    field( ATLAS_XORSTR( "ragebot_preferences" ).get( ), value.m_preferences );
    field( ATLAS_XORSTR( "prevent_bait" ).get( ), value.m_prevent_bait );
    field( ATLAS_XORSTR( "bait_override_key" ).get( ), value.m_bait_override_key );
    field( ATLAS_XORSTR( "splash_quality" ).get( ), value.m_splash_quality );
    field( ATLAS_XORSTR( "target_classes" ).get( ), value.m_target_classes );

    field( ATLAS_XORSTR( "projectile_draw_path" ).get( ), value.m_projectile_draw_path );
    field( ATLAS_XORSTR( "projectile_path_color" ).get( ), value.m_projectile_path_color );
    field( ATLAS_XORSTR( "projectile_path_low_confidence_color" ).get( ), value.m_projectile_path_low_confidence_color );
    field( ATLAS_XORSTR( "projectile_path_style" ).get( ), value.m_projectile_path_style );
    field( ATLAS_XORSTR( "projectile_path_outline" ).get( ), value.m_projectile_path_outline );
    field( ATLAS_XORSTR( "projectile_path_fade" ).get( ), value.m_projectile_path_fade );
    field( ATLAS_XORSTR( "projectile_path_fade_time" ).get( ), value.m_projectile_path_fade_time );
    field( ATLAS_XORSTR( "projectile_path_animate" ).get( ), value.m_projectile_path_animate );
    field( ATLAS_XORSTR( "projectile_path_gradient" ).get( ), value.m_projectile_path_gradient );
    field( ATLAS_XORSTR( "projectile_path_color_end" ).get( ), value.m_projectile_path_color_end );
    field( ATLAS_XORSTR( "projectile_path_bounds" ).get( ), value.m_projectile_path_bounds );
    field( ATLAS_XORSTR( "projectile_path_bounds_style" ).get( ), value.m_projectile_path_bounds_style );
    field( ATLAS_XORSTR( "projectile_path_bounds_color" ).get( ), value.m_projectile_path_bounds_color );
    field( ATLAS_XORSTR( "hitscan_draw_hitboxes" ).get( ), value.m_hitscan_draw_hitboxes );
    field( ATLAS_XORSTR( "hitscan_hitbox_color" ).get( ), value.m_hitscan_hitbox_color );
    field( ATLAS_XORSTR( "hitscan_target_color" ).get( ), value.m_hitscan_target_color );
    field( ATLAS_XORSTR( "hitscan_draw_shot" ).get( ), value.m_hitscan_draw_shot );
    field( ATLAS_XORSTR( "hitscan_shot_color" ).get( ), value.m_hitscan_shot_color );
    field( ATLAS_XORSTR( "hitscan_draw_duration" ).get( ), value.m_hitscan_draw_duration );

    field( ATLAS_XORSTR( "auto_charge_sticky_targets" ).get( ), value.m_auto_charge_sticky_targets );
    field( ATLAS_XORSTR( "huntsman_wait_for_kill" ).get( ), value.m_huntsman_wait_for_kill );

    field( ATLAS_XORSTR( "hitscan_profiles" ).get( ), value.m_hitscan_profiles );
    field( ATLAS_XORSTR( "projectile_profiles" ).get( ), value.m_projectile_profiles );
}

template< typename visitor >
static void config_fields( config::melee_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "activate_key" ).get( ), value.m_activate_key );
    field( ATLAS_XORSTR( "auto_swing" ).get( ), value.m_auto_swing );
    field( ATLAS_XORSTR( "auto_backstab" ).get( ), value.m_auto_backstab );
    field( ATLAS_XORSTR( "swing_prediction" ).get( ), value.m_swing_prediction );
    field( ATLAS_XORSTR( "always_crit" ).get( ), value.m_always_crit );
}

template< typename visitor >
static void config_fields( config::rjump_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "key" ).get( ), value.m_key );
    field( ATLAS_XORSTR( "priority" ).get( ), value.m_priority );
    field( ATLAS_XORSTR( "land_shot" ).get( ), value.m_land_shot );
    field( ATLAS_XORSTR( "pogo" ).get( ), value.m_pogo );
    field( ATLAS_XORSTR( "pogo_crouch" ).get( ), value.m_pogo_crouch );
}

template< typename visitor >
static void config_fields( config::lagcomp_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "draw_records" ).get( ), value.m_draw_records );
    field( ATLAS_XORSTR( "extrapolate" ).get( ), value.m_extrapolate );
}

template< typename visitor >
static void config_fields( config::resolver_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
}

template< typename visitor >
static void config_fields( config::crits_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "key" ).get( ), value.m_key );
    field( ATLAS_XORSTR( "force" ).get( ), value.m_force );
    field( ATLAS_XORSTR( "avoid" ).get( ), value.m_avoid );
    field( ATLAS_XORSTR( "indicator" ).get( ), value.m_indicator );
}

template< typename visitor >
static void config_fields( config::pills_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "seed_spin" ).get( ), value.m_seed_spin );
    field( ATLAS_XORSTR( "crit_priority" ).get( ), value.m_crit_priority );
}

template< typename visitor >
static void config_fields( config::auto_det_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "key" ).get( ), value.m_key );
    field( ATLAS_XORSTR( "radius_scale" ).get( ), value.m_radius_scale );
    field( ATLAS_XORSTR( "min_targets" ).get( ), value.m_min_targets );
    field( ATLAS_XORSTR( "avoid_self" ).get( ), value.m_avoid_self );
    field( ATLAS_XORSTR( "target_buildings" ).get( ), value.m_target_buildings );
    field( ATLAS_XORSTR( "dormant" ).get( ), value.m_dormant );
}

template< typename visitor >
static void config_fields( config::air_blast_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "key" ).get( ), value.m_key );
    field( ATLAS_XORSTR( "aim" ).get( ), value.m_aim );
    field( ATLAS_XORSTR( "redirect" ).get( ), value.m_redirect );
    field( ATLAS_XORSTR( "prefer_crits" ).get( ), value.m_prefer_crits );
    field( ATLAS_XORSTR( "extinguish" ).get( ), value.m_extinguish );
    field( ATLAS_XORSTR( "push" ).get( ), value.m_push );
}

template< typename visitor >
static void config_fields( config::auto_vacc_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "auto_pop" ).get( ), value.m_auto_pop );
    field( ATLAS_XORSTR( "idle_resist" ).get( ), value.m_idle_resist );
    field( ATLAS_XORSTR( "reactivity" ).get( ), value.m_reactivity );
    field( ATLAS_XORSTR( "request_bullet_key" ).get( ), value.m_request_bullet_key );
    field( ATLAS_XORSTR( "request_blast_key" ).get( ), value.m_request_blast_key );
    field( ATLAS_XORSTR( "request_fire_key" ).get( ), value.m_request_fire_key );
}

template< typename visitor >
static void config_fields( config::skin_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "model" ).get( ), value.m_model );
    field( ATLAS_XORSTR( "paintkit" ).get( ), value.m_paintkit );
    field( ATLAS_XORSTR( "wear" ).get( ), value.m_wear );
    field( ATLAS_XORSTR( "seed" ).get( ), value.m_seed );
    field( ATLAS_XORSTR( "australium" ).get( ), value.m_australium );
    field( ATLAS_XORSTR( "festivized" ).get( ), value.m_festivized );
    field( ATLAS_XORSTR( "unusual" ).get( ), value.m_unusual );
    field( ATLAS_XORSTR( "killstreak" ).get( ), value.m_killstreak );
    field( ATLAS_XORSTR( "sheen" ).get( ), value.m_sheen );
    field( ATLAS_XORSTR( "killstreaker" ).get( ), value.m_killstreaker );
}

template< typename visitor >
static void config_fields( config::skin_entry_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "definition" ).get( ), value.m_definition );
    field( ATLAS_XORSTR( "skin" ).get( ), value.m_skin );
}

template< typename visitor >
static void config_fields( config::wearable_skin_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "unusual" ).get( ), value.m_unusual );
}

template< typename visitor >
static void config_fields( config::wearable_entry_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "definition" ).get( ), value.m_definition );
    field( ATLAS_XORSTR( "skin" ).get( ), value.m_skin );
}

template< typename visitor >
static void config_fields( config::spells_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "pumpkin_bombs" ).get( ), value.m_pumpkin_bombs );
    field( ATLAS_XORSTR( "voices_from_below" ).get( ), value.m_voices_from_below );
    field( ATLAS_XORSTR( "green_flames" ).get( ), value.m_green_flames );
    field( ATLAS_XORSTR( "footprints" ).get( ), value.m_footprints );
    field( ATLAS_XORSTR( "footprint_custom" ).get( ), value.m_footprint_custom );
    field( ATLAS_XORSTR( "footprint_color" ).get( ), value.m_footprint_color );
    field( ATLAS_XORSTR( "jingle" ).get( ), value.m_jingle );
    field( ATLAS_XORSTR( "replace_step" ).get( ), value.m_replace_step );
    field( ATLAS_XORSTR( "full_moon" ).get( ), value.m_full_moon );
}

template< typename visitor >
static void config_fields( config::ragdolls_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "targets" ).get( ), value.m_targets );
    field( ATLAS_XORSTR( "gold" ).get( ), value.m_gold );
    field( ATLAS_XORSTR( "ice" ).get( ), value.m_ice );
    field( ATLAS_XORSTR( "dissolve" ).get( ), value.m_dissolve );
    field( ATLAS_XORSTR( "burning" ).get( ), value.m_burning );
    field( ATLAS_XORSTR( "electrocuted" ).get( ), value.m_electrocuted );
    field( ATLAS_XORSTR( "ash" ).get( ), value.m_ash );
}

template< typename visitor >
static void config_fields( config::skins_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "shared" ).get( ), value.m_shared );
    field( ATLAS_XORSTR( "weapons" ).get( ), value.m_weapons );
    field( ATLAS_XORSTR( "sheen_custom" ).get( ), value.m_sheen_custom );
    field( ATLAS_XORSTR( "sheen_color" ).get( ), value.m_sheen_color );
    field( ATLAS_XORSTR( "wearable_slot" ).get( ), value.m_wearable_slot );
    field( ATLAS_XORSTR( "wearables" ).get( ), value.m_wearables );
    field( ATLAS_XORSTR( "spells" ).get( ), value.m_spells );
    field( ATLAS_XORSTR( "ragdolls" ).get( ), value.m_ragdolls );
}

template< typename visitor >
static void config_fields( config::radio_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "key" ).get( ), value.m_key );
    field( ATLAS_XORSTR( "station" ).get( ), value.m_station );
    field( ATLAS_XORSTR( "volume" ).get( ), value.m_volume );
    field( ATLAS_XORSTR( "scale_to_game_volume" ).get( ), value.m_scale_to_game_volume );
    field( ATLAS_XORSTR( "indicator" ).get( ), value.m_indicator );
    field( ATLAS_XORSTR( "indicator_style" ).get( ), value.m_indicator_style );
    field( ATLAS_XORSTR( "indicator_position" ).get( ), value.m_indicator_position );
    field( ATLAS_XORSTR( "duck_on_voice" ).get( ), value.m_duck_on_voice );
    field( ATLAS_XORSTR( "duck_on_self" ).get( ), value.m_duck_on_self );
    field( ATLAS_XORSTR( "duck_on_others" ).get( ), value.m_duck_on_others );
    field( ATLAS_XORSTR( "duck_level" ).get( ), value.m_duck_level );
    field( ATLAS_XORSTR( "mute_unfocused" ).get( ), value.m_mute_unfocused );
    field( ATLAS_XORSTR( "crossfade" ).get( ), value.m_crossfade );
    field( ATLAS_XORSTR( "next_key" ).get( ), value.m_next_key );
    field( ATLAS_XORSTR( "previous_key" ).get( ), value.m_previous_key );
    field( ATLAS_XORSTR( "louder_key" ).get( ), value.m_louder_key );
    field( ATLAS_XORSTR( "quieter_key" ).get( ), value.m_quieter_key );
}

template< typename visitor >
static void config_fields( config::soundboard_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "key" ).get( ), value.m_key );
    field( ATLAS_XORSTR( "loop" ).get( ), value.m_loop );
    field( ATLAS_XORSTR( "volume" ).get( ), value.m_volume );
}

template< typename visitor >
static void config_fields( config::recorder_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "record_key" ).get( ), value.m_record_key );
    field( ATLAS_XORSTR( "replay_key" ).get( ), value.m_replay_key );
    field( ATLAS_XORSTR( "replay_attacks" ).get( ), value.m_replay_attacks );
    field( ATLAS_XORSTR( "camera_follows" ).get( ), value.m_camera_follows );
    field( ATLAS_XORSTR( "markers" ).get( ), value.m_markers );
    field( ATLAS_XORSTR( "marker_color" ).get( ), value.m_marker_color );
    field( ATLAS_XORSTR( "marker_distance" ).get( ), value.m_marker_distance );
    field( ATLAS_XORSTR( "route" ).get( ), value.m_route );
    field( ATLAS_XORSTR( "route_color" ).get( ), value.m_route_color );
    field( ATLAS_XORSTR( "traveled_color" ).get( ), value.m_traveled_color );
    field( ATLAS_XORSTR( "route_style" ).get( ), value.m_route_style );
    field( ATLAS_XORSTR( "key_overlay" ).get( ), value.m_key_overlay );
    field( ATLAS_XORSTR( "key_overlay_position" ).get( ), value.m_key_overlay_position );
}

template< typename visitor >
static void config_fields( config::streamer_mode_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "hide_targets" ).get( ), value.m_hide_targets );
    field( ATLAS_XORSTR( "stream_proof" ).get( ), value.m_stream_proof );
}

template< typename visitor >
static void config_fields( config::events_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "event_targets" ).get( ), value.m_notify_targets );
    field( ATLAS_XORSTR( "vote_start_notify_party" ).get( ), value.m_vote_start_notify_party );
}

template< typename visitor >
static void config_fields( config::player_tag_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "account" ).get( ), value.m_account );
    field( ATLAS_XORSTR( "tag" ).get( ), value.m_tag );
    field( ATLAS_XORSTR( "alias" ).get( ), value.m_alias );
    field( ATLAS_XORSTR( "groups" ).get( ), value.m_groups );
}

template< typename visitor >
static void config_fields( config::player_group_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "name" ).get( ), value.m_name );
    field( ATLAS_XORSTR( "color" ).get( ), value.m_color );
}

template< typename visitor >
static void config_fields( config::medibot_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ATLAS_XORSTR( "activate_key" ).get( ), value.m_activate_key );
    field( ATLAS_XORSTR( "prefer_mode" ).get( ), value.m_prefer_mode );
    field( ATLAS_XORSTR( "auto_uber" ).get( ), value.m_auto_uber );
    field( ATLAS_XORSTR( "auto_kritz" ).get( ), value.m_auto_kritz );
}

template< typename visitor >
static void config_fields( config::anti_aim_profile_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "yaw_base" ).get( ), value.m_yaw_base );
    field( ATLAS_XORSTR( "real_yaw_mode" ).get( ), value.m_real_yaw_mode );
    field( ATLAS_XORSTR( "fake_yaw_mode" ).get( ), value.m_fake_yaw_mode );
    field( ATLAS_XORSTR( "real_pitch_mode" ).get( ), value.m_real_pitch_mode );
    field( ATLAS_XORSTR( "fake_pitch_mode" ).get( ), value.m_fake_pitch_mode );
    field( ATLAS_XORSTR( "static_real_angle" ).get( ), value.m_static_real_angle );
    field( ATLAS_XORSTR( "static_fake_angle" ).get( ), value.m_static_fake_angle );
    field( ATLAS_XORSTR( "cylical_pos_1_real" ).get( ), value.m_cylical_pos_1_real );
    field( ATLAS_XORSTR( "cylical_pos_2_real" ).get( ), value.m_cylical_pos_2_real );
    field( ATLAS_XORSTR( "cylical_pos_1_fake" ).get( ), value.m_cylical_pos_1_fake );
    field( ATLAS_XORSTR( "cylical_pos_2_fake" ).get( ), value.m_cylical_pos_2_fake );
    field( ATLAS_XORSTR( "real_cycle_time" ).get( ), value.m_real_cycle_time );
    field( ATLAS_XORSTR( "fake_cycle_time" ).get( ), value.m_fake_cycle_time );
    field( ATLAS_XORSTR( "apply_jitter_real" ).get( ), value.m_apply_jitter_real );
    field( ATLAS_XORSTR( "apply_jitter_fake" ).get( ), value.m_apply_jitter_fake );
    field( ATLAS_XORSTR( "jitter_amount" ).get( ), value.m_jitter_amount );
    field( ATLAS_XORSTR( "spin_amount" ).get( ), value.m_spin_amount );
    field( ATLAS_XORSTR( "break_pitch" ).get( ), value.m_break_pitch );
}

template< typename visitor >
static void config_fields( config::fakelag_profile_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "fakelag_mode" ).get( ), value.m_fakelag_mode );
    field( ATLAS_XORSTR( "static_amount" ).get( ), value.m_static_amount );
    field( ATLAS_XORSTR( "random_min" ).get( ), value.m_random_min );
    field( ATLAS_XORSTR( "random_max" ).get( ), value.m_random_max );
    field( ATLAS_XORSTR( "adaptive_randomized" ).get( ), value.m_adaptive_randomized_choke );
}

template< typename visitor >
static void config_fields( config::anti_hack_t &value, visitor &field )
{
    field( ATLAS_XORSTR( "aa_enabled" ).get( ), value.m_anti_aim_enabled );
    field( ATLAS_XORSTR( "fl_enabled" ).get( ), value.m_fakelag_enabled );
    field( ATLAS_XORSTR( "aa_key" ).get( ), value.m_anti_aim_key );
    field( ATLAS_XORSTR( "fl_key" ).get( ), value.m_fakelag_key );
    field( ATLAS_XORSTR( "aa_mode" ).get( ), value.m_aa_mode );
    field( ATLAS_XORSTR( "hs_threat_only" ).get( ), value.m_headshot_threats_only );
    field( ATLAS_XORSTR( "anti_backstab" ).get( ), value.m_anti_backstab );
    field( ATLAS_XORSTR( "aa_profiles" ).get( ), value.m_aa_profiles );
    field( ATLAS_XORSTR( "fl_profiles" ).get( ), value.m_fl_profiles );
    field( ATLAS_XORSTR( "draw_real" ).get( ), value.m_draw_real_hitboxes );
    field( ATLAS_XORSTR( "draw_fake" ).get( ), value.m_draw_fake_hitboxes );
    field( ATLAS_XORSTR( "real_draw_mode" ).get( ), value.m_real_draw_mode );
    field( ATLAS_XORSTR( "fake_draw_mode" ).get( ), value.m_fake_draw_mode );
    field( ATLAS_XORSTR( "real_base_color" ).get( ), value.m_real_base_color );
    field( ATLAS_XORSTR( "fake_base_color" ).get( ), value.m_fake_base_color );
    field( ATLAS_XORSTR( "real_highlight_color" ).get( ), value.m_real_highlight_color );
    field( ATLAS_XORSTR( "fake_highlight_color" ).get( ), value.m_fake_highlight_color );
    field( ATLAS_XORSTR( "fake_chams" ).get( ), value.m_fake_chams );
    field( ATLAS_XORSTR( "fake_chams_color" ).get( ), value.m_fake_chams_color );
    field( ATLAS_XORSTR( "fake_glow" ).get( ), value.m_fake_glow );
}

template< typename visitor >
static void config_fields( config &value, visitor &field )
{
    field( ATLAS_XORSTR( "esp" ).get( ), value.m_esp );
    field( ATLAS_XORSTR( "hitmarker" ).get( ), value.m_hitmarker );
    field( ATLAS_XORSTR( "chams" ).get( ), value.m_chams );
    field( ATLAS_XORSTR( "glow" ).get( ), value.m_glow );
    field( ATLAS_XORSTR( "weather" ).get( ), value.m_weather );
    field( ATLAS_XORSTR( "tracers" ).get( ), value.m_tracers );
    field( ATLAS_XORSTR( "arrows" ).get( ), value.m_arrows );
    field( ATLAS_XORSTR( "skybox" ).get( ), value.m_skybox );
    field( ATLAS_XORSTR( "world" ).get( ), value.m_world );
    field( ATLAS_XORSTR( "fog" ).get( ), value.m_fog );
    field( ATLAS_XORSTR( "post_process" ).get( ), value.m_post_process );
    field( ATLAS_XORSTR( "viewmodel" ).get( ), value.m_viewmodel );
    field( ATLAS_XORSTR( "misc" ).get( ), value.m_misc );
    field( ATLAS_XORSTR( "queue" ).get( ), value.m_queue );
    field( ATLAS_XORSTR( "exploits" ).get( ), value.m_exploits );
    field( ATLAS_XORSTR( "ragebot" ).get( ), value.m_ragebot );
    field( ATLAS_XORSTR( "melee" ).get( ), value.m_melee );
    field( ATLAS_XORSTR( "rjump" ).get( ), value.m_rjump );
    field( ATLAS_XORSTR( "lagcomp" ).get( ), value.m_lagcomp );
    field( ATLAS_XORSTR( "resolver" ).get( ), value.m_resolver );
    field( ATLAS_XORSTR( "crits" ).get( ), value.m_crits );
    field( ATLAS_XORSTR( "pills" ).get( ), value.m_pills );
    field( ATLAS_XORSTR( "auto_vacc" ).get( ), value.m_auto_vacc );
    field( ATLAS_XORSTR( "auto_det" ).get( ), value.m_auto_det );
    field( ATLAS_XORSTR( "air_blast" ).get( ), value.m_air_blast );
    field( ATLAS_XORSTR( "skins" ).get( ), value.m_skins );
    field( ATLAS_XORSTR( "radio" ).get( ), value.m_radio );
    field( ATLAS_XORSTR( "soundboard" ).get( ), value.m_soundboard );
    field( ATLAS_XORSTR( "projectile_path" ).get( ), value.m_projectile_path );
    field( ATLAS_XORSTR( "trajectory" ).get( ), value.m_trajectory );
    field( ATLAS_XORSTR( "projectile_warning" ).get( ), value.m_projectile_warning );
    field( ATLAS_XORSTR( "streamer_mode" ).get( ), value.m_streamer_mode );
    field( ATLAS_XORSTR( "medibot" ).get( ), value.m_medibot );
    field( ATLAS_XORSTR( "events" ).get( ), value.m_events );
    field( ATLAS_XORSTR( "player_tags" ).get( ), value.m_player_tags );
    field( ATLAS_XORSTR( "player_groups" ).get( ), value.m_player_groups );
    field( ATLAS_XORSTR( "recorder" ).get( ), value.m_recorder );
    field( ATLAS_XORSTR( "anti_hack" ).get( ), value.m_anti_hack );
}
