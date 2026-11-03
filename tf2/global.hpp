#pragma once

#include "entities.hpp"

#define MAKE_VFUNC_IDX( idx_name ) \
    u16 m_##idx_name{ 1024 }

// Manages all game offsets.
class tf2
{
public:
    // Class singleton pointers.
    CGlobalVarsBase              *m_globals{ };
    CHLClient                    *m_client{ };
    CEngineClient                *m_engine{ };
    CInput                       *m_input{ };
    ClientModeShared             *m_client_mode{ };
    CClientEntityList            *m_entity_list{ };
    void                         *m_entity_list_internal{ };
    CStudioRender                *m_studio_render{ };
    CModelInfoClient             *m_model_info{ };
    IMDLCache                    *m_mdl_cache{ };
    CEngineTrace                 *m_trace{ };
    CMaterialSystem              *m_material_system{ };
    IPredictionSystem            *m_pred_system{ };
    ICvar                        *m_cvar{ };
    CEngineVGui                  *m_vgui{ };
    CMatSystemSurface            *m_surface{ };
    CVRenderView                 *m_render_view{ };
    CMoveHelperClient            *m_move_helper{ };
    CPrediction                  *m_prediction{ };
    CGameMovement                *m_game_movement{ };
    IKeyValuesSystem             *m_kv_system{ };
    CClientState                 *m_cl{ };
    IDemoRecorder                *m_demo_recorder{ };
    IDemoPlayer                  *m_demo_player{ };
    IMemAlloc                    *m_alloc{ };
    IPhysics                     *m_physics{ };
    IPhysicsCollision            *m_physics_collision{ };
    IPhysicsSurfaceProps         *m_physics_surface_props{ };
    IGameEventManager2           *m_game_events{ };
    IVModelRender                *m_model_render{ };
    INetworkStringTableContainer *m_string_tables{ };
    IModelLoader                 *m_model_loader{ };
    IViewRenderBeams             *m_beams{ };
    IFileSystem                  *m_filesystem{ };
    IStaticPropMgr               *m_static_prop_mgr{ };
    CViewRender                  *m_view_render{ };
    vgui::IPanel                 *m_panel{ };
#if defined( ATLAS_DEV )
    IVEngineServer *m_engine_server{ };
#endif // ATLAS_DEV

    // Prediction datamaps.
    datamap_t *m_C_TFWeaponBase_datamap{ };
    datamap_t *m_C_WeaponMedigun_datamap{ };
    datamap_t *m_C_TFMinigun_datamap{ };
    datamap_t *m_C_TFFlameThrower_datamap{ };

    // ConVars.
    ConVar *sv_maxunlag{ };
    ConVar *sv_lagcompensation_teleport_dist{ };
    ConVar *sv_gravity{ };
    ConVar *tf_grenadelauncher_livetime{ };
    ConVar *tf_sticky_radius_ramp_time{ };
    ConVar *tf_sticky_airdet_radius{ };
    ConVar *tf_fireball_speed{ };
    ConVar *tf_fireball_distance{ };
    ConVar *tf_fireball_max_lifetime{ };
    ConVar *tf_flamethrower_boxsize{ };
    ConVar *sv_friction{ };
    ConVar *sv_accelerate{ };
    ConVar *sv_airaccelerate{ };
    ConVar *tf_max_charge_speed{ };
    ConVar *tf_demoman_charge_drain_time{ };
    ConVar *weapon_vaccinator_resist_duration{ };
    ConVar *sv_stopspeed{ };
    ConVar *sv_stepsize{ };
    ConVar *sv_maxvelocity{ };
    ConVar *tf_clamp_back_speed{ };
    ConVar *tf_clamp_back_speed_min{ };
    ConVar *tf_clamp_airducks{ };
    ConVar *tf_movement_aircurrent_friction_mult{ };
    ConVar *tf_movement_aircurrent_aircontrol_mult{ };
    ConVar *tf_movement_lost_footing_restick{ };
    ConVar *tf_movement_lost_footing_friction{ };
    ConVar *tf_parachute_maxspeed_xy{ };
    ConVar *tf_parachute_maxspeed_z{ };
    ConVar *tf_parachute_aircontrol{ };
    ConVar *tf_use_fixed_weaponspreads{ nullptr };
    ConVar *tf_weapon_criticals_melee{ nullptr };
    ConVar *tf_weapon_criticals_bucket_cap{ nullptr };
    ConVar *tf_weapon_criticals_bucket_bottom{ nullptr };
    ConVar *tf_weapon_criticals_nopred{ nullptr };
    ConVar *sv_usercmd_custom_random_seed{ nullptr };
    ConVar *developer{ nullptr };
    ConVar *host_limitlocal{ nullptr };
    ConVar *sv_clockcorrection_msecs{ nullptr };
    ConVar *sv_maxusrcmdprocessticks{ nullptr };
    ConVar *sv_maxusrcmdprocessticks_holdaim{ nullptr };
    ConVar *sv_cheats{ nullptr };
    ConVar *rate{ nullptr };
    ConVar *cl_cmdrate{ nullptr };
    ConVar *cl_updaterate{ nullptr };
    ConVar *cl_interp_ratio{ nullptr };
    ConVar *cl_interp{ nullptr };
    ConVar *cl_smooth{ nullptr };
    ConVar *cl_pred_optimize{ nullptr };
    ConVar *sv_lan{ nullptr };
    ConVar *sv_maxcmdrate{ nullptr };
    ConVar *sv_maxupdaterate{ nullptr };
    ConVar *sv_client_min_interp_ratio{ nullptr };
    ConVar *sv_client_max_interp_ratio{ };
    ConVar *sv_minupdaterate{ };
    ConVar *cl_interpolate{ };
    ConVar *sv_maxrate{ nullptr };
    ConVar *sv_minrate{ nullptr };
    ConVar *mat_queue_mode{ nullptr };
    ConVar *r_threaded_particles{ nullptr };
    ConVar *r_threaded_renderables{ nullptr };
    ConVar *r_hunkalloclightmaps{ nullptr };
    ConVar *mat_bumpmap{ nullptr };
    ConVar *mat_managedtextures{ nullptr };
    ConVar *mat_forcemanagedtextureintohardware{ nullptr };
    ConVar *r_drawbatchdecals{ nullptr };
    ConVar *mp_friendlyfire{ nullptr };
    ConVar *cl_windspeed{ nullptr };
    ConVar *cl_winddir{ nullptr };
    ConVar *r_rainspeed{ nullptr };
    ConVar *r_rainwidth{ nullptr };
    ConVar *r_rainlength{ nullptr };
    ConVar *sv_skyname{ nullptr };
    ConVar *default_fov{ nullptr };
    ConVar *fov_desired{ nullptr };
    ConVar *mat_postprocess_enable{ nullptr };
    ConVar *mat_motion_blur_enabled{ nullptr };
    ConVar *mat_motion_blur_strength{ nullptr };
    ConVar *tf_medigun_autoheal{ nullptr };
    ConVar *cl_flipviewmodels{ nullptr };
    ConVar *r_drawtracers{ nullptr };
    ConVar *r_drawtracers_firstperson{ nullptr };
    ConVar *cl_ragdoll_fade_time{ nullptr };
    ConVar *cl_ragdoll_physics_enable{ nullptr };
    ConVar *cl_ragdoll_forcefade{ nullptr };

    // Modules.
    util::pe_module m_client_dll{ };
    util::pe_module m_engine_dll{ };
    util::pe_module m_studiorender_dll{ };
    util::pe_module m_vgui2_dll{ };
    util::pe_module m_vguimatsurface_dll{ };
    util::pe_module m_tier0_dll{ };
    util::pe_module m_materialsystem_dll{ };
    util::pe_module m_vstdlib_dll{ };
    util::pe_module m_vphysics_dll{ };
    util::pe_module m_datacache_dll{ };
    util::pe_module m_shaderapidx9_dll{ };
    util::pe_module m_d3d9_dll{ };
    util::pe_module m_serverbrowser_dll{ };
    util::pe_module m_steam_api_dll{ };
    util::pe_module m_filesystem_stdio_dll{ };

    // Function addresses.
    util::address m_KeyValuesSystem_fn{ };
    util::address m_Tier0_Warning_fn{ };
    util::address m_CUserCmd_GetChecksum{ };
    util::address m_CClientEntityList_AddListenerEntity{ };
    util::address m_CClientEntityList_NextBaseEntity{ };
    util::address m_PredCopy_TransferData{ };
    util::address m_CL_RunPrediction_fn{ };
    util::address m_CAttributeManager_AttribHookValue{ };
    util::address m_Host_ShouldRun_fn{ };
    util::address m_CClientState_IsPaused_fn{ };
    util::address m_CClientState_ForceFullUpdate_fn{ };
    util::address m_net_time{ };
    util::address m_host_frametime_unbounded{ };
    util::address m_host_frametime_stddeviation{ };
    util::address m_host_frameticks{ };
    util::address m_gamerules{ };
    util::address m_CBaseFlex_ctor{ };
    util::address m_CBaseAnimating_FrameAdvance{ };
    util::address m_CBaseAnimating_InvalidateBoneCache{ };
    util::address m_CBaseEntity_CalcAbsoluteVelocity{ };
    util::address m_CBaseAnimating_GetModelPtr{ };
    util::address m_CBaseAnimating_SequenceDuration{ };
    util::address m_CBaseAnimating_GetSequenceActivity{ };
    util::address m_CStudioHdr_LookupSequence{ };
    util::address m_Studio_BoneIndexByName{ };
    util::address m_BoneSetup_InitPose{ };
    util::address m_CBoneSetup_AccumulatePose{ };
    util::address m_IBoneSetup_CalcAutoplaySequences{ };
    util::address m_BoneSetup_CalcBoneAdj{ };
    util::address m_CBaseAnimating_SetupBonesAttachmentHelper{ };
    util::address m_CBaseEntity_SetAbsOrigin{ };
    util::address m_CBaseEntity_SetAbsAngles{ };
    util::address m_CBaseEntity_SetPredictionRandomSeed{ };
    util::address m_CBaseEntity_SetModelByIndex{ };
    util::address m_CBaseEntity_GetClassname{ };
    util::address m_CBaseEntity_EstimateAbsVelocity{ };
    util::address m_CBasePlayer_ItemPostFrame{ };
    util::address m_interpolate_flag{ };
    util::address m_enable_extrapolation_retaddr{ };
    util::address m_cl_fire_events_retaddr{ };
    util::address m_cl_event_queue{ };
    util::address m_CStudioHdr_SequencesAvailable{ };
    util::address m_CStudioHdr_GetNumSeq{ };
    util::address m_CStudioHdr_pSeqdesc{ };
    util::address m_MD5_PseudoRandom{ };
    util::address m_SharedRandomInt{ };
    util::address m_prediction_player{ };
    util::address m_LookupWeaponInfoSlot{ };
    util::address m_GetFileWeaponInfoFromHandle{ };
    util::address m_CTFPlayer_GetClassEyeHeight{ };
    util::address m_CTFMinigun_StartMuzzleEffect{ };
    util::address m_CTFMinigun_StartBrassEffect{ };
    util::address m_KeyValues_ctor{ };
    util::address m_KeyValues_SetString{ };
    util::address m_KeyValues_SetInt{ };
    util::address m_RandomSeed_fn{ };
    util::address m_RandomFloat_fn{ };
    util::address m_RandomInt_fn{ };
    util::address m_R_InitSky{ };
    util::address m_C_TFPlayer_TeamFortress_CalculateMaxSpeed{ };
    util::address m_GetItemSchema{ };
    util::address m_CEconItemSchema_GetAttributeDefinition{ };
    util::address m_CAttributeList_SetRuntimeAttributeValue{ };
    util::address m_CEconItemSchema_GetItemDefinition{ };
    util::address m_C_TFPlayer_ShouldDraw_retaddr{ };
    util::address m_C_TFWearable_ShouldDraw_retaddr{ };
    util::address m_C_TFPlayer_CreateMove_retaddr{ };
    util::address m_C_TFPlayer_CreateMove_freeze_retaddr{ };
    util::address m_C_TFPlayer_CreateMove_kart_retaddr{ };
    util::address m_C_TFPlayer_ValidateModelIndex_dispenser_retaddr{ };
    util::address m_C_TFPlayer_ValidateModelIndex_disguise_retaddr{ };
    util::address m_CTFInput_ApplyMouse_kart_retaddr{ };
    util::address m_KeyValues_SetInt_retaddr{ };
    util::address m_KeyValues_SetInt_jump{ };
    util::address m_C_TFPlayerShared_IsPlayerDominated_retaddr{ };
    util::address m_C_TFPlayerShared_IsPlayerDominated_jump{ };
    util::address m_TeamStatus_GetTeam_retaddr{ };
    util::address m_CUniformRandomStream_ctor{ };
    util::address m_C_TFPlayer_GetEntityForLoadoutSlot{ };
    util::address m_VoiceEncoder_OPUS_create{ };
    util::address m_C_TFPlayerResource_GetPlayerName{ };
    util::address m_TFPartyClient{ };
    util::address m_TFGCClientSystem{ };
    util::address m_CTFGCClientSystem_GetLobby{ };
    util::address m_GameResources{ };
    util::address m_C_TFPartyClient_SendToPartyChat{ };
    util::address m_CTFInventoryManager_GetItemInLoadoutForClass_retaddr{ };

    void  random_seed( i32 seed );
    float random_float( float minimum, float maximum );
    i32   random_int( i32 minimum, i32 maximum );

    CUniformRandomStream m_random{ };

    // Indices.
    MAKE_VFUNC_IDX( chl_client_get_all_classes );
    MAKE_VFUNC_IDX( chl_client_get_player_view );
    MAKE_VFUNC_IDX( chl_client_create_move );
    MAKE_VFUNC_IDX( chl_client_write_usercmd_delta_to_buffer );
    MAKE_VFUNC_IDX( c_input_cam_to_first_person );
    MAKE_VFUNC_IDX( c_input_cam_to_third_person );
    MAKE_VFUNC_IDX( c_input_cam_is_third_person );
    MAKE_VFUNC_IDX( base_player_third_person_switch );
    MAKE_VFUNC_IDX( client_entity_list_get_client_entity );
    MAKE_VFUNC_IDX( client_entity_list_get_client_entity_from_handle );
    MAKE_VFUNC_IDX( mdl_cache_find_mdl );
    MAKE_VFUNC_IDX( mdl_cache_get_studio_hdr );
    MAKE_VFUNC_IDX( mdl_cache_get_hardware_data );
    MAKE_VFUNC_IDX( mdl_cache_begin_lock );
    MAKE_VFUNC_IDX( mdl_cache_end_lock );
    MAKE_VFUNC_IDX( client_entity_list_get_highest_entity_index );
    MAKE_VFUNC_IDX( engine_client_get_local_player );
    MAKE_VFUNC_IDX( engine_client_plat_float_time );
    MAKE_VFUNC_IDX( engine_client_get_player_info );
    MAKE_VFUNC_IDX( engine_client_get_player_for_user_id );
    MAKE_VFUNC_IDX( engine_client_is_in_game );
    MAKE_VFUNC_IDX( engine_client_is_connected );
    MAKE_VFUNC_IDX( engine_client_is_playing_demo );
    MAKE_VFUNC_IDX( engine_client_get_screen_size );
    MAKE_VFUNC_IDX( engine_client_get_view_angles );
    MAKE_VFUNC_IDX( engine_client_set_view_angles );
    MAKE_VFUNC_IDX( engine_client_get_max_clients );
    MAKE_VFUNC_IDX( engine_client_get_net_channel_info );
    MAKE_VFUNC_IDX( engine_client_fire_events );
    MAKE_VFUNC_IDX( engine_client_is_taking_screenshot );
    MAKE_VFUNC_IDX( engine_client_get_map_entities_string );
    MAKE_VFUNC_IDX( engine_client_client_cmd_unrestricted );
    MAKE_VFUNC_IDX( engine_client_server_cmd_key_values );
    MAKE_VFUNC_IDX( mat_system_surface_unlock_cursor );
    MAKE_VFUNC_IDX( mat_system_surface_set_cursor_always_visible );
    MAKE_VFUNC_IDX( mat_system_surface_play_sound );
    MAKE_VFUNC_IDX( move_helper_client_process_impacts );
    MAKE_VFUNC_IDX( move_helper_client_set_host );
    MAKE_VFUNC_IDX( move_helper_client_reset_touch_list );
    MAKE_VFUNC_IDX( prediction_get_local_view_angles );
    MAKE_VFUNC_IDX( prediction_set_local_view_angles );
    MAKE_VFUNC_IDX( prediction_run_command );
    MAKE_VFUNC_IDX( prediction_setup_move );
    MAKE_VFUNC_IDX( prediction_finish_move );
    MAKE_VFUNC_IDX( render_view_get_matrices_for_view );
    MAKE_VFUNC_IDX( render_view_push_3d_view );
    MAKE_VFUNC_IDX( render_view_pop_view );
    MAKE_VFUNC_IDX( render_context_bind_local_cubemap );
    MAKE_VFUNC_IDX( render_context_set_ambient_light );
    MAKE_VFUNC_IDX( render_context_set_ambient_light_cube );
    MAKE_VFUNC_IDX( render_context_set_lighting_origin );
    MAKE_VFUNC_IDX( material_system_find_texture );
    MAKE_VFUNC_IDX( studio_render_set_local_lights );
    MAKE_VFUNC_IDX( studio_render_set_ambient_light_colors );
    MAKE_VFUNC_IDX( model_render_suppress_engine_lighting );
    MAKE_VFUNC_IDX( string_table_container_remove_all_tables );
    MAKE_VFUNC_IDX( string_table_container_find_table );
    MAKE_VFUNC_IDX( string_table_container_create_string_table_ex );
    MAKE_VFUNC_IDX( string_table_add_string );
    MAKE_VFUNC_IDX( string_table_find_string_index );
    MAKE_VFUNC_IDX( model_loader_get_model_for_name );
    MAKE_VFUNC_IDX( key_values_system_alloc_key_values_memory );
    MAKE_VFUNC_IDX( mem_alloc_alloc );
    MAKE_VFUNC_IDX( mem_alloc_free );
    MAKE_VFUNC_IDX( mem_alloc_realloc );
    MAKE_VFUNC_IDX( base_entity_get_pred_desc_map );
    MAKE_VFUNC_IDX( base_entity_on_latch_interpolated_variables );
    MAKE_VFUNC_IDX( base_entity_get_solid );
    MAKE_VFUNC_IDX( base_entity_should_collide );
    MAKE_VFUNC_IDX( base_animating_get_attachment );
    MAKE_VFUNC_IDX( gamerules_get_view_vectors );
    MAKE_VFUNC_IDX( filesystem_add_search_path );
    MAKE_VFUNC_IDX( filesystem_relative_path_to_full_path );
    MAKE_VFUNC_IDX( filesystem_create_dir_hierarchy );
    MAKE_VFUNC_IDX( filesystem_find_first_ex );
    MAKE_VFUNC_IDX( filesystem_find_next );
    MAKE_VFUNC_IDX( filesystem_find_is_directory );
    MAKE_VFUNC_IDX( filesystem_find_close );
    MAKE_VFUNC_IDX( filesystem_open );
    MAKE_VFUNC_IDX( filesystem_size );
    MAKE_VFUNC_IDX( filesystem_read );
    MAKE_VFUNC_IDX( filesystem_close );
    MAKE_VFUNC_IDX( view_render_beams_create_beam_points );
    MAKE_VFUNC_IDX( view_render_beams_draw_beam );
    MAKE_VFUNC_IDX( model_info_get_model_name );
    MAKE_VFUNC_IDX( model_info_get_studio_model );
    MAKE_VFUNC_IDX( model_info_get_model_index );
    MAKE_VFUNC_IDX( model_info_get_vcollide );
    MAKE_VFUNC_IDX( model_info_get_model_type );
    MAKE_VFUNC_IDX( model_info_get_model );
    MAKE_VFUNC_IDX( model_info_get_model_bounds );
    MAKE_VFUNC_IDX( tf_player_anim_state_update );
    MAKE_VFUNC_IDX( net_channel_is_loopback );
    MAKE_VFUNC_IDX( net_channel_is_timing_out );
    MAKE_VFUNC_IDX( net_channel_send_net_msg );
    MAKE_VFUNC_IDX( net_channel_set_choked );
    MAKE_VFUNC_IDX( net_channel_send_datagram );
    MAKE_VFUNC_IDX( net_channel_can_packet );
    MAKE_VFUNC_IDX( net_channel_get_latency );
    MAKE_VFUNC_IDX( tf_weapon_base_get_weapon_id );
    MAKE_VFUNC_IDX( tf_weapon_base_get_damage_type );
    MAKE_VFUNC_IDX( tf_weapon_base_apply_fire_delay );
    MAKE_VFUNC_IDX( tf_weapon_base_get_slot );
    MAKE_VFUNC_IDX( tf_weapon_base_are_random_crits_enabled );
    MAKE_VFUNC_IDX( tf_weapon_base_get_projectile_damage );
    MAKE_VFUNC_IDX( cvar_find_var );
    MAKE_VFUNC_IDX( cvar_get_commands );
    MAKE_VFUNC_IDX( demo_recorder_is_recording );
    MAKE_VFUNC_IDX( demo_recorder_record_user_input );
    MAKE_VFUNC_IDX( demo_player_is_playing_back );
    MAKE_VFUNC_IDX( studio_render_set_color_modulation );
    MAKE_VFUNC_IDX( studio_render_set_alpha_modulation );
    MAKE_VFUNC_IDX( studio_render_forced_material_override );
    MAKE_VFUNC_IDX( material_system_create_material );
    MAKE_VFUNC_IDX( material_system_find_material );
    MAKE_VFUNC_IDX( material_system_get_render_context );
    MAKE_VFUNC_IDX( material_system_first_material );
    MAKE_VFUNC_IDX( material_system_next_material );
    MAKE_VFUNC_IDX( material_system_invalid_material );
    MAKE_VFUNC_IDX( material_system_get_material );
    MAKE_VFUNC_IDX( render_context_depth_range );
    MAKE_VFUNC_IDX( render_context_clear_buffers );
    MAKE_VFUNC_IDX( render_context_set_stencil_enable );
    MAKE_VFUNC_IDX( render_context_set_stencil_fail_operation );
    MAKE_VFUNC_IDX( render_context_set_stencil_zfail_operation );
    MAKE_VFUNC_IDX( render_context_set_stencil_pass_operation );
    MAKE_VFUNC_IDX( render_context_set_stencil_compare_function );
    MAKE_VFUNC_IDX( render_context_set_stencil_reference_value );
    MAKE_VFUNC_IDX( render_context_set_stencil_test_mask );
    MAKE_VFUNC_IDX( render_context_set_stencil_write_mask );
    MAKE_VFUNC_IDX( render_context_set_render_target );
    MAKE_VFUNC_IDX( render_context_viewport );
    MAKE_VFUNC_IDX( render_context_cull_mode );
    MAKE_VFUNC_IDX( render_context_clear_color4ub );
    MAKE_VFUNC_IDX( render_context_draw_screen_space_rectangle );
    MAKE_VFUNC_IDX( render_context_push_render_target_and_viewport );
    MAKE_VFUNC_IDX( render_context_pop_render_target_and_viewport );
    MAKE_VFUNC_IDX( render_context_get_call_queue );
    MAKE_VFUNC_IDX( render_context_get_matrix );
    MAKE_VFUNC_IDX( render_context_release );
    MAKE_VFUNC_IDX( render_context_fog_mode );
    MAKE_VFUNC_IDX( render_context_fog_start );
    MAKE_VFUNC_IDX( render_context_fog_end );
    MAKE_VFUNC_IDX( render_context_fog_color3fv );
    MAKE_VFUNC_IDX( render_context_fog_max_density );
    MAKE_VFUNC_IDX( call_queue_queue_functor_internal );
    MAKE_VFUNC_IDX( mesh_handle_late_creation );
    MAKE_VFUNC_IDX( material_system_create_named_render_target_texture_ex );
    MAKE_VFUNC_IDX( material_get_name );
    MAKE_VFUNC_IDX( material_get_texture_group_name );
    MAKE_VFUNC_IDX( material_color_modulate );
    MAKE_VFUNC_IDX( material_find_var );
    MAKE_VFUNC_IDX( material_is_error_material );
    MAKE_VFUNC_IDX( material_is_precached );
    MAKE_VFUNC_IDX( material_precache );
    MAKE_VFUNC_IDX( material_var_set_float_value );
    MAKE_VFUNC_IDX( material_var_set_vec_value );
    MAKE_VFUNC_IDX( texture_increment_reference_count );
    MAKE_VFUNC_IDX( texture_decrement_reference_count );
    MAKE_VFUNC_IDX( texture_delete_if_unreferenced );
    MAKE_VFUNC_IDX( material_increment_reference_count );
    MAKE_VFUNC_IDX( render_view_set_blend );
    MAKE_VFUNC_IDX( render_view_get_blend );
    MAKE_VFUNC_IDX( render_view_set_color_modulation );
    MAKE_VFUNC_IDX( render_view_get_color_modulation );
    MAKE_VFUNC_IDX( material_set_material_var_flag );
    MAKE_VFUNC_IDX( material_decrement_reference_count );
    MAKE_VFUNC_IDX( material_delete_if_unreferenced );
    MAKE_VFUNC_IDX( convar_set_value_chr );
    MAKE_VFUNC_IDX( convar_set_value_fl );
    MAKE_VFUNC_IDX( convar_set_value_int );
    MAKE_VFUNC_IDX( convar_get_name );
    MAKE_VFUNC_IDX( convar_is_flag_set );
    MAKE_VFUNC_IDX( engine_trace_trace_ray );
    MAKE_VFUNC_IDX( model_info_get_vcollide_model );
    MAKE_VFUNC_IDX( model_info_get_collide_for_virtual_terrain );
    MAKE_VFUNC_IDX( model_info_get_cache_handle );
    MAKE_VFUNC_IDX( engine_trace_get_brushes );
    MAKE_VFUNC_IDX( engine_trace_get_brush_info );
    MAKE_VFUNC_IDX( engine_trace_get_displacements );
    MAKE_VFUNC_IDX( engine_trace_enumerate_entities );
    MAKE_VFUNC_IDX( engine_trace_get_collideable );
    MAKE_VFUNC_IDX( engine_trace_clip_entity );
    MAKE_VFUNC_IDX( collideable_get_model );
    MAKE_VFUNC_IDX( collideable_get_origin );
    MAKE_VFUNC_IDX( collideable_get_angles );
    MAKE_VFUNC_IDX( collideable_get_transform );
    MAKE_VFUNC_IDX( collideable_get_solid );
    MAKE_VFUNC_IDX( collideable_get_solid_flags );
    MAKE_VFUNC_IDX( physics_collision_convex_from_planes );
    MAKE_VFUNC_IDX( physics_collision_convex_free );
    MAKE_VFUNC_IDX( physics_collision_convert_convex );
    MAKE_VFUNC_IDX( physics_collision_destroy_collide );
    MAKE_VFUNC_IDX( physics_collision_get_convexes );
    MAKE_VFUNC_IDX( physics_collision_polyhedron );
    MAKE_VFUNC_IDX( physics_collision_create_mesh );
    MAKE_VFUNC_IDX( physics_collision_destroy_mesh );
    MAKE_VFUNC_IDX( physics_collision_trace_collide );
    MAKE_VFUNC_IDX( physics_collision_get_extent );
    MAKE_VFUNC_IDX( physics_create_environment );
    MAKE_VFUNC_IDX( physics_destroy_environment );
    MAKE_VFUNC_IDX( physics_surface_props_get_surface_index );
    MAKE_VFUNC_IDX( physics_surface_props_get_surface_data );
    MAKE_VFUNC_IDX( physics_surface_props_get_physics_properties );
    MAKE_VFUNC_IDX( physics_env_set_gravity );
    MAKE_VFUNC_IDX( physics_env_set_air_density );
    MAKE_VFUNC_IDX( physics_env_create_poly_object );
    MAKE_VFUNC_IDX( physics_env_create_poly_object_static );
    MAKE_VFUNC_IDX( physics_env_destroy_object );
    MAKE_VFUNC_IDX( physics_env_simulate );
    MAKE_VFUNC_IDX( physics_env_set_simulation_timestep );
    MAKE_VFUNC_IDX( physics_env_reset_simulation_clock );
    MAKE_VFUNC_IDX( physics_env_set_collision_event_handler );
    MAKE_VFUNC_IDX( physics_env_set_performance_settings );
    MAKE_VFUNC_IDX( physics_object_is_asleep );
    MAKE_VFUNC_IDX( physics_object_wake );
    MAKE_VFUNC_IDX( physics_object_set_drag_coefficient );
    MAKE_VFUNC_IDX( physics_object_set_position );
    MAKE_VFUNC_IDX( physics_object_get_position );
    MAKE_VFUNC_IDX( physics_object_set_velocity );
    MAKE_VFUNC_IDX( physics_object_get_velocity );
    MAKE_VFUNC_IDX( physics_collision_collide_get_aabb );
    MAKE_VFUNC_IDX( physics_collision_bbox_to_collide );
    MAKE_VFUNC_IDX( physics_collision_data_get_surface_normal );
    MAKE_VFUNC_IDX( physics_collision_data_get_contact_point );
    MAKE_VFUNC_IDX( static_prop_mgr_create_vphysics_representations );
    MAKE_VFUNC_IDX( static_prop_mgr_is_static_prop_cbasehandle );
    MAKE_VFUNC_IDX( static_prop_mgr_get_all_in_aabb );
    MAKE_VFUNC_IDX( vgui_panel_get_name );
    MAKE_VFUNC_IDX( engine_vgui_is_game_ui_visible );
    MAKE_VFUNC_IDX( player_group_get_num_members );
    MAKE_VFUNC_IDX( player_group_get_member );
    MAKE_VFUNC_IDX( lobby_get_num_members );
    MAKE_VFUNC_IDX( lobby_get_member_details );
    MAKE_VFUNC_IDX( econ_notification_destructor );

    atl::unordered_map< hash_t, ConCommandBase * > m_cmd_table{ };
};

struct projectile_target_t;

// Manages cheat specific vars.
class client
{
public:
    // https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_weaponbase.cpp#L5801
    static constexpr f32 PROJECTILE_AIM_DISTANCE{ 2000.0f };
    static constexpr f32 PROJECTILE_AIM_FRACTION{ 0.1f };

    C_TFPlayer         *m_local{ nullptr };
    C_TFWeaponBase     *m_weapon{ nullptr };
    C_TFPlayerResource *m_resource{ nullptr };
    CUserCmd           *m_cmd{ nullptr };
    CUserCmd           *m_last_cmd{ nullptr };

    atl::atomic< bool > m_is_running{ false };
    atl::atomic< bool > m_is_early_init_finished{ false };

    bool m_send_packet{ true };
    bool m_final_tick{ true };

    float m_reload_gesture_start{ -1.0f };

    VMatrix            m_w2s{ };
    atl::atomic< i32 > m_screen_width{ };
    atl::atomic< i32 > m_screen_height{ };
    float              m_view_fov{ };

    void  update_w2s( );
    void  update_local_data( );
    void  on_entity_deleted( C_BaseEntity *entity, u32 handle );
    bool  world_to_screen( const vec3 &world, vec2 &screen ) const;
    bool  clip_segment_to_screen( const vec3 &start, const vec3 &end, vec2 &screen_start,
                                  vec2 &screen_end ) const;
    bool  is_on_screen( const rect_t &bounds ) const;
    bool  is_on_screen( C_BaseEntity *entity ) const;
    usize pickup_kind( C_BaseEntity *entity, ETFClassID class_id ) const;
    void  on_directx_frame( void *device );
    void  fix_movement( CUserCmd *cmd, const vec3 &original_angles );
    float attrib_hook( const C_BaseEntity *entity, float value, const char *hook,
                       bool global_const_string = false ) const;
    vec3  projectile_fire_offset( C_TFWeaponBase *weapon, vec3 offset ) const;
    vec3  projectile_fire_origin( C_TFPlayer *player, C_TFWeaponBase *weapon, const vec3 &angles, vec3 offset ) const;
    void  projectile_fire_setup( C_TFPlayer *player, C_TFWeaponBase *weapon, const vec3 &angles,
                                 vec3 offset, bool hit_teammates, vec3 &out_position,
                                 vec3 &out_angles, const projectile_target_t *target = nullptr );
    bool  projectile_grenade_setup( C_TFPlayer *player, C_TFWeaponBase *weapon, const vec3 &angles,
                                    vec3 &out_position );
    bool  trace_line( C_BaseEntity *skip, C_BaseEntity *target, const vec3 &from, const vec3 &to, u32 mask );
    void  trace_hull( const vec3 &start, const vec3 &end, const vec3 &mins, const vec3 &maxs, trace_t &trace );
    bool  has_live_weapon_command( ) const;
    bool  weapon_attack_allowed( f32 time ) const;
    bool  weapon_fired( f32 previous_last_fire ) const;
    bool  will_fire_this_tick( ) const;
    bool  weapon_charge_pending( ) const;
    bool  flame_ball_ready( C_TFWeaponBase *weapon ) const;
    void  manage_psilent( );
    bool  is_delayed_throw_weapon( C_TFWeaponBase *weapon ) const;
    bool  weapon_throw_pending( ) const;
    void  clear_throw( );
    void  latch_throw( f32 previous_last_fire );
    void  update_throw( );
    void  apply_throw_angle( );
    void  reconcile_throw_batch(
        i32 first_command,
        i32 simulation_ticks,
        i32 batch_player_tickbase,
        i32 server_tick );
    i32  throw_spawn_tick( ) const;
    void predict_reload_shell( C_TFWeaponBase *weapon );
    bool chat_open( ) const;
    bool game_owns_keyboard( ) const;
    void sync_chat_cursor( );
    bool should_perform_clean_frame( );
    bool should_draw_teammate( C_TFPlayer *player );
    bool is_friend( u32 account );
    bool is_friend( C_TFPlayer *player );
    bool is_atlas_user( u32 account ) const;
    bool is_atlas_user( C_TFPlayer *player ) const;
    bool is_party_member( u32 account ) const;
    u8   party_index( u32 account ) const;
    bool is_ignored_player( u32 account );
    void refresh_party( );
    void refresh_lobby_parties( );

    static color_t party_color( u8 party );

    const char *get_player_name( IGameResources *resources, i32 idx );
    const char *get_player_name( i32 idx );

    void draw_3d_text( const vec3 &origin, const color_t &col, const char *buf );
    void draw_3d_box( const vec3 &origin, const vec3 &mins, const vec3 &maxs, const color_t &col, const vec3 &angle_of_rotation = vec3{ } );
    void draw_3d_box( const matrix3x4 &transform, const vec3 &mins, const vec3 &maxs, const color_t &col );
    void draw_3d_line( const vec3 &start, const vec3 &end, const color_t &col, bool outlined );
    void draw_3d_overlay_circle( const vec3 &origin, const color_t &col, f32 radius = 1.f, bool outlined = false );

    static bool  is_in_game_and_alive( );
    static usize get_profile( );

    i32  m_throw_command{ -1 };
    i32  m_throw_tick{ -1 };
    u32  m_throw_weapon{ ~0u };
    vec3 m_throw_angle{ };
    bool m_chat_cursor_held{ false };
    bool m_will_fire{ false };
    bool m_psilent_choked{ false };
    bool m_psilent_release{ false };
    bool m_vm_flipped{ false };

    void *m_handler{ nullptr };

    atl::atomic< bool >  m_has_been_in_game_and_alive_once{ false };
    atl::atomic< usize > m_target_config_profile{ 0 };

    struct steam_friend_t
    {
        u32  m_handle{ };
        u32  m_friends_id{ };
        bool m_friend{ };
    };

    struct lobby_party_t
    {
        u32 m_account{ };
        u8  m_party{ };
    };

    steam_friend_t m_steam_friends[ MAX_TRACKED_PLAYERS ]{ };
    u32            m_party_members[ MAX_PARTY_SIZE ]{ };
    usize          m_party_member_count{ };
    lobby_party_t  m_lobby_parties[ MAX_TRACKED_PLAYERS ]{ };
    usize          m_lobby_party_count{ };
};

class hooks
{
public:
    api_hook m_present{ };
    api_hook m_reset{ };
    api_hook m_create_move{ };
    api_hook m_validate_ucmd{ };
    api_hook m_frame_stage_notify{ };
    api_hook m_paint{ };
    api_hook m_do_image_space_motion_blur{ };
    api_hook m_set_screen_overlay_material{ };
    api_hook m_render_view{ };
    api_hook m_get_usercmd{ };
    api_hook m_check_init_predictable{ };
    api_hook m_s_start_sound{ };
    api_hook m_level_shutdown{ };
    api_hook m_client_shutdown{ };
    api_hook m_voice_status{ };
    api_hook m_dispatch_user_message{ };
    api_hook m_fx_fire_bullets{ };
    api_hook m_cl_move{ };
    api_hook m_cl_read_packets{ };
    api_hook m_packet_start{ };
    api_hook m_run_simulation{ };
    api_hook m_do_post_screen_space_effects{ };
    api_hook m_override_view{ };
    api_hook m_draw_model{ };
    api_hook m_internal_draw_model{ };
    api_hook m_draw_no_z_translucent{ };
    api_hook m_level_init_pre_entity{ };
    api_hook m_forced_material_override{ };
    api_hook m_set_color_modulation{ };
    api_hook m_draw_static_props{ };
    api_hook m_get_model{ };
    api_hook m_lightcache_get_dynamic{ };
    api_hook m_update_visibility{ };
    api_hook m_set_dormant{ };
    api_hook m_minigun_on_new_model{ };
    api_hook m_setup_bones{ };
    api_hook m_estimate_abs_velocity{ };
    api_hook m_anim_state_do_animation_event{ };
    api_hook m_base_animating_interpolate{ };
    api_hook m_update_client_side_animation{ };
    api_hook m_build_transformations{ };
    api_hook m_compute_aim_yaw{ };
    api_hook m_check_for_sequence_change{ };
    api_hook m_maintain_sequence_transitions{ };
    api_hook m_set_sequence{ };
    api_hook m_base_interpolate_part1{ };
    api_hook m_interpolate_server_entities{ };
    api_hook m_client_state_is_paused{ };
    api_hook m_client_state_get_time{ };
    api_hook m_net_chan_process_packet{ };
    api_hook m_net_chan_set_choked{ };
    api_hook m_net_chan_read_sub_channel_data{ };
    api_hook m_net_chan_send_net_msg{ };
    api_hook m_post_think{ };
    api_hook m_get_max_item_count{ };
    api_hook m_reset_latched{ };
    api_hook m_on_client_use_consumable{ };
    api_hook m_lock_cursor{ };
    api_hook m_set_cursor_position{ };
    api_hook m_set_cursor{ };
    api_hook m_check_for_pure_server_whitelist{ };
    api_hook m_data_table_warning{ };
    api_hook m_weapon_item_post_frame{ };
    api_hook m_sniper_item_post_frame{ };
    api_hook m_sniper_classic_item_post_frame{ };
    api_hook m_medigun_item_post_frame{ };
    api_hook m_flamethrower_item_post_frame{ };
    api_hook m_flameball_item_post_frame{ };
    api_hook m_weapon_item_busy_frame{ };
    api_hook m_is_first_time_predicted{ };
    api_hook m_get_local_view_angles{ };
    api_hook m_calc_is_attack_critical{ };
    api_hook m_fire_bullet{ };
    api_hook m_util_particle_tracer{ };
    api_hook m_calc_view_model_view{ };
    api_hook m_calc_view_model_bob_helper{ };
    api_hook m_should_flip_view_model{ };
    api_hook m_hud_crosshair_get_draw_position{ };
    api_hook m_get_view_model_fov{ };
    api_hook m_avoid_players{ };
    api_hook m_get_client_interp_amount{ };
    api_hook m_enable_world_fog{ };
    api_hook m_enable_3d_skybox_fog{ };
    api_hook m_get_screen_aspect_ratio{ };
    api_hook m_fire_event_client_side{ };
    api_hook m_get_model_index{ };
    api_hook m_apply_attribute_float_wrapper{ };
    api_hook m_attrib_hook_value_int{ };
    api_hook m_sheen_on_bind{ };
    api_hook m_third_person_manager_update{ };
    api_hook m_match_invite_on_tick{ };
    api_hook m_emit_sound{ };
    api_hook m_ragdoll_on_data_changed{ };
    api_hook m_tf_is_holiday_active{ };
    api_hook m_game_rules_is_holiday_active{ };
    api_hook m_get_direct_ping_to_datacenter{ };
    api_hook m_voice_get_compressed_data{ };
    api_hook m_find_command{ };
    api_hook m_find_command_const{ };
    api_hook m_unregister_con_command{ };
    api_hook m_unregister_con_commands{ };
    api_hook m_in_cond{ };
    api_hook m_paint_traverse{ };
    api_hook m_should_draw_view_model{ };
    api_hook m_is_player_dominated{ };
    api_hook m_player_panel_get_team{ };
    api_hook m_set_int{ };
    api_hook m_check_achievements_enabled{ };
    api_hook m_set_avatar_steam_id{ };
    api_hook m_set_player{ };
    api_hook m_update_player_avatar1{ };
    api_hook m_update_player_avatar2{ };
    api_hook m_update_player_avatar3{ };
    api_hook m_update_player_avatar4{ };
    api_hook m_get_player_name{ };
    api_hook m_insert_and_colorize_text{ };
    api_hook m_verify_changed_loadouts_are_valid{ };
    api_hook m_generate_equip_region_conflict_mask{ };
    api_hook m_get_item_in_loadout_for_class{ };
    api_hook m_get_shoot_sound{ };
    api_hook m_create_tf_ragdoll{ };
    api_hook m_cbuf_execute_command{ };
    api_hook m_notification_queue_add{ };
};

extern tf2    g_tf2;
extern client g_cl;
extern hooks  g_hooks;
extern api    g_api;

inline C_TFGameRules *TFGameRules( )
{
    return *g_tf2.m_gamerules.get_as< C_TFGameRules ** >( );
}

inline C_TFPartyClient *TFPartyClient( )
{
    return g_tf2.m_TFPartyClient.call_as< C_TFPartyClient * >( );
}

inline CTFGCClientSystem *TFGCClientSystem( )
{
    return g_tf2.m_TFGCClientSystem.call_as< CTFGCClientSystem * >( );
}

inline IGameResources *GameResources( )
{
    return g_tf2.m_GameResources.call_as< IGameResources * >( );
}

struct globals_backup_t
{
    float m_curtime{ };
    float m_frametime{ };
    i32   m_tickcount{ };
    i32   m_framecount{ };
    float m_client_frametime{ };

    void store( )
    {
        m_curtime          = g_tf2.m_globals->curtime;
        m_frametime        = g_tf2.m_globals->frametime;
        m_tickcount        = g_tf2.m_globals->tickcount;
        m_framecount       = g_tf2.m_globals->framecount;
        m_client_frametime = g_tf2.m_cl->m_frameTime;
    }

    void restore( ) const
    {
        g_tf2.m_globals->curtime    = m_curtime;
        g_tf2.m_globals->frametime  = m_frametime;
        g_tf2.m_globals->tickcount  = m_tickcount;
        g_tf2.m_globals->framecount = m_framecount;
        g_tf2.m_cl->m_frameTime     = m_client_frametime;
    }
};

#define TICK_INTERVAL       ( g_tf2.m_globals->interval_per_tick )
#define TIME_TO_TICKS( dt ) ( ( int )( 0.5 + ( double )( dt ) / ( double )TICK_INTERVAL ) ) // float32 tie at tick>=2^23 (~35h uptime) makes round-half-to-even truncate to even-only ticks, desyncing lagcomp stamps..
#define TICKS_TO_TIME( t )  ( TICK_INTERVAL * ( t ) )
#define ROUND_TO_TICKS( t ) ( TICK_INTERVAL * TIME_TO_TICKS( t ) )

#if defined( ATLAS_DEV )
#define DBGOUT( fmtstr, ... ) g_tf2.m_Tier0_Warning_fn.call_variadic< void >( "[!] " fmtstr "\n", __VA_ARGS__ )
#else
#define DBGOUT( fmtstr, ... )
#endif
