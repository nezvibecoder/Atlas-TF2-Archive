#include "config_storage.hpp"
#include "global.hpp"
#include "menu.hpp"

#include "netvars.hpp"
#include "steam.hpp"
#include "steam_html.hpp"

#include "runtime/integrity.hpp"
#include "runtime/thread_pool.hpp"
#include "script/script.hpp"

#define RAISE_ERROR( msg )                                                                                    \
    I( MessageBoxA )( NULL, ATLAS_XORSTR( msg ).get( ), ATLAS_XORSTR( "atlas-error" ).get( ), MB_ICONERROR ); \
    __debugbreak( )

#include "hooks.inl"

long ATLAS_STDCALL exception_filter( PEXCEPTION_POINTERS info )
{
    return exceptions::on_caught_exception( info );
}

u32 ATLAS_STDCALL atlas_thread( void *parameter )
{
    g_cl.m_is_running             = false;
    g_cl.m_is_early_init_finished = false;

    if ( !g_api.startup( parameter ) )
    {
        RAISE_ERROR( "Failed to retrieve required startup info.\n"
                     "Please check to make sure your anti-virus is disabled, otherwise make a support ticket if this issue persists." );
        return 1;
    }

    if ( !atlas_integrity::ipc_watcher_startup( ) )
    {
        RAISE_ERROR( "Failed to retrieve required startup info.\n"
                     "Please check to make sure your anti-virus is disabled, otherwise make a support ticket if this issue persists." );
        return 1;
    }

    if ( !atlas_integrity::wait_for_initial_handshake( 60000 ) )
    {
        return 1;
    }

    // d3d9
    {
        const u64 started{ I( GetTickCount64 )( ) };

        do
        {
            g_tf2.m_d3d9_dll = util::find_module( ATLAS_XORWSTR( L"d3d9.dll" ).get( ) );

            if ( g_tf2.m_d3d9_dll.is_null( ) )
            {
                I( Sleep )( 50 );
            }
        } while ( g_tf2.m_d3d9_dll.is_null( ) && I( GetTickCount64 )( ) - started < 60000 ); // ~60s

        if ( g_tf2.m_d3d9_dll.is_null( ) )
        {
            RAISE_ERROR( "Early initialization failed.\n"
                         "Please check to make sure your anti-virus is disabled, otherwise make a support ticket if this issue persists." );
            return 1;
        }
    }

    // Render
    {
        // rendering comes up first so the loading panel can report the stages below while they run
        g_loading.enter( load_stage_t::render );
#if defined( ATLAS_DISABLE_CLOUD )
        void *present_addr{ };
        void *reset_addr{ };
        render::find_device_methods( present_addr, reset_addr );

        g_hooks.m_present = api_hook{ present_addr, reinterpret_cast< void * >( &hkPresent ) };
        g_hooks.m_reset   = api_hook{ reset_addr, reinterpret_cast< void * >( &hkReset ) };
#else
        const u64 started{ I( GetTickCount64 )( ) };
        while ( !g_api.cache_module( api_module::shaderapidx9 ) &&
                I( GetTickCount64 )( ) - started < 60000 )
        {
            I( Sleep )( 50 );
        }

        if ( !g_api.cache_module( api_module::shaderapidx9 ) )
        {
            RAISE_ERROR( "Early initialization failed.\n"
                         "Please check to make sure your anti-virus is disabled, otherwise make a support ticket if this issue persists." );
            return 1;
        }

        g_hooks.m_present = g_api.get_hook_pkg( HASH_CT( "hook.tf2.d3d9.present" ), reinterpret_cast< void * >( &hkPresent ) );
        g_hooks.m_reset   = g_api.get_hook_pkg( HASH_CT( "hook.tf2.d3d9.reset" ), reinterpret_cast< void * >( &hkReset ) );
#endif // ATLAS_DEV

        if ( !g_hooks.m_present.enable( ) ||
             !g_hooks.m_reset.enable( ) )
        {
            RAISE_ERROR( "Early initialization failed.\n"
                         "Please check to make sure your anti-virus is disabled, otherwise make a support ticket if this issue persists." );
            return 1;
        }

        if ( !g_render.init( ) )
        {
            RAISE_ERROR( "Early initialization failed.\n"
                         "Please check to make sure your anti-virus is disabled, otherwise make a support ticket if this issue persists." );
            return 1;
        }

        if ( !register_menu( ) )
        {
            RAISE_ERROR( "Early initialization failed.\n"
                         "Please check to make sure your anti-virus is disabled, otherwise make a support ticket if this issue persists." );
            return 1;
        }
    }

    g_cl.m_is_early_init_finished = true;

    // Modules
    {
        g_loading.enter( load_stage_t::modules );

        const u64 started{ I( GetTickCount64 )( ) };
        bool      modules_ready{ false };

        do
        {
            g_tf2.m_client_dll           = util::find_module( ATLAS_XORWSTR( L"client.dll" ).get( ) );
            g_tf2.m_engine_dll           = util::find_module( ATLAS_XORWSTR( L"engine.dll" ).get( ) );
            g_tf2.m_studiorender_dll     = util::find_module( ATLAS_XORWSTR( L"studiorender.dll" ).get( ) );
            g_tf2.m_vgui2_dll            = util::find_module( ATLAS_XORWSTR( L"vgui2.dll" ).get( ) );
            g_tf2.m_vguimatsurface_dll   = util::find_module( ATLAS_XORWSTR( L"vguimatsurface.dll" ).get( ) );
            g_tf2.m_tier0_dll            = util::find_module( ATLAS_XORWSTR( L"tier0.dll" ).get( ) );
            g_tf2.m_materialsystem_dll   = util::find_module( ATLAS_XORWSTR( L"materialsystem.dll" ).get( ) );
            g_tf2.m_vstdlib_dll          = util::find_module( ATLAS_XORWSTR( L"vstdlib.dll" ).get( ) );
            g_tf2.m_vphysics_dll         = util::find_module( ATLAS_XORWSTR( L"vphysics.dll" ).get( ) );
            g_tf2.m_datacache_dll        = util::find_module( ATLAS_XORWSTR( L"datacache.dll" ).get( ) );
            g_tf2.m_serverbrowser_dll    = util::find_module( ATLAS_XORWSTR( L"serverbrowser.dll" ).get( ) ); // serverbrowser is fetched here to check if the game is fully loaded, we don't fetch any interfaces from it for now.
            g_tf2.m_steam_api_dll        = util::find_module( ATLAS_XORWSTR( L"steam_api64.dll" ).get( ) );
            g_tf2.m_filesystem_stdio_dll = util::find_module( ATLAS_XORWSTR( L"filesystem_stdio.dll" ).get( ) );

            modules_ready = !g_tf2.m_client_dll.is_null( ) &&
                            !g_tf2.m_engine_dll.is_null( ) &&
                            !g_tf2.m_studiorender_dll.is_null( ) &&
                            !g_tf2.m_vgui2_dll.is_null( ) &&
                            !g_tf2.m_vguimatsurface_dll.is_null( ) &&
                            !g_tf2.m_tier0_dll.is_null( ) &&
                            !g_tf2.m_materialsystem_dll.is_null( ) &&
                            !g_tf2.m_vstdlib_dll.is_null( ) &&
                            !g_tf2.m_vphysics_dll.is_null( ) &&
                            !g_tf2.m_serverbrowser_dll.is_null( ) &&
                            !g_tf2.m_steam_api_dll.is_null( ) &&
                            !g_tf2.m_filesystem_stdio_dll.is_null( );

            if ( !modules_ready )
            {
                I( Sleep )( 50 );
            }
        } while ( !modules_ready && I( GetTickCount64 )( ) - started < 60000 ); // ~60s

        if ( !modules_ready )
        {
            RAISE_ERROR( "Failed to acquire required game memory.\nIf this error persists, please make a support ticket." );
            return 1;
        }

        if ( !g_tf2.m_client_dll.is_valid( ) ||
             !g_tf2.m_engine_dll.is_valid( ) ||
             !g_tf2.m_studiorender_dll.is_valid( ) ||
             !g_tf2.m_vgui2_dll.is_valid( ) ||
             !g_tf2.m_vguimatsurface_dll.is_valid( ) ||
             !g_tf2.m_tier0_dll.is_valid( ) ||
             !g_tf2.m_materialsystem_dll.is_valid( ) ||
             !g_tf2.m_vstdlib_dll.is_valid( ) ||
             !g_tf2.m_vphysics_dll.is_valid( ) ||
             !g_tf2.m_serverbrowser_dll.is_valid( ) ||
             !g_tf2.m_steam_api_dll.is_valid( ) ||
             !g_tf2.m_filesystem_stdio_dll.is_valid( ) )
        {
            return 1;
        }

        g_api.cache_modules( );
    }

    I( Sleep )( 1000 );

    // Interfaces
    {
        g_loading.enter( load_stage_t::interfaces );

        g_tf2.m_client                = g_tf2.m_client_dll.find_interface( ATLAS_XORSTR( "VClient017" ).get( ) ).get_as< CHLClient * >( );
        g_tf2.m_prediction            = g_tf2.m_client_dll.find_interface( ATLAS_XORSTR( "VClientPrediction001" ).get( ) ).get_as< CPrediction * >( );
        g_tf2.m_entity_list           = g_tf2.m_client_dll.find_interface( ATLAS_XORSTR( "VClientEntityList003" ).get( ) ).get_as< CClientEntityList * >( );
        g_tf2.m_game_movement         = g_tf2.m_client_dll.find_interface( ATLAS_XORSTR( "GameMovement001" ).get( ) ).get_as< CGameMovement * >( );
        g_tf2.m_engine                = g_tf2.m_engine_dll.find_interface( ATLAS_XORSTR( "VEngineClient014" ).get( ) ).get_as< CEngineClient * >( );
        g_tf2.m_vgui                  = g_tf2.m_engine_dll.find_interface( ATLAS_XORSTR( "VEngineVGui002" ).get( ) ).get_as< CEngineVGui * >( );
        g_tf2.m_render_view           = g_tf2.m_engine_dll.find_interface( ATLAS_XORSTR( "VEngineRenderView014" ).get( ) ).get_as< CVRenderView * >( );
        g_tf2.m_model_info            = g_tf2.m_engine_dll.find_interface( ATLAS_XORSTR( "VModelInfoClient006" ).get( ) ).get_as< CModelInfoClient * >( );
        g_tf2.m_static_prop_mgr       = g_tf2.m_engine_dll.find_interface( ATLAS_XORSTR( "StaticPropMgrClient004" ).get( ) ).get_as< IStaticPropMgr * >( );
        g_tf2.m_game_events           = g_tf2.m_engine_dll.find_interface( ATLAS_XORSTR( "GAMEEVENTSMANAGER002" ).get( ) ).get_as< IGameEventManager2 * >( );
        g_tf2.m_model_render          = g_tf2.m_engine_dll.find_interface( ATLAS_XORSTR( "VEngineModel016" ).get( ) ).get_as< IVModelRender * >( );
        g_tf2.m_string_tables         = g_tf2.m_engine_dll.find_interface( ATLAS_XORSTR( "VEngineClientStringTable001" ).get( ) ).get_as< INetworkStringTableContainer * >( );
        g_tf2.m_trace                 = g_tf2.m_engine_dll.find_interface( ATLAS_XORSTR( "EngineTraceClient003" ).get( ) ).get_as< CEngineTrace * >( );
        g_tf2.m_mdl_cache             = g_tf2.m_datacache_dll.find_interface( ATLAS_XORSTR( "MDLCache004" ).get( ) ).get_as< IMDLCache * >( );
        g_tf2.m_cvar                  = g_tf2.m_vstdlib_dll.find_interface( ATLAS_XORSTR( "VEngineCvar004" ).get( ) ).get_as< ICvar * >( );
        g_tf2.m_material_system       = g_tf2.m_materialsystem_dll.find_interface( ATLAS_XORSTR( "VMaterialSystem082" ).get( ) ).get_as< CMaterialSystem * >( );
        g_tf2.m_surface               = g_tf2.m_vguimatsurface_dll.find_interface( ATLAS_XORSTR( "VGUI_Surface030" ).get( ) ).get_as< CMatSystemSurface * >( );
        g_tf2.m_panel                 = g_tf2.m_vgui2_dll.find_interface( ATLAS_XORSTR( "VGUI_Panel009" ).get( ) ).get_as< vgui::IPanel * >( );
        g_tf2.m_studio_render         = g_tf2.m_studiorender_dll.find_interface( ATLAS_XORSTR( "VStudioRender025" ).get( ) ).get_as< CStudioRender * >( );
        g_tf2.m_physics               = g_tf2.m_vphysics_dll.find_interface( ATLAS_XORSTR( "VPhysics031" ).get( ) ).get_as< IPhysics * >( );
        g_tf2.m_physics_collision     = g_tf2.m_vphysics_dll.find_interface( ATLAS_XORSTR( "VPhysicsCollision007" ).get( ) ).get_as< IPhysicsCollision * >( );
        g_tf2.m_physics_surface_props = g_tf2.m_vphysics_dll.find_interface( ATLAS_XORSTR( "VPhysicsSurfaceProps001" ).get( ) ).get_as< IPhysicsSurfaceProps * >( );
        g_tf2.m_filesystem            = g_tf2.m_filesystem_stdio_dll.find_interface( ATLAS_XORSTR( "VFileSystem022" ).get( ) ).get_as< IFileSystem * >( );

        if ( !g_tf2.m_client ||
             !g_tf2.m_prediction ||
             !g_tf2.m_entity_list ||
             !g_tf2.m_game_movement ||
             !g_tf2.m_engine ||
             !g_tf2.m_vgui ||
             !g_tf2.m_render_view ||
             !g_tf2.m_model_info ||
             !g_tf2.m_trace ||
             !g_tf2.m_cvar ||
             !g_tf2.m_material_system ||
             !g_tf2.m_surface ||
             !g_tf2.m_panel ||
             !g_tf2.m_studio_render ||
             !g_tf2.m_game_events ||
             !g_tf2.m_model_render ||
             !g_tf2.m_string_tables ||
             !g_tf2.m_physics ||
             !g_tf2.m_physics_collision ||
             !g_tf2.m_physics_surface_props ||
             !g_tf2.m_filesystem )
        {
            g_hooks.m_reset.reset( );
            g_hooks.m_present.reset( );
            RAISE_ERROR( "Failed to acquire required game memory.\nThe cheat may be out of date." );
            return 1;
        }

#if defined( ATLAS_DEV )
        g_tf2.m_engine_server = g_tf2.m_engine_dll.find_interface( ATLAS_XORSTR( "VEngineServer023" ).get( ) ).get_as< IVEngineServer * >( );
        Assert( g_tf2.m_engine_server != nullptr );
#endif // ATLAS_DEV

        const u64 pointers_started{ I( GetTickCount64 )( ) };
        bool      pointers_ready{ false };

        do
        {
            g_tf2.m_model_loader         = g_api.get_signature_pkg( HASH_CT( "tf2.engine.model_loader" ) ).get_as< IModelLoader * >( );
            g_tf2.m_beams                = g_api.get_signature_pkg( HASH_CT( "tf2.client.view_render_beams" ) ).get_as< IViewRenderBeams * >( );
            g_tf2.m_input                = g_api.get_signature_pkg( HASH_CT( "tf2.cinput.pointer" ) ).get_as< CInput * >( );
            g_tf2.m_client_mode          = g_api.get_signature_pkg( HASH_CT( "tf2.client_mode.pointer" ) ).get_as< ClientModeShared * >( );
            g_tf2.m_globals              = g_api.get_signature_pkg( HASH_CT( "tf2.engine.globals" ) ).get_as< CGlobalVarsBase * >( );
            g_tf2.m_pred_system          = g_api.get_signature_pkg( HASH_CT( "tf2.prediction_systems.head" ) ).get_as< IPredictionSystem * >( );
            g_tf2.m_move_helper          = g_api.get_signature_pkg( HASH_CT( "tf2.move_helper.pointer" ) ).get_as< CMoveHelperClient * >( );
            g_tf2.m_cl                   = g_api.get_signature_pkg( HASH_CT( "tf2.engine.client_state" ) ).get_as< CClientState * >( );
            g_tf2.m_demo_player          = g_api.get_signature_pkg( HASH_CT( "tf2.engine.demo_player" ) ).get_as< IDemoPlayer * >( );
            g_tf2.m_demo_recorder        = g_api.get_signature_pkg( HASH_CT( "tf2.engine.demo_recorder" ) ).get_as< IDemoRecorder * >( );
            g_tf2.m_view_render          = g_api.get_signature_pkg( HASH_CT( "tf2.client.view_render" ) ).get_as< CViewRender * >( );
            g_tf2.m_entity_list_internal = g_api.get_signature_pkg( HASH_CT( "tf2.client.entity_list.pointer" ) ).get_as< void * >( );

            pointers_ready = g_tf2.m_model_loader &&
                             g_tf2.m_input &&
                             g_tf2.m_client_mode &&
                             g_tf2.m_globals &&
                             g_tf2.m_pred_system &&
                             g_tf2.m_move_helper &&
                             g_tf2.m_cl &&
                             g_tf2.m_demo_player &&
                             g_tf2.m_demo_recorder &&
                             g_tf2.m_view_render &&
                             g_tf2.m_beams &&
                             g_tf2.m_entity_list_internal;

            if ( !pointers_ready )
            {
                I( Sleep )( 50 );
            }
        } while ( !pointers_ready && I( GetTickCount64 )( ) - pointers_started < 60000 );

        if ( !pointers_ready )
        {
            g_hooks.m_reset.reset( );
            g_hooks.m_present.reset( );
            RAISE_ERROR( "Failed to acquire required game memory.\nThe cheat may be out of date." );
            return 1;
        }
    }

    I( Sleep )( 100 );

    // Exports
    {
        g_loading.enter( load_stage_t::exports );

        // Functions all go first, then the classes.
        // As some functions return a pointer to a singleton within the game memory.
        g_tf2.m_KeyValuesSystem_fn        = g_tf2.m_vstdlib_dll.find_export( ATLAS_XORSTR( "KeyValuesSystem" ).get( ) );
        g_tf2.m_Tier0_Warning_fn          = g_tf2.m_tier0_dll.find_export( ATLAS_XORSTR( "Warning" ).get( ) );
        g_tf2.m_RandomSeed_fn             = g_tf2.m_vstdlib_dll.find_export( ATLAS_XORSTR( "RandomSeed" ).get( ) );
        g_tf2.m_RandomFloat_fn            = g_tf2.m_vstdlib_dll.find_export( ATLAS_XORSTR( "RandomFloat" ).get( ) );
        g_tf2.m_RandomInt_fn              = g_tf2.m_vstdlib_dll.find_export( ATLAS_XORSTR( "RandomInt" ).get( ) );
        g_tf2.m_CUniformRandomStream_ctor = g_tf2.m_vstdlib_dll.find_export_fuzzy( ATLAS_XORSTR( "CUniformRandomStream" ).get( ) );

        if ( g_tf2.m_KeyValuesSystem_fn.is_null( ) ||
             g_tf2.m_Tier0_Warning_fn.is_null( ) ||
             g_tf2.m_RandomSeed_fn.is_null( ) ||
             g_tf2.m_RandomFloat_fn.is_null( ) ||
             g_tf2.m_RandomInt_fn.is_null( ) ||
             g_tf2.m_CUniformRandomStream_ctor.is_null( ) )
        {
            RAISE_ERROR( "Failed to acquire required game memory.\nThe cheat may be out of date." );
            return 1;
        }

        g_tf2.m_CUniformRandomStream_ctor.call_as< void >( &g_tf2.m_random );

        g_tf2.m_alloc     = g_tf2.m_tier0_dll.find_export( ATLAS_XORSTR( "g_pMemAlloc" ).get( ) ).deref( ).get_as< IMemAlloc * >( );
        g_tf2.m_kv_system = g_tf2.m_KeyValuesSystem_fn.call_as< IKeyValuesSystem * >( );

        if ( !g_tf2.m_alloc ||
             !g_tf2.m_kv_system )
        {
            RAISE_ERROR( "Failed to acquire required game memory.\nThe cheat may be out of date." );
            return 1;
        }
    }

    I( Sleep )( 100 );

    // Signatures
    {
        g_loading.enter( load_stage_t::signatures );

        g_tf2.m_PredCopy_TransferData                                = g_api.get_signature_pkg( HASH_CT( "tf2.pred_copy.transfer_data" ) );
        g_tf2.m_CL_RunPrediction_fn                                  = g_api.get_signature_pkg( HASH_CT( "tf2.engine.cl_run_prediction" ) );
        g_tf2.m_CUserCmd_GetChecksum                                 = g_api.get_signature_pkg( HASH_CT( "tf2.client.usercmd_getchecksum" ) );
        g_tf2.m_CClientEntityList_AddListenerEntity                  = g_api.get_signature_pkg( HASH_CT( "tf2.client.entity_list.add_listener" ) );
        g_tf2.m_CClientEntityList_NextBaseEntity                     = g_api.get_signature_pkg( HASH_CT( "tf2.client.entity_list.next_entity" ) );
        g_tf2.m_CAttributeManager_AttribHookValue                    = g_api.get_signature_pkg( HASH_CT( "tf2.client.attrib_hook_value" ) );
        g_tf2.m_Host_ShouldRun_fn                                    = g_api.get_signature_pkg( HASH_CT( "tf2.engine.host_should_run" ) );
        g_tf2.m_CClientState_IsPaused_fn                             = g_api.get_signature_pkg( HASH_CT( "tf2.engine.client_state_is_paused" ) );
        g_tf2.m_CClientState_ForceFullUpdate_fn                      = g_api.get_signature_pkg( HASH_CT( "tf2.engine.client_state_force_full_update" ) );
        g_tf2.m_R_InitSky                                            = g_api.get_signature_pkg( HASH_CT( "tf2.engine.r_init_sky" ) );
        g_tf2.m_net_time                                             = g_api.get_signature_pkg( HASH_CT( "tf2.engine.net_time" ) );
        g_tf2.m_gamerules                                            = g_api.get_signature_pkg( HASH_CT( "tf2.client.gamerules_pointer" ) );
        g_tf2.m_host_frametime_unbounded                             = g_api.get_signature_pkg( HASH_CT( "tf2.engine.host_frametime_unbounded" ) );
        g_tf2.m_host_frametime_stddeviation                          = g_api.get_signature_pkg( HASH_CT( "tf2.engine.host_frametime_stddeviation" ) );
        g_tf2.m_host_frameticks                                      = g_api.get_signature_pkg( HASH_CT( "tf2.engine.host_frameticks" ) );
        g_tf2.m_KeyValues_ctor                                       = g_api.get_signature_pkg( HASH_CT( "tf2.client.keyvalues_ctor" ) );
        g_tf2.m_KeyValues_SetString                                  = g_api.get_signature_pkg( HASH_CT( "tf2.client.keyvalues_set_string" ) );
        g_tf2.m_KeyValues_SetInt                                     = g_api.get_signature_pkg( HASH_CT( "tf2.client.keyvalues_set_int" ) );
        g_tf2.m_CBaseFlex_ctor                                       = g_api.get_signature_pkg( HASH_CT( "tf2.client.base_flex_ctor" ) );
        g_tf2.m_CBaseEntity_SetModelByIndex                          = g_api.get_signature_pkg( HASH_CT( "tf2.client.set_model_by_index" ) );
        g_tf2.m_CBaseAnimating_FrameAdvance                          = g_api.get_signature_pkg( HASH_CT( "tf2.client.frame_advance" ) );
        g_tf2.m_CBaseAnimating_InvalidateBoneCache                   = g_api.get_signature_pkg( HASH_CT( "tf2.client.invalidate_bone_cache" ) );
        g_tf2.m_CBaseEntity_CalcAbsoluteVelocity                     = g_api.get_signature_pkg( HASH_CT( "tf2.client.calc_absolute_velocity" ) );
        g_tf2.m_CBaseAnimating_GetModelPtr                           = g_api.get_signature_pkg( HASH_CT( "tf2.client.get_model_ptr" ) );
        g_tf2.m_CBaseAnimating_SequenceDuration                      = g_api.get_signature_pkg( HASH_CT( "tf2.client.sequence_duration" ) );
        g_tf2.m_CBaseAnimating_GetSequenceActivity                   = g_api.get_signature_pkg( HASH_CT( "tf2.client.get_sequence_activity" ) );
        g_tf2.m_CStudioHdr_LookupSequence                            = g_api.get_signature_pkg( HASH_CT( "tf2.client.lookup_sequence" ) );
        g_tf2.m_Studio_BoneIndexByName                               = g_api.get_signature_pkg( HASH_CT( "tf2.client.bone_index_by_name" ) );
        g_tf2.m_BoneSetup_InitPose                                   = g_api.get_signature_pkg( HASH_CT( "tf2.client.bonesetup_init_pose" ) );
        g_tf2.m_CBoneSetup_AccumulatePose                            = g_api.get_signature_pkg( HASH_CT( "tf2.client.bonesetup_accumulate_pose" ) );
        g_tf2.m_IBoneSetup_CalcAutoplaySequences                     = g_api.get_signature_pkg( HASH_CT( "tf2.client.bonesetup_calc_autoplay_sequences" ) );
        g_tf2.m_BoneSetup_CalcBoneAdj                                = g_api.get_signature_pkg( HASH_CT( "tf2.client.bonesetup_calc_bone_adj" ) );
        g_tf2.m_CBaseAnimating_SetupBonesAttachmentHelper            = g_api.get_signature_pkg( HASH_CT( "tf2.client.setup_bones_attachment_helper" ) );
        g_tf2.m_CBaseEntity_SetAbsOrigin                             = g_api.get_signature_pkg( HASH_CT( "tf2.client.set_abs_origin" ) );
        g_tf2.m_CBaseEntity_SetAbsAngles                             = g_api.get_signature_pkg( HASH_CT( "tf2.client.set_abs_angles" ) );
        g_tf2.m_interpolate_flag                                     = g_api.get_signature_pkg( HASH_CT( "tf2.client.interpolate_flag" ) );
        g_tf2.m_enable_extrapolation_retaddr                         = g_api.get_signature_pkg( HASH_CT( "tf2.client.enable_extrapolation_retaddr" ) );
        g_tf2.m_cl_fire_events_retaddr                               = g_api.get_signature_pkg( HASH_CT( "tf2.engine.cl_fire_events_retaddr" ) );
        g_tf2.m_cl_event_queue                                       = g_api.get_signature_pkg( HASH_CT( "tf2.engine.cl_event_queue" ) );
        g_tf2.m_CStudioHdr_SequencesAvailable                        = g_api.get_signature_pkg( HASH_CT( "tf2.client.sequences_available" ) );
        g_tf2.m_CStudioHdr_GetNumSeq                                 = g_api.get_signature_pkg( HASH_CT( "tf2.client.get_num_seq" ) );
        g_tf2.m_CStudioHdr_pSeqdesc                                  = g_api.get_signature_pkg( HASH_CT( "tf2.client.pseqdesc" ) );
        g_tf2.m_CBaseEntity_SetPredictionRandomSeed                  = g_api.get_signature_pkg( HASH_CT( "tf2.client.set_prediction_random_seed" ) );
        g_tf2.m_CBasePlayer_ItemPostFrame                            = g_api.get_signature_pkg( HASH_CT( "tf2.client.item_post_frame" ) );
        g_tf2.m_MD5_PseudoRandom                                     = g_api.get_signature_pkg( HASH_CT( "tf2.client.md5_pseudo_random" ) );
        g_tf2.m_SharedRandomInt                                      = g_api.get_signature_pkg( HASH_CT( "tf2.client.shared_random_int" ) );
        g_tf2.m_prediction_player                                    = g_api.get_signature_pkg( HASH_CT( "tf2.client.prediction_player" ) );
        g_tf2.m_CBaseEntity_GetClassname                             = g_api.get_signature_pkg( HASH_CT( "tf2.client.base_entity_get_classname" ) );
        g_tf2.m_LookupWeaponInfoSlot                                 = g_api.get_signature_pkg( HASH_CT( "tf2.client.lookup_weapon_info_slot" ) );
        g_tf2.m_GetFileWeaponInfoFromHandle                          = g_api.get_signature_pkg( HASH_CT( "tf2.client.get_file_weapon_info_from_handle" ) );
        g_tf2.m_C_TFPlayer_TeamFortress_CalculateMaxSpeed            = g_api.get_signature_pkg( HASH_CT( "tf2.client.team_fortress_calc_max_speed" ) );
        g_tf2.m_CTFPlayer_GetClassEyeHeight                          = g_api.get_signature_pkg( HASH_CT( "tf2.client.get_class_eye_height" ) );
        g_tf2.m_CTFMinigun_StartMuzzleEffect                         = g_api.get_signature_pkg( HASH_CT( "tf2.client.minigun_start_muzzle_effect" ) );
        g_tf2.m_CTFMinigun_StartBrassEffect                          = g_api.get_signature_pkg( HASH_CT( "tf2.client.minigun_start_brass_effect" ) );
        g_tf2.m_GetItemSchema                                        = g_api.get_signature_pkg( HASH_CT( "tf2.client.get_item_schema" ) );
        g_tf2.m_CEconItemSchema_GetAttributeDefinition               = g_api.get_signature_pkg( HASH_CT( "tf2.client.get_attribute_definition" ) );
        g_tf2.m_CAttributeList_SetRuntimeAttributeValue              = g_api.get_signature_pkg( HASH_CT( "tf2.client.set_runtime_attribute_value" ) );
        g_tf2.m_CEconItemSchema_GetItemDefinition                    = g_api.get_signature_pkg( HASH_CT( "tf2.client.get_item_definition" ) );
        g_tf2.m_C_TFPlayer_ShouldDraw_retaddr                        = g_api.get_signature_pkg( HASH_CT( "tf2.client.ctfplayer_shoulddraw_retaddr" ) );
        g_tf2.m_C_TFWearable_ShouldDraw_retaddr                      = g_api.get_signature_pkg( HASH_CT( "tf2.client.ctfwearable_shoulddraw_retaddr" ) );
        g_tf2.m_C_TFPlayer_CreateMove_retaddr                        = g_api.get_signature_pkg( HASH_CT( "tf2.client.ctfplayer_createmove_retaddr" ) );
        g_tf2.m_C_TFPlayer_CreateMove_freeze_retaddr                 = g_api.get_signature_pkg( HASH_CT( "tf2.client.ctfplayer_createmove_freeze_retaddr" ) );
        g_tf2.m_C_TFPlayer_CreateMove_kart_retaddr                   = g_api.get_signature_pkg( HASH_CT( "tf2.client.ctfplayer_createmove_kart_retaddr" ) );
        g_tf2.m_C_TFPlayer_ValidateModelIndex_dispenser_retaddr      = g_api.get_signature_pkg( HASH_CT( "tf2.client.ctfplayer_validatemodelindex_dispenser_retaddr" ) );
        g_tf2.m_C_TFPlayer_ValidateModelIndex_disguise_retaddr       = g_api.get_signature_pkg( HASH_CT( "tf2.client.ctfplayer_validatemodelindex_disguise_retaddr" ) );
        g_tf2.m_CTFInput_ApplyMouse_kart_retaddr                     = g_api.get_signature_pkg( HASH_CT( "tf2.client.ctfinput_applymouse_kart_retaddr" ) );
        g_tf2.m_C_TFPlayerShared_IsPlayerDominated_retaddr           = g_api.get_signature_pkg( HASH_CT( "tf2.client.ctfplayershared_isplayerdominated_retaddr" ) );
        g_tf2.m_C_TFPlayerShared_IsPlayerDominated_jump              = g_api.get_signature_pkg( HASH_CT( "tf2.client.ctfplayershared_isplayerdominated_jump" ) );
        g_tf2.m_TeamStatus_GetTeam_retaddr                           = g_api.get_signature_pkg( HASH_CT( "tf2.client.team_status_get_team_retaddr" ) );
        g_tf2.m_KeyValues_SetInt_retaddr                             = g_api.get_signature_pkg( HASH_CT( "tf2.client.keyvalues_setint_retaddr" ) );
        g_tf2.m_KeyValues_SetInt_jump                                = g_api.get_signature_pkg( HASH_CT( "tf2.client.keyvalues_setint_jump" ) );
        g_tf2.m_C_TFPartyClient_SendToPartyChat                      = g_api.get_signature_pkg( HASH_CT( "tf2.client.tfpartyclient.sendpartychat" ) );
        g_tf2.m_TFPartyClient                                        = g_api.get_signature_pkg( HASH_CT( "tf2.client.tfpartyclient" ) );
        g_tf2.m_TFGCClientSystem                                     = g_api.get_signature_pkg( HASH_CT( "tf2.client.tfgcclientsystem" ) );
        g_tf2.m_CTFGCClientSystem_GetLobby                           = g_api.get_signature_pkg( HASH_CT( "tf2.client.tfgcclientsystem.get_lobby" ) );
        g_tf2.m_GameResources                                        = g_api.get_signature_pkg( HASH_CT( "tf2.client.game_resources" ) );
        g_tf2.m_CTFInventoryManager_GetItemInLoadoutForClass_retaddr = g_api.get_signature_pkg( HASH_CT( "tf2.client.getiteminloadoutforclass_retaddr" ) );
        g_tf2.m_C_TFPlayer_GetEntityForLoadoutSlot                   = g_api.get_signature_pkg( HASH_CT( "tf2.client.get_entity_for_loadout_slot" ) );
        g_tf2.m_VoiceEncoder_OPUS_create                             = g_api.get_signature_pkg( HASH_CT( "tf2.steamclient.voice_encoder_opus_create" ) );

        // Datamaps.
        // While the RTTI scanning before was cool, we're under a bit of a time pressure if we've just disconnected earlier from being in game.
        // Not all hardware is created equally, some may do this iterative stuff WAY slower than others. We have to account for everyone here.
        g_tf2.m_C_TFWeaponBase_datamap   = g_api.get_signature_pkg( HASH_CT( "tf2.client.ctfweaponbase_datamap" ) ).get_as< datamap_t * >( );
        g_tf2.m_C_WeaponMedigun_datamap  = g_api.get_signature_pkg( HASH_CT( "tf2.client.cweaponmedigun_datamap" ) ).get_as< datamap_t * >( );
        g_tf2.m_C_TFMinigun_datamap      = g_api.get_signature_pkg( HASH_CT( "tf2.client_ctfminigun_datamap" ) ).get_as< datamap_t * >( );
        g_tf2.m_C_TFFlameThrower_datamap = g_api.get_signature_pkg( HASH_CT( "tf2.client.ctfflamethrower_datamap" ) ).get_as< datamap_t * >( );
    }

    I( Sleep )( 100 );

    // Indices
    {
        g_loading.enter( load_stage_t::indices );

        g_tf2.m_chl_client_get_all_classes                            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.chl_client.get_all_classes" ) );
        g_tf2.m_chl_client_get_player_view                            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.chl_client.get_player_view" ) );
        g_tf2.m_chl_client_create_move                                = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.chl_client.create_move" ) );
        g_tf2.m_chl_client_write_usercmd_delta_to_buffer              = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.chl_client.write_usercmd_delta_to_buffer" ) );
        g_tf2.m_c_input_cam_to_first_person                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.c_input.cam_to_first_person" ) );
        g_tf2.m_c_input_cam_to_third_person                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.c_input.cam_to_third_person" ) );
        g_tf2.m_c_input_cam_is_third_person                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.c_input.cam_is_third_person" ) );
        g_tf2.m_base_player_third_person_switch                       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.base_player.third_person_switch" ) );
        g_tf2.m_client_entity_list_get_client_entity                  = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.client_entity_list.get_client_entity" ) );
        g_tf2.m_client_entity_list_get_client_entity_from_handle      = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.client_entity_list.get_client_entity_from_handle" ) );
        g_tf2.m_client_entity_list_get_highest_entity_index           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.client_entity_list.get_highest_entity_index" ) );
        g_tf2.m_engine_client_get_local_player                        = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.get_local_player" ) );
        g_tf2.m_engine_client_plat_float_time                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.plat_float_time" ) );
        g_tf2.m_engine_client_get_player_info                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.get_player_info" ) );
        g_tf2.m_engine_client_get_player_for_user_id                  = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.get_player_for_user_id" ) );
        g_tf2.m_engine_client_is_in_game                              = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.is_in_game" ) );
        g_tf2.m_engine_client_is_connected                            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.is_connected" ) );
        g_tf2.m_engine_client_is_playing_demo                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.is_playing_demo" ) );
        g_tf2.m_engine_client_get_screen_size                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.get_screen_size" ) );
        g_tf2.m_engine_client_get_view_angles                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.get_view_angles" ) );
        g_tf2.m_engine_client_set_view_angles                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.set_view_angles" ) );
        g_tf2.m_engine_client_get_max_clients                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.get_max_clients" ) );
        g_tf2.m_engine_client_get_net_channel_info                    = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.get_net_channel_info" ) );
        g_tf2.m_engine_client_is_taking_screenshot                    = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.is_taking_screenshot" ) );
        g_tf2.m_engine_client_fire_events                             = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.fire_events" ) );
        g_tf2.m_mat_system_surface_unlock_cursor                      = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.mat_system_surface.unlock_cursor" ) );
        g_tf2.m_mat_system_surface_set_cursor_always_visible          = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.mat_system_surface.set_cursor_always_visible" ) );
        g_tf2.m_mat_system_surface_play_sound                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.mat_system_surface.play_sound" ) );
        g_tf2.m_move_helper_client_process_impacts                    = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.move_helper_client.process_impacts" ) );
        g_tf2.m_move_helper_client_set_host                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.move_helper_client.set_host" ) );
        g_tf2.m_move_helper_client_reset_touch_list                   = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.move_helper_client.reset_touch_list" ) );
        g_tf2.m_prediction_get_local_view_angles                      = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.prediction.get_local_view_angles" ) );
        g_tf2.m_prediction_set_local_view_angles                      = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.prediction.set_local_view_angles" ) );
        g_tf2.m_prediction_run_command                                = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.prediction.run_command" ) );
        g_tf2.m_prediction_setup_move                                 = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.prediction.setup_move" ) );
        g_tf2.m_prediction_finish_move                                = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.prediction.finish_move" ) );
        g_tf2.m_render_view_get_matrices_for_view                     = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_view.get_matrices_for_view" ) );
        g_tf2.m_render_view_push_3d_view                              = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_view.push_3d_view" ) );
        g_tf2.m_render_view_pop_view                                  = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_view.pop_view" ) );
        g_tf2.m_render_context_bind_local_cubemap                     = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.bind_local_cubemap" ) );
        g_tf2.m_render_context_set_ambient_light                      = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.set_ambient_light" ) );
        g_tf2.m_render_context_set_ambient_light_cube                 = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.set_ambient_light_cube" ) );
        g_tf2.m_render_context_set_lighting_origin                    = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.set_lighting_origin" ) );
        g_tf2.m_material_system_find_texture                          = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material_system.find_texture" ) );
        g_tf2.m_studio_render_set_local_lights                        = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.studio_render.set_local_lights" ) );
        g_tf2.m_studio_render_set_ambient_light_colors                = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.studio_render.set_ambient_light_colors" ) );
        g_tf2.m_model_render_suppress_engine_lighting                 = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.model_render.suppress_engine_lighting" ) );
        g_tf2.m_string_table_container_remove_all_tables              = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.string_table_container.remove_all_tables" ) );
        g_tf2.m_string_table_container_find_table                     = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.string_table_container.find_table" ) );
        g_tf2.m_string_table_container_create_string_table_ex         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.string_table_container.create_string_table_ex" ) );
        g_tf2.m_string_table_add_string                               = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.string_table.add_string" ) );
        g_tf2.m_string_table_find_string_index                        = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.string_table.find_string_index" ) );
        g_tf2.m_model_loader_get_model_for_name                       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.model_loader.get_model_for_name" ) );
        g_tf2.m_key_values_system_alloc_key_values_memory             = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.key_values_system.alloc_key_values_memory" ) );
        g_tf2.m_mem_alloc_alloc                                       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.mem_alloc.alloc" ) );
        g_tf2.m_mem_alloc_free                                        = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.mem_alloc.free" ) );
        g_tf2.m_mem_alloc_realloc                                     = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.mem_alloc.realloc" ) );
        g_tf2.m_base_entity_get_pred_desc_map                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.base_entity.get_pred_desc_map" ) );
        g_tf2.m_base_entity_on_latch_interpolated_variables           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.base_entity.on_latch_interpolated_variables" ) );
        g_tf2.m_base_entity_get_solid                                 = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.base_entity.get_solid" ) );
        g_tf2.m_base_entity_should_collide                            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.base_entity.should_collide" ) );
        g_tf2.m_base_animating_get_attachment                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.base_animating.get_attachment" ) );
        g_tf2.m_gamerules_get_view_vectors                            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.gamerules.get_view_vectors" ) );
        g_tf2.m_view_render_beams_create_beam_points                  = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.view_render_beams.create_beam_points" ) );
        g_tf2.m_view_render_beams_draw_beam                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.view_render_beams.draw_beam" ) );
        g_tf2.m_model_info_get_model_name                             = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.model_info.get_model_name" ) );
        g_tf2.m_model_info_get_studio_model                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.model_info.get_studio_model" ) );
        g_tf2.m_model_info_get_model_index                            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.model_info.get_model_index" ) );
        g_tf2.m_model_info_get_vcollide                               = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.model_info.get_vcollide" ) );
        g_tf2.m_model_info_get_model_type                             = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.model_info.get_model_type" ) );
        g_tf2.m_tf_player_anim_state_update                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.tf_player_anim_state.update" ) );
        g_tf2.m_mdl_cache_find_mdl                                    = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.mdl_cache.find_mdl" ) );
        g_tf2.m_mdl_cache_get_studio_hdr                              = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.mdl_cache.get_studio_hdr" ) );
        g_tf2.m_mdl_cache_get_hardware_data                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.mdl_cache.get_hardware_data" ) );
        g_tf2.m_mdl_cache_begin_lock                                  = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.mdl_cache.begin_lock" ) );
        g_tf2.m_mdl_cache_end_lock                                    = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.mdl_cache.end_lock" ) );
        g_tf2.m_net_channel_is_loopback                               = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.net_channel.is_loopback" ) );
        g_tf2.m_net_channel_get_latency                               = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.net_channel.get_latency" ) );
        g_tf2.m_net_channel_is_timing_out                             = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.net_channel.is_timing_out" ) );
        g_tf2.m_net_channel_send_net_msg                              = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.net_channel.send_net_msg" ) );
        g_tf2.m_net_channel_set_choked                                = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.net_channel.set_choked" ) );
        g_tf2.m_net_channel_send_datagram                             = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.net_channel.send_datagram" ) );
        g_tf2.m_net_channel_can_packet                                = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.net_channel.can_packet" ) );
        g_tf2.m_cvar_find_var                                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.cvar.find_var" ) );
        g_tf2.m_cvar_get_commands                                     = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.cvar.get_commands" ) );
        g_tf2.m_demo_recorder_is_recording                            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.demo_recorder.is_recording" ) );
        g_tf2.m_demo_recorder_record_user_input                       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.demo_recorder.record_user_input" ) );
        g_tf2.m_demo_player_is_playing_back                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.demo_player.is_playing_back" ) );
        g_tf2.m_studio_render_set_color_modulation                    = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.studio_render.set_color_modulation" ) );
        g_tf2.m_studio_render_set_alpha_modulation                    = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.studio_render.set_alpha_modulation" ) );
        g_tf2.m_studio_render_forced_material_override                = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.studio_render.forced_material_override" ) );
        g_tf2.m_material_system_create_material                       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material_system.create_material" ) );
        g_tf2.m_material_system_find_material                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material_system.find_material" ) );
        g_tf2.m_material_system_get_render_context                    = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material_system.get_render_context" ) );
        g_tf2.m_material_system_first_material                        = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material_system.first_material" ) );
        g_tf2.m_material_system_next_material                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material_system.next_material" ) );
        g_tf2.m_material_system_invalid_material                      = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material_system.invalid_material" ) );
        g_tf2.m_material_system_get_material                          = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material_system.get_material" ) );
        g_tf2.m_render_context_depth_range                            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.depth_range" ) );
        g_tf2.m_render_context_clear_buffers                          = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.clear_buffers" ) );
        g_tf2.m_render_context_set_stencil_enable                     = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.set_stencil_enable" ) );
        g_tf2.m_render_context_set_stencil_fail_operation             = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.set_stencil_fail_operation" ) );
        g_tf2.m_render_context_set_stencil_zfail_operation            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.set_stencil_zfail_operation" ) );
        g_tf2.m_render_context_set_stencil_pass_operation             = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.set_stencil_pass_operation" ) );
        g_tf2.m_render_context_set_stencil_compare_function           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.set_stencil_compare_function" ) );
        g_tf2.m_render_context_set_stencil_reference_value            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.set_stencil_reference_value" ) );
        g_tf2.m_render_context_set_stencil_test_mask                  = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.set_stencil_test_mask" ) );
        g_tf2.m_render_context_set_stencil_write_mask                 = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.set_stencil_write_mask" ) );
        g_tf2.m_render_context_set_render_target                      = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.set_render_target" ) );
        g_tf2.m_render_context_viewport                               = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.viewport" ) );
        g_tf2.m_render_context_cull_mode                              = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.cull_mode" ) );
        g_tf2.m_render_context_clear_color4ub                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.clear_color4ub" ) );
        g_tf2.m_render_context_draw_screen_space_rectangle            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.draw_screen_space_rectangle" ) );
        g_tf2.m_render_context_push_render_target_and_viewport        = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.push_render_target_and_viewport" ) );
        g_tf2.m_render_context_pop_render_target_and_viewport         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.pop_render_target_and_viewport" ) );
        g_tf2.m_render_context_get_call_queue                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.get_call_queue" ) );
        g_tf2.m_render_context_get_matrix                             = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.get_matrix" ) );
        g_tf2.m_render_context_release                                = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.release" ) );
        g_tf2.m_render_context_fog_mode                               = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.fog_mode" ) );
        g_tf2.m_render_context_fog_start                              = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.fog_start" ) );
        g_tf2.m_render_context_fog_end                                = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.fog_end" ) );
        g_tf2.m_render_context_fog_color3fv                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.fog_color3fv" ) );
        g_tf2.m_render_context_fog_max_density                        = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_context.fog_max_density" ) );
        g_tf2.m_call_queue_queue_functor_internal                     = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.call_queue.queue_functor_internal" ) );
        g_tf2.m_mesh_handle_late_creation                             = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.mesh.handle_late_creation" ) );
        g_tf2.m_material_system_create_named_render_target_texture_ex = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material_system.create_named_render_target_texture_ex" ) );
        g_tf2.m_material_get_name                                     = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material.get_name" ) );
        g_tf2.m_material_get_texture_group_name                       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material.get_texture_group_name" ) );
        g_tf2.m_material_color_modulate                               = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material.color_modulate" ) );
        g_tf2.m_material_find_var                                     = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material.find_var" ) );
        g_tf2.m_material_is_error_material                            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material.is_error_material" ) );
        g_tf2.m_material_is_precached                                 = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material.is_precached" ) );
        g_tf2.m_material_precache                                     = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material.precache" ) );
        g_tf2.m_material_var_set_float_value                          = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material_var.set_float_value" ) );
        g_tf2.m_material_var_set_vec_value                            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material_var.set_vec_value" ) );
        g_tf2.m_texture_increment_reference_count                     = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.texture.increment_reference_count" ) );
        g_tf2.m_texture_decrement_reference_count                     = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.texture.decrement_reference_count" ) );
        g_tf2.m_texture_delete_if_unreferenced                        = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.texture.delete_if_unreferenced" ) );
        g_tf2.m_material_increment_reference_count                    = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material.increment_reference_count" ) );
        g_tf2.m_render_view_set_blend                                 = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_view.set_blend" ) );
        g_tf2.m_render_view_get_blend                                 = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_view.get_blend" ) );
        g_tf2.m_render_view_set_color_modulation                      = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_view.set_color_modulation" ) );
        g_tf2.m_render_view_get_color_modulation                      = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.render_view.get_color_modulation" ) );
        g_tf2.m_material_set_material_var_flag                        = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material.set_material_var_flag" ) );
        g_tf2.m_material_decrement_reference_count                    = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material.decrement_reference_count" ) );
        g_tf2.m_material_delete_if_unreferenced                       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.material.delete_if_unreferenced" ) );
        g_tf2.m_tf_weapon_base_get_weapon_id                          = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.tf_weapon_base.get_weapon_id" ) );
        g_tf2.m_tf_weapon_base_get_damage_type                        = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.tf_weapon_base.get_damage_type" ) );
        g_tf2.m_tf_weapon_base_apply_fire_delay                       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.tf_weapon_base.apply_fire_delay" ) );
        g_tf2.m_tf_weapon_base_get_slot                               = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.tf_weapon_base.get_slot" ) );
        g_tf2.m_tf_weapon_base_are_random_crits_enabled               = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.tf_weapon_base.are_random_crits_enabled" ) );
        g_tf2.m_tf_weapon_base_get_projectile_damage                  = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.tf_weapon_base.get_projectile_damage" ) );
        g_tf2.m_convar_set_value_chr                                  = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.convar.set_value_chr" ) );
        g_tf2.m_convar_set_value_fl                                   = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.convar.set_value_fl" ) );
        g_tf2.m_convar_set_value_int                                  = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.convar.set_value_int" ) );
        g_tf2.m_convar_get_name                                       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.convar.get_name" ) );
        g_tf2.m_convar_is_flag_set                                    = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.convar.is_flag_set" ) );
        g_tf2.m_engine_client_client_cmd_unrestricted                 = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.client_cmd_unrestricted" ) );
        g_tf2.m_engine_client_server_cmd_key_values                   = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.server_cmd_key_values" ) );
        g_tf2.m_engine_trace_trace_ray                                = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_trace.trace_ray" ) );
        g_tf2.m_model_info_get_vcollide_model                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.model_info.get_vcollide_model" ) );
        g_tf2.m_model_info_get_collide_for_virtual_terrain            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.model_info.get_collide_for_virtual_terrain" ) );
        g_tf2.m_model_info_get_cache_handle                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.model_info.get_cache_handle" ) );
        g_tf2.m_engine_trace_get_brushes                              = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_trace.get_brushes" ) );
        g_tf2.m_engine_trace_get_brush_info                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_trace.get_brush_info" ) );
        g_tf2.m_engine_trace_get_displacements                        = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_trace.get_displacements" ) );
        g_tf2.m_engine_trace_enumerate_entities                       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_trace.enumerate_entities" ) );
        g_tf2.m_engine_trace_get_collideable                          = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_trace.get_collideable" ) );
        g_tf2.m_engine_trace_clip_entity                              = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_trace.clip_entity" ) );
        g_tf2.m_collideable_get_model                                 = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.collideable.get_model" ) );
        g_tf2.m_collideable_get_origin                                = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.collideable.get_origin" ) );
        g_tf2.m_collideable_get_angles                                = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.collideable.get_angles" ) );
        g_tf2.m_collideable_get_transform                             = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.collideable.get_transform" ) );
        g_tf2.m_collideable_get_solid                                 = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.collideable.get_solid" ) );
        g_tf2.m_collideable_get_solid_flags                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.collideable.get_solid_flags" ) );
        g_tf2.m_physics_collision_convex_from_planes                  = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_collision.convex_from_planes" ) );
        g_tf2.m_physics_collision_convex_free                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_collision.convex_free" ) );
        g_tf2.m_physics_collision_convert_convex                      = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_collision.convert_convex" ) );
        g_tf2.m_physics_collision_destroy_collide                     = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_collision.destroy_collide" ) );
        g_tf2.m_physics_collision_get_convexes                        = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_collision.get_convexes" ) );
        g_tf2.m_physics_collision_polyhedron                          = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_collision.polyhedron" ) );
        g_tf2.m_physics_collision_create_mesh                         = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_collision.create_mesh" ) );
        g_tf2.m_physics_collision_destroy_mesh                        = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_collision.destroy_mesh" ) );
        g_tf2.m_physics_collision_trace_collide                       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_collision.trace_collide" ) );
        g_tf2.m_physics_collision_get_extent                          = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_collision.get_extent" ) );
        g_tf2.m_physics_create_environment                            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics.create_environment" ) );
        g_tf2.m_physics_destroy_environment                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics.destroy_environment" ) );
        g_tf2.m_physics_surface_props_get_surface_index               = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_surface_props.get_surface_index" ) );
        g_tf2.m_physics_surface_props_get_surface_data                = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_surface_props.get_surface_data" ) );
        g_tf2.m_physics_surface_props_get_physics_properties          = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_surface_props.get_physics_properties" ) );
        g_tf2.m_physics_env_set_gravity                               = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_env.set_gravity" ) );
        g_tf2.m_physics_env_set_air_density                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_env.set_air_density" ) );
        g_tf2.m_physics_env_create_poly_object                        = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_env.create_poly_object" ) );
        g_tf2.m_physics_env_create_poly_object_static                 = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_env.create_poly_object_static" ) );
        g_tf2.m_physics_env_destroy_object                            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_env.destroy_object" ) );
        g_tf2.m_physics_env_simulate                                  = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_env.simulate" ) );
        g_tf2.m_physics_env_set_simulation_timestep                   = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_env.set_simulation_timestep" ) );
        g_tf2.m_physics_env_reset_simulation_clock                    = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_env.reset_simulation_clock" ) );
        g_tf2.m_physics_env_set_collision_event_handler               = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_env.set_collision_event_handler" ) );
        g_tf2.m_physics_env_set_performance_settings                  = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_env.set_performance_settings" ) );
        g_tf2.m_physics_object_is_asleep                              = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_object.is_asleep" ) );
        g_tf2.m_physics_object_wake                                   = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_object.wake" ) );
        g_tf2.m_physics_object_set_drag_coefficient                   = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_object.set_drag_coefficient" ) );
        g_tf2.m_physics_object_set_position                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_object.set_position" ) );
        g_tf2.m_physics_object_get_position                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_object.get_position" ) );
        g_tf2.m_physics_object_set_velocity                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_object.set_velocity" ) );
        g_tf2.m_physics_object_get_velocity                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_object.get_velocity" ) );
        g_tf2.m_physics_collision_collide_get_aabb                    = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_collision.collide_get_aabb" ) );
        g_tf2.m_physics_collision_bbox_to_collide                     = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_collision.bbox_to_collide" ) );
        g_tf2.m_physics_collision_data_get_surface_normal             = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_collision_data.get_surface_normal" ) );
        g_tf2.m_physics_collision_data_get_contact_point              = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.physics_collision_data.get_contact_point" ) );
        g_tf2.m_static_prop_mgr_create_vphysics_representations       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.static_prop_mgr.create_vphysics_representations" ) );
        g_tf2.m_static_prop_mgr_is_static_prop_cbasehandle            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.static_prop_mgr.is_static_prop" ) );
        g_tf2.m_static_prop_mgr_get_all_in_aabb                       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.static_prop_mgr.get_all_in_aabb" ) );
        g_tf2.m_filesystem_add_search_path                            = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.filesystem.add_search_path" ) );
        g_tf2.m_filesystem_relative_path_to_full_path                 = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.filesystem.relative_path_to_full_path" ) );
        g_tf2.m_filesystem_create_dir_hierarchy                       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.filesystem.create_dir_hierarchy" ) );
        g_tf2.m_filesystem_find_first_ex                              = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.filesystem.find_first_ex" ) );
        g_tf2.m_filesystem_find_next                                  = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.filesystem.find_next" ) );
        g_tf2.m_filesystem_find_is_directory                          = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.filesystem.find_is_directory" ) );
        g_tf2.m_filesystem_find_close                                 = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.filesystem.find_close" ) );
        g_tf2.m_filesystem_open                                       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.filesystem.open" ) );
        g_tf2.m_filesystem_size                                       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.filesystem.size" ) );
        g_tf2.m_filesystem_read                                       = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.filesystem.read" ) );
        g_tf2.m_filesystem_close                                      = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.filesystem.close" ) );
        g_tf2.m_vgui_panel_get_name                                   = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.vgui_panel.get_name" ) );
        g_tf2.m_engine_vgui_is_game_ui_visible                        = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_vgui.is_game_ui_visible" ) );
        g_tf2.m_engine_client_is_taking_screenshot                    = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.is_taking_screenshot" ) );
        g_tf2.m_player_group_get_num_members                          = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.player_group.get_num_members" ) );
        g_tf2.m_player_group_get_member                               = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.player_group.get_member" ) );
        g_tf2.m_lobby_get_num_members                                 = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.lobby.get_num_members" ) );
        g_tf2.m_lobby_get_member_details                              = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.lobby.get_member_details" ) );
        g_tf2.m_engine_client_get_map_entities_string                 = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.engine_client.get_map_entities_string" ) );
        g_tf2.m_model_info_get_model                                  = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.model_info.get_model" ) );
        g_tf2.m_model_info_get_model_bounds                           = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.model_info.get_model_bounds" ) );
        g_tf2.m_econ_notification_destructor                          = g_api.get_index_pkg( HASH_CT( "vfunc.tf2.econ_notification.destructor" ) );
    }

    I( Sleep )( 100 );

#if defined( ATLAS_DEV )
    g_tf2.m_engine->ClientCmd_Unrestricted( ATLAS_XORSTR( "showconsole" ).get( ) );
#endif // ATLAS_DEV

    // ConVars and ConCommands.
    {
        g_loading.enter( load_stage_t::convars );

        {
            // Walk the CVar list. Strip specific ConCommand flags.
            // Also cache the ConCommand to our hash table for PERFORMANCE!!!
            ConCommandBase *command_base{ g_tf2.m_cvar->GetCommands( ) };
            while ( command_base )
            {
                // DBGOUT( "Got ConCommandBase: %s at 0x%x, flags: %i", command_base->m_pszName, ( uptr )command_base, command_base->m_nFlags );
                command_base->m_nFlags &= ~( FCVAR_HIDDEN | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT | FCVAR_NOT_CONNECTED );

                // This is gay.
                g_tf2.m_cmd_table.insert( HASH_RT( command_base->m_pszName ), command_base );
                command_base = command_base->m_pNext;
            }
        }

        // TODO: Either remove this or change the calls.
        g_tf2.sv_maxunlag                            = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_maxunlag" ).get( ) );
        g_tf2.sv_lagcompensation_teleport_dist       = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_lagcompensation_teleport_dist" ).get( ) );
        g_tf2.sv_gravity                             = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_gravity" ).get( ) );
        g_tf2.tf_grenadelauncher_livetime            = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_grenadelauncher_livetime" ).get( ) );
        g_tf2.tf_sticky_radius_ramp_time             = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_sticky_radius_ramp_time" ).get( ) );
        g_tf2.tf_sticky_airdet_radius                = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_sticky_airdet_radius" ).get( ) );
        g_tf2.tf_fireball_speed                      = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_fireball_speed" ).get( ) );
        g_tf2.tf_fireball_distance                   = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_fireball_distance" ).get( ) );
        g_tf2.tf_fireball_max_lifetime               = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_fireball_max_lifetime" ).get( ) );
        g_tf2.tf_flamethrower_boxsize                = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_flamethrower_boxsize" ).get( ) );
        g_tf2.sv_friction                            = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_friction" ).get( ) );
        g_tf2.sv_accelerate                          = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_accelerate" ).get( ) );
        g_tf2.sv_airaccelerate                       = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_airaccelerate" ).get( ) );
        g_tf2.tf_max_charge_speed                    = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_max_charge_speed" ).get( ) );
        g_tf2.tf_demoman_charge_drain_time           = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_demoman_charge_drain_time" ).get( ) );
        g_tf2.weapon_vaccinator_resist_duration      = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "weapon_vaccinator_resist_duration" ).get( ) );
        g_tf2.sv_stopspeed                           = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_stopspeed" ).get( ) );
        g_tf2.sv_stepsize                            = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_stepsize" ).get( ) );
        g_tf2.sv_maxvelocity                         = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_maxvelocity" ).get( ) );
        g_tf2.tf_clamp_back_speed                    = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_clamp_back_speed" ).get( ) );
        g_tf2.tf_clamp_back_speed_min                = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_clamp_back_speed_min" ).get( ) );
        g_tf2.tf_clamp_airducks                      = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_clamp_airducks" ).get( ) );
        g_tf2.tf_movement_aircurrent_friction_mult   = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_movement_aircurrent_friction_mult" ).get( ) );
        g_tf2.tf_movement_aircurrent_aircontrol_mult = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_movement_aircurrent_aircontrol_mult" ).get( ) );
        g_tf2.tf_movement_lost_footing_restick       = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_movement_lost_footing_restick" ).get( ) );
        g_tf2.tf_movement_lost_footing_friction      = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_movement_lost_footing_friction" ).get( ) );
        g_tf2.tf_parachute_maxspeed_xy               = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_parachute_maxspeed_xy" ).get( ) );
        g_tf2.tf_parachute_maxspeed_z                = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_parachute_maxspeed_z" ).get( ) );
        g_tf2.tf_parachute_aircontrol                = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_parachute_aircontrol" ).get( ) );
        g_tf2.tf_use_fixed_weaponspreads             = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_use_fixed_weaponspreads" ).get( ) );
        g_tf2.tf_weapon_criticals_melee              = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_weapon_criticals_melee" ).get( ) );
        g_tf2.tf_weapon_criticals_bucket_cap         = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_weapon_criticals_bucket_cap" ).get( ) );
        g_tf2.tf_weapon_criticals_bucket_bottom      = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_weapon_criticals_bucket_bottom" ).get( ) );
        g_tf2.tf_weapon_criticals_nopred             = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_weapon_criticals_nopred" ).get( ) );
        g_tf2.sv_usercmd_custom_random_seed          = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_usercmd_custom_random_seed" ).get( ) );
        g_tf2.developer                              = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "developer" ).get( ) );
        g_tf2.host_limitlocal                        = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "host_limitlocal" ).get( ) );
        g_tf2.sv_clockcorrection_msecs               = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_clockcorrection_msecs" ).get( ) );
        g_tf2.sv_maxusrcmdprocessticks               = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_maxusrcmdprocessticks" ).get( ) );
        g_tf2.sv_maxusrcmdprocessticks_holdaim       = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_maxusrcmdprocessticks_holdaim" ).get( ) );
        g_tf2.sv_cheats                              = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_cheats" ).get( ) );
        g_tf2.rate                                   = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "rate" ).get( ) );
        g_tf2.cl_cmdrate                             = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "cl_cmdrate" ).get( ) );
        g_tf2.cl_updaterate                          = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "cl_updaterate" ).get( ) );
        g_tf2.cl_interp_ratio                        = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "cl_interp_ratio" ).get( ) );
        g_tf2.cl_interp                              = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "cl_interp" ).get( ) );
        g_tf2.cl_smooth                              = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "cl_smooth" ).get( ) );
        g_tf2.cl_pred_optimize                       = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "cl_pred_optimize" ).get( ) );
        g_tf2.sv_lan                                 = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_lan" ).get( ) );
        g_tf2.sv_maxcmdrate                          = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_maxcmdrate" ).get( ) );
        g_tf2.sv_maxupdaterate                       = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_maxupdaterate" ).get( ) );
        g_tf2.sv_maxrate                             = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_maxrate" ).get( ) );
        g_tf2.sv_minrate                             = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_minrate" ).get( ) );
        g_tf2.sv_client_min_interp_ratio             = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_client_min_interp_ratio" ).get( ) );
        g_tf2.sv_client_max_interp_ratio             = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_client_max_interp_ratio" ).get( ) );
        g_tf2.sv_minupdaterate                       = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_minupdaterate" ).get( ) );
        g_tf2.cl_interpolate                         = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "cl_interpolate" ).get( ) );
        g_tf2.mat_queue_mode                         = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "mat_queue_mode" ).get( ) );
        g_tf2.r_threaded_particles                   = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "r_threaded_particles" ).get( ) );
        g_tf2.r_threaded_renderables                 = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "r_threaded_renderables" ).get( ) );
        g_tf2.r_hunkalloclightmaps                   = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "r_hunkalloclightmaps" ).get( ) );
        g_tf2.mat_bumpmap                            = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "mat_bumpmap" ).get( ) );
        g_tf2.mat_managedtextures                    = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "mat_managedtextures" ).get( ) );
        g_tf2.r_drawbatchdecals                      = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "r_drawbatchdecals" ).get( ) );
        g_tf2.mat_forcemanagedtextureintohardware    = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "mat_forcemanagedtextureintohardware" ).get( ) );
        g_tf2.mp_friendlyfire                        = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "mp_friendlyfire" ).get( ) );
        g_tf2.cl_windspeed                           = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "cl_windspeed" ).get( ) );
        g_tf2.cl_winddir                             = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "cl_winddir" ).get( ) );
        g_tf2.r_rainspeed                            = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "r_rainspeed" ).get( ) );
        g_tf2.r_rainwidth                            = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "r_rainwidth" ).get( ) );
        g_tf2.r_rainlength                           = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "r_rainlength" ).get( ) );
        g_tf2.sv_skyname                             = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "sv_skyname" ).get( ) );
        g_tf2.default_fov                            = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "default_fov" ).get( ) );
        g_tf2.fov_desired                            = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "fov_desired" ).get( ) );
        g_tf2.mat_postprocess_enable                 = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "mat_postprocess_enable" ).get( ) );
        g_tf2.mat_motion_blur_enabled                = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "mat_motion_blur_enabled" ).get( ) );
        g_tf2.mat_motion_blur_strength               = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "mat_motion_blur_strength" ).get( ) );
        g_tf2.cl_flipviewmodels                      = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "cl_flipviewmodels" ).get( ) );
        g_tf2.r_drawtracers                          = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "r_drawtracers" ).get( ) );
        g_tf2.r_drawtracers_firstperson              = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "r_drawtracers_firstperson" ).get( ) );
        g_tf2.cl_ragdoll_fade_time                   = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "cl_ragdoll_fade_time" ).get( ) );
        g_tf2.cl_ragdoll_physics_enable              = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "cl_ragdoll_physics_enable" ).get( ) );
        g_tf2.cl_ragdoll_forcefade                   = g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "cl_ragdoll_forcefade" ).get( ) );

        // the batch spans MAX_SHIFT - 1 ticks and only reaches the server if the channel is
        // allowed to carry it; a cmdrate under the tickrate makes the engine choke commands on its
        // own, which eats the front of the span and leaves the server re-anchoring the clock in
        // bursts the client cannot predict.. and i also got it from rijin x)
        g_tf2.rate->SetValue( 1048576 );
        g_tf2.cl_cmdrate->SetValue( 66 );
        g_tf2.cl_updaterate->SetValue( 66 );
        g_tf2.cl_interp_ratio->SetValue( 1 );
        g_tf2.cl_interp->SetValue( 0.0f );
        g_tf2.cl_smooth->SetValue( 0 );
        g_tf2.cl_pred_optimize->SetValue( 0 );

        g_tf2.sv_lan->SetValue( 1 );
        g_tf2.sv_maxcmdrate->SetValue( 66 );
        g_tf2.sv_maxupdaterate->SetValue( 66 );
        g_tf2.sv_client_min_interp_ratio->SetValue( -1 );
        g_tf2.sv_maxrate->SetValue( 0 );
        g_tf2.sv_minrate->SetValue( 80000 );

        g_tf2.mat_queue_mode->SetValue( 2 );
        g_tf2.r_threaded_particles->SetValue( 1 );
        g_tf2.r_threaded_renderables->SetValue( 1 );

        g_tf2.r_hunkalloclightmaps->SetValue( 0 );

        g_tf2.mat_managedtextures->SetValue( 1 );
        g_tf2.mat_forcemanagedtextureintohardware->SetValue( 0 );
        g_tf2.r_drawbatchdecals->SetValue( 1 );

        g_tf2.r_drawtracers->SetValue( 1 );
        g_tf2.r_drawtracers_firstperson->SetValue( 1 );

#if defined( ATLAS_DEV )
        g_tf2.developer->SetValue( 1 );
#endif // ATLAS_DEV

        if ( g_tf2.m_engine->IsInGame( ) )
        {
            g_tf2.m_engine->ClientCmd_Unrestricted( ATLAS_XORSTR( "disconnect" ).get( ) );
            DBGOUT( "Injected while in game! Disconnecting as a fail safe..." );

            I( Sleep )( 100 );
        }
    }

    I( Sleep )( 100 );

    // Feature init.
    {
        g_loading.enter( load_stage_t::netvars );

        if ( !g_netvars.load( ) )
        {
            RAISE_ERROR( "Failed to acquire required game memory.\nThe cheat may be out of date." );
            return 1;
        }

        I( Sleep )( 50 );

        g_loading.enter( load_stage_t::datamaps );

        if ( !g_datamap.init( ) )
        {
            RAISE_ERROR( "Failed to allocate required game memory.\nIf this error persists, please make a support ticket." );
            return 1;
        }

        I( Sleep )( 50 );

        g_loading.enter( load_stage_t::features );

        if ( !g_events.install( ) )
        {
            RAISE_ERROR( "Failed to acquire required game memory.\nThe cheat may be out of date." );
            return 1;
        }

        I( Sleep )( 50 );

        if ( !g_crits.install( ) )
        {
            RAISE_ERROR( "Failed to acquire required game memory.\nThe cheat may be out of date." );
            return 1;
        }

        I( Sleep )( 50 );

        if ( !player_data::install_netvar_hooks( ) )
        {
            RAISE_ERROR( "Failed to acquire required game memory.\nThe cheat may be out of date." );
            return 1;
        }

        I( Sleep )( 50 );

        g_skybox.request( );
        g_skins.request_game_tables( );
    }

    I( Sleep )( 100 );

    // Hooks
    {
        g_loading.enter( load_stage_t::hooks );

        // VFunc hooks go first, then the sigged.
        g_hooks.m_create_move                         = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.create_move" ), reinterpret_cast< void * >( &hkCreateMove ), g_tf2.m_input );
        g_hooks.m_get_usercmd                         = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.get_usercmd" ), reinterpret_cast< void * >( &hkGetUserCmd ), g_tf2.m_input );
        g_hooks.m_frame_stage_notify                  = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.frame_stage_notify" ), reinterpret_cast< void * >( &hkFrameStageNotify ), g_tf2.m_client );
        g_hooks.m_paint                               = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine_vgui.paint" ), reinterpret_cast< void * >( &hkPaint ), g_tf2.m_vgui );
        g_hooks.m_level_shutdown                      = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.level_shutdown" ), reinterpret_cast< void * >( &hkLevelShutdown ), g_tf2.m_client );
        g_hooks.m_client_shutdown                     = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.shutdown" ), reinterpret_cast< void * >( &hkClientShutdown ), g_tf2.m_client );
        g_hooks.m_voice_status                        = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.voice_status" ), reinterpret_cast< void * >( &hkVoiceStatus ), g_tf2.m_client );
        g_hooks.m_dispatch_user_message               = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.dispatch_user_message" ), reinterpret_cast< void * >( &hkDispatchUserMessage ), g_tf2.m_client );
        g_hooks.m_do_post_screen_space_effects        = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client_mode.do_post_screen_space_effects" ), reinterpret_cast< void * >( &hkDoPostScreenSpaceEffects ), g_tf2.m_client_mode );
        g_hooks.m_override_view                       = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client_mode.override_view" ), reinterpret_cast< void * >( &hkOverrideView ), g_tf2.m_client_mode );
        g_hooks.m_draw_model                          = g_api.get_hook_pkg( HASH_CT( "hook.tf2.studio_render.draw_model" ), reinterpret_cast< void * >( &hkDrawModel ), g_tf2.m_studio_render );
        g_hooks.m_internal_draw_model                 = g_api.get_hook_pkg( HASH_CT( "hook.tf2.studiorender.internal_draw_model" ), reinterpret_cast< void * >( &hkInternalDrawModel ) );
        g_hooks.m_draw_no_z_translucent               = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.draw_no_z_buffer_translucent_renderables" ), reinterpret_cast< void * >( &hkDrawNoZBufferTranslucentRenderables ) );
        g_hooks.m_level_init_pre_entity               = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.level_init_pre_entity" ), reinterpret_cast< void * >( &hkLevelInitPreEntity ), g_tf2.m_client );
        g_hooks.m_forced_material_override            = g_api.get_hook_pkg( HASH_CT( "hook.tf2.studio_render.forced_material_override" ), reinterpret_cast< void * >( &hkForcedMaterialOverride ), g_tf2.m_studio_render );
        g_hooks.m_set_color_modulation                = g_api.get_hook_pkg( HASH_CT( "hook.tf2.studio_render.set_color_modulation" ), reinterpret_cast< void * >( &hkSetColorModulation ), g_tf2.m_studio_render );
        g_hooks.m_draw_static_props                   = g_api.get_hook_pkg( HASH_CT( "hook.tf2.static_prop_mgr.draw_static_props" ), reinterpret_cast< void * >( &hkDrawStaticProps ), g_tf2.m_static_prop_mgr );
        g_hooks.m_set_cursor                          = g_api.get_hook_pkg( HASH_CT( "hook.tf2.surface.set_cursor" ), reinterpret_cast< void * >( &hkSetCursor ), g_tf2.m_surface );
        g_hooks.m_set_cursor_position                 = g_api.get_hook_pkg( HASH_CT( "hook.tf2.surface.set_cursor_position" ), reinterpret_cast< void * >( &hkSetCursorPosition ), g_tf2.m_surface );
        g_hooks.m_lock_cursor                         = g_api.get_hook_pkg( HASH_CT( "hook.tf2.surface.lock_cursor" ), reinterpret_cast< void * >( &hkLockCursor ), g_tf2.m_surface );
        g_hooks.m_is_first_time_predicted             = g_api.get_hook_pkg( HASH_CT( "hook.tf2.prediction.is_first_time_predicted" ), reinterpret_cast< void * >( &hkIsFirstTimePredicted ), g_tf2.m_prediction );
        g_hooks.m_get_local_view_angles               = g_api.get_hook_pkg( HASH_CT( "hook.tf2.prediction.get_local_view_angles" ), reinterpret_cast< void * >( &hkGetLocalViewAngles ), g_tf2.m_prediction );
        g_hooks.m_fire_event_client_side              = g_api.get_hook_pkg( HASH_CT( "hook.tf2.game_events.fire_event_client_side" ), reinterpret_cast< void * >( &hkFireEventClientSide ), g_tf2.m_game_events );
        g_hooks.m_get_model_index                     = g_api.get_hook_pkg( HASH_CT( "hook.tf2.model_info.get_model_index" ), reinterpret_cast< void * >( &hkGetModelIndex ), g_tf2.m_model_info );
        g_hooks.m_get_view_model_fov                  = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client_mode.get_view_model_fov" ), reinterpret_cast< void * >( &hkGetViewModelFOV ), g_tf2.m_client_mode );
        g_hooks.m_set_screen_overlay_material         = g_api.get_hook_pkg( HASH_CT( "hook.tf2.view_render.set_screen_overlay_material" ), reinterpret_cast< void * >( &hkSetScreenOverlayMaterial ), g_tf2.m_view_render );
        g_hooks.m_render_view                         = g_api.get_hook_pkg( HASH_CT( "hook.tf2.view_render.render_view" ), reinterpret_cast< void * >( &hkRenderView ), g_tf2.m_view_render );
        g_hooks.m_find_command_const                  = g_api.get_hook_pkg( HASH_CT( "hook.tf2.cvar.find_command_base_const" ), reinterpret_cast< void * >( &hkFindCommandBase_const ), g_tf2.m_cvar );
        g_hooks.m_find_command                        = g_api.get_hook_pkg( HASH_CT( "hook.tf2.cvar.find_command_base" ), reinterpret_cast< void * >( &hkFindCommandBase ), g_tf2.m_cvar );
        g_hooks.m_unregister_con_command              = g_api.get_hook_pkg( HASH_CT( "hook.tf2.cvar.unregister_con_command" ), reinterpret_cast< void * >( &hkUnregisterConCommand ), g_tf2.m_cvar );
        g_hooks.m_unregister_con_commands             = g_api.get_hook_pkg( HASH_CT( "hook.tf2.cvar.unregister_con_commands" ), reinterpret_cast< void * >( &hkUnregisterConCommands ), g_tf2.m_cvar );
        g_hooks.m_paint_traverse                      = g_api.get_hook_pkg( HASH_CT( "hook.tf2.panel.paint_traverse" ), reinterpret_cast< void * >( &hkPaintTraverse ), g_tf2.m_panel );
        g_hooks.m_should_draw_view_model              = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client_mode.should_draw_view_model" ), reinterpret_cast< void * >( &hkShouldDrawViewModel ), g_tf2.m_client_mode );
        g_hooks.m_get_screen_aspect_ratio             = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.get_screen_aspect_ratio" ), reinterpret_cast< void * >( &hkGetScreenAspectRatio ), g_tf2.m_engine );
        g_hooks.m_validate_ucmd                       = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.validate_user_cmd" ), reinterpret_cast< void * >( &hkValidateUserCmd ) );
        g_hooks.m_check_init_predictable              = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.check_init_predictable" ), reinterpret_cast< void * >( &hkCheckInitPredictable ) );
        g_hooks.m_s_start_sound                       = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.s_start_sound" ), reinterpret_cast< void * >( &hkS_StartSound ) );
        g_hooks.m_cl_move                             = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.cl_move" ), reinterpret_cast< void * >( &hkCL_Move ) );
        g_hooks.m_run_simulation                      = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.run_simulation" ), reinterpret_cast< void * >( &hkRunSimulation ) );
        g_hooks.m_cl_read_packets                     = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.cl_read_packets" ), reinterpret_cast< void * >( &hkCL_ReadPackets ) );
        g_hooks.m_packet_start                        = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.packet_start" ), reinterpret_cast< void * >( &hkPacketStart ) );
        g_hooks.m_get_model                           = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.get_model" ), reinterpret_cast< void * >( &hkGetModel ) );
        g_hooks.m_lightcache_get_dynamic              = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.lightcache_get_dynamic" ), reinterpret_cast< void * >( &hkLightcacheGetDynamic ) );
        g_hooks.m_update_visibility                   = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.update_visibility" ), reinterpret_cast< void * >( &hkUpdateVisibility ) );
        g_hooks.m_set_dormant                         = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.set_dormant" ), reinterpret_cast< void * >( &hkSetDormant ) );
        g_hooks.m_minigun_on_new_model                = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.minigun_on_new_model" ), reinterpret_cast< void * >( &hkMinigunOnNewModel ) );
        g_hooks.m_setup_bones                         = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.setup_bones" ), reinterpret_cast< void * >( &hkSetupBones ) );
        g_hooks.m_estimate_abs_velocity               = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.estimate_abs_velocity" ), reinterpret_cast< void * >( &hkEstimateAbsVelocity ) );
        g_hooks.m_anim_state_do_animation_event       = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.anim_state_do_animation_event" ), reinterpret_cast< void * >( &hkAnimStateDoAnimationEvent ) );
        g_hooks.m_base_animating_interpolate          = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.base_animating_interpolate" ), reinterpret_cast< void * >( &hkBaseAnimatingInterpolate ) );
        g_hooks.m_update_client_side_animation        = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.update_client_side_animation" ), reinterpret_cast< void * >( &hkUpdateClientSideAnimation ) );
        g_hooks.m_build_transformations               = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.build_transformations" ), reinterpret_cast< void * >( &hkBuildTransformations ) );
        g_hooks.m_compute_aim_yaw                     = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.compute_aim_yaw" ), reinterpret_cast< void * >( &hkComputeAimYaw ) );
        g_hooks.m_check_for_sequence_change           = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.check_for_sequence_change" ), reinterpret_cast< void * >( &hkCheckForSequenceChange ) );
        g_hooks.m_maintain_sequence_transitions       = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.maintain_sequence_transitions" ), reinterpret_cast< void * >( &hkMaintainSequenceTransitions ) );
        g_hooks.m_set_sequence                        = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.set_sequence" ), reinterpret_cast< void * >( &hkSetSequence ) );
        g_hooks.m_base_interpolate_part1              = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.base_interpolate_part1" ), reinterpret_cast< void * >( &hkBaseInterpolatePart1 ) );
        g_hooks.m_interpolate_server_entities         = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.interpolate_server_entities" ), reinterpret_cast< void * >( &hkInterpolateServerEntities ) );
        g_hooks.m_client_state_is_paused              = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.client_state_is_paused" ), reinterpret_cast< void * >( &hkIsPaused ) );
        g_hooks.m_client_state_get_time               = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.client_state_get_time" ), reinterpret_cast< void * >( &hkGetTime ) );
        g_hooks.m_net_chan_process_packet             = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.net_chan_process_packet" ), reinterpret_cast< void * >( &hkProcessPacket ) );
        g_hooks.m_net_chan_set_choked                 = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.net_chan_set_choked" ), reinterpret_cast< void * >( &hkSetChoked ) );
        g_hooks.m_net_chan_read_sub_channel_data      = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.net_chan_read_sub_channel_data" ), reinterpret_cast< void * >( &hkReadSubChannelData ) );
        g_hooks.m_net_chan_send_net_msg               = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.net_chan_send_net_msg" ), reinterpret_cast< void * >( &hkSendNetMsg ) );
        g_hooks.m_post_think                          = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.post_think" ), reinterpret_cast< void * >( &hkPostThink ) );
        g_hooks.m_check_for_pure_server_whitelist     = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.check_for_pure_server_white_list" ), reinterpret_cast< void * >( &hkCheckForPureServerWhitelist ) );
        g_hooks.m_data_table_warning                  = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.data_table_warning" ), reinterpret_cast< void * >( &hkDataTableWarning ) );
        g_hooks.m_reset_latched                       = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.reset_latched" ), reinterpret_cast< void * >( &hkResetLatched ) );
        g_hooks.m_on_client_use_consumable            = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.on_client_use_consumable" ), reinterpret_cast< void * >( &hkOnClientUseConsumable ) );
        g_hooks.m_get_max_item_count                  = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.get_max_item_count" ), reinterpret_cast< void * >( &hkGetMaxItemCount ) );
        g_hooks.m_fx_fire_bullets                     = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.fx_fire_bullets" ), reinterpret_cast< void * >( &hkFX_FireBullets ) );
        g_hooks.m_weapon_item_post_frame              = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.weapon_item_post_frame" ), reinterpret_cast< void * >( &hkWeaponItemPostFrame ) );
        g_hooks.m_sniper_item_post_frame              = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.sniper_item_post_frame" ), reinterpret_cast< void * >( &hkSniperItemPostFrame ) );
        g_hooks.m_sniper_classic_item_post_frame      = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.sniper_classic_item_post_frame" ), reinterpret_cast< void * >( &hkSniperClassicItemPostFrame ) );
        g_hooks.m_medigun_item_post_frame             = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.medigun_item_post_frame" ), reinterpret_cast< void * >( &hkMedigunItemPostFrame ) );
        g_hooks.m_flamethrower_item_post_frame        = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.flamethrower_item_post_frame" ), reinterpret_cast< void * >( &hkFlameThrowerItemPostFrame ) );
        g_hooks.m_weapon_item_busy_frame              = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.weapon_item_busy_frame" ), reinterpret_cast< void * >( &hkWeaponItemBusyFrame ) );
        g_hooks.m_apply_attribute_float_wrapper       = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.apply_attribute_float_wrapper" ), reinterpret_cast< void * >( &hkApplyAttributeFloatWrapper ) );
        g_hooks.m_attrib_hook_value_int               = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.attrib_hook_value_int" ), reinterpret_cast< void * >( &hkAttribHookValueInt ) );
        g_hooks.m_sheen_on_bind                       = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.sheen_on_bind" ), reinterpret_cast< void * >( &hkSheenOnBind ) );
        g_hooks.m_third_person_manager_update         = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.third_person_manager_update" ), reinterpret_cast< void * >( &hkThirdPersonManagerUpdate ) );
        g_hooks.m_match_invite_on_tick                = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.match_invite_on_tick" ), reinterpret_cast< void * >( &hkMatchInviteOnTick ) );
        g_hooks.m_emit_sound                          = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.emit_sound" ), reinterpret_cast< void * >( &hkEmitSound ) );
        g_hooks.m_ragdoll_on_data_changed             = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.ragdoll_on_data_changed" ), reinterpret_cast< void * >( &hkRagdollOnDataChanged ) );
        g_hooks.m_tf_is_holiday_active                = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.tf_is_holiday_active" ), reinterpret_cast< void * >( &hkTFIsHolidayActive ) );
        g_hooks.m_game_rules_is_holiday_active        = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.game_rules_is_holiday_active" ), reinterpret_cast< void * >( &hkGameRulesIsHolidayActive ) );
        g_hooks.m_calc_is_attack_critical             = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.calc_is_attack_critical" ), reinterpret_cast< void * >( &hkCalcIsAttackCritical ) );
        g_hooks.m_fire_bullet                         = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.fire_bullet" ), reinterpret_cast< void * >( &hkFireBullet ) );
        g_hooks.m_util_particle_tracer                = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.util_particle_tracer" ), reinterpret_cast< void * >( &hkUTIL_ParticleTracer ) );
        g_hooks.m_calc_view_model_view                = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.calc_view_model_view" ), reinterpret_cast< void * >( &hkCalcViewModelView ) );
        g_hooks.m_calc_view_model_bob_helper          = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.calc_view_model_bob" ), reinterpret_cast< void * >( &hkCalcViewModelBobHelper ) );
        g_hooks.m_should_flip_view_model              = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.should_flip_view_model" ), reinterpret_cast< void * >( &hkShouldFlipViewModel ) );
        g_hooks.m_hud_crosshair_get_draw_position     = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.hud_crosshair_get_draw_position" ), reinterpret_cast< void * >( &hkHudCrosshairGetDrawPosition ) );
        g_hooks.m_avoid_players                       = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.avoid_players" ), reinterpret_cast< void * >( &hkAvoidPlayers ) );
        g_hooks.m_get_client_interp_amount            = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.get_client_interp_amount" ), reinterpret_cast< void * >( &hkGetClientInterpAmount ) );
        g_hooks.m_do_image_space_motion_blur          = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.do_image_space_motion_blur" ), reinterpret_cast< void * >( &hkDoImageSpaceMotionBlur ) );
        g_hooks.m_enable_world_fog                    = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.enable_world_fog" ), reinterpret_cast< void * >( &hkEnableWorldFog ) );
        g_hooks.m_enable_3d_skybox_fog                = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.enable_3d_skybox_fog" ), reinterpret_cast< void * >( &hkEnable3dSkyboxFog ) );
        g_hooks.m_get_direct_ping_to_datacenter       = g_api.get_hook_pkg( HASH_CT( "hook.tf2.steamclient.get_direct_ping_to_datacenter" ), reinterpret_cast< void * >( &hkGetDirectPingToDataCenter ) );
        g_hooks.m_voice_get_compressed_data           = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.voice_get_compressed_data" ), reinterpret_cast< void * >( &hkVoiceGetCompressedData ) );
        g_hooks.m_in_cond                             = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.incond" ), reinterpret_cast< void * >( &hkInCond ) );
        g_hooks.m_is_player_dominated                 = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.is_player_dominated" ), reinterpret_cast< void * >( &hkIsPlayerDominated ) );
        g_hooks.m_player_panel_get_team               = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.player_panel_get_team" ), reinterpret_cast< void * >( &hkPlayerPanelGetTeam ) );
        g_hooks.m_set_int                             = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.set_int" ), reinterpret_cast< void * >( &hkSetInt ) );
        g_hooks.m_check_achievements_enabled          = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.check_achievements_enabled" ), reinterpret_cast< void * >( &hkCheckAchievementsEnabled ) );
        g_hooks.m_set_avatar_steam_id                 = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.set_avatar_steam_id" ), reinterpret_cast< void * >( &hkSetAvatarSteamID ) );
        g_hooks.m_set_player                          = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.set_player" ), reinterpret_cast< void * >( &hkSetPlayer ) );
        g_hooks.m_update_player_avatar1               = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.update_player_avatar1" ), reinterpret_cast< void * >( &hkUpdatePlayerAvatar1 ) );
        g_hooks.m_update_player_avatar2               = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.update_player_avatar2" ), reinterpret_cast< void * >( &hkUpdatePlayerAvatar2 ) );
        g_hooks.m_update_player_avatar3               = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.update_player_avatar3" ), reinterpret_cast< void * >( &hkUpdatePlayerAvatar3 ) );
        g_hooks.m_update_player_avatar4               = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.update_player_avatar4" ), reinterpret_cast< void * >( &hkUpdatePlayerAvatar4 ) );
        g_hooks.m_get_player_name                     = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.get_player_name" ), reinterpret_cast< void * >( &hkGetPlayerName ) );
        g_hooks.m_insert_and_colorize_text            = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.insert_and_colorize_text" ), reinterpret_cast< void * >( &hkInsertAndColorizeText ) );
        g_hooks.m_verify_changed_loadouts_are_valid   = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.verify_changed_loadouts_are_valid" ), reinterpret_cast< void * >( &hkVerifyChangedLoadoutsAreValid ) );
        g_hooks.m_generate_equip_region_conflict_mask = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.generate_equip_region_conflict_mask" ), reinterpret_cast< void * >( &hkGenerateEquipRegionConflictMask ) );
        g_hooks.m_get_item_in_loadout_for_class       = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.get_item_in_loadout_for_class" ), reinterpret_cast< void * >( &hkGetItemInLoadoutForClass ) );
        g_hooks.m_get_shoot_sound                     = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.get_shoot_sound" ), reinterpret_cast< void * >( &hkGetShootSound ) );
        g_hooks.m_flameball_item_post_frame           = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.flameball_item_post_frame" ), reinterpret_cast< void * >( &hkFlameballItemPostFrame ) );
        g_hooks.m_create_tf_ragdoll                   = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.create_tf_ragdoll" ), reinterpret_cast< void * >( &hkCreateTFRagdoll ) );
        g_hooks.m_cbuf_execute_command                = g_api.get_hook_pkg( HASH_CT( "hook.tf2.engine.cbuf_execute_command" ), reinterpret_cast< void * >( &hkCbuf_ExecuteCommand ) );
        g_hooks.m_notification_queue_add              = g_api.get_hook_pkg( HASH_CT( "hook.tf2.client.notification_queue_add" ), reinterpret_cast< void * >( &hkNotificationQueue_Add ) );

        if ( !g_hooks.m_create_move.enable( ) ||
             !g_hooks.m_validate_ucmd.enable( ) ||
             !g_hooks.m_frame_stage_notify.enable( ) ||
             !g_hooks.m_paint.enable( ) ||
             !g_hooks.m_get_usercmd.enable( ) ||
             !g_hooks.m_check_init_predictable.enable( ) ||
             !g_hooks.m_s_start_sound.enable( ) ||
             !g_hooks.m_level_shutdown.enable( ) ||
             !g_hooks.m_client_shutdown.enable( ) ||
             !g_hooks.m_voice_status.enable( ) ||
             !g_hooks.m_dispatch_user_message.enable( ) ||
             !g_hooks.m_cl_move.enable( ) ||
             !g_hooks.m_run_simulation.enable( ) ||
             !g_hooks.m_cl_read_packets.enable( ) ||
             !g_hooks.m_packet_start.enable( ) ||
             !g_hooks.m_draw_model.enable( ) ||
             !g_hooks.m_internal_draw_model.enable( ) ||
             !g_hooks.m_draw_no_z_translucent.enable( ) ||
             !g_hooks.m_level_init_pre_entity.enable( ) ||
             !g_hooks.m_forced_material_override.enable( ) ||
             !g_hooks.m_set_color_modulation.enable( ) ||
             !g_hooks.m_draw_static_props.enable( ) ||
             !g_hooks.m_get_model.enable( ) ||
             !g_hooks.m_lightcache_get_dynamic.enable( ) ||
             !g_hooks.m_update_visibility.enable( ) ||
             !g_hooks.m_set_dormant.enable( ) ||
             !g_hooks.m_minigun_on_new_model.enable( ) ||
             !g_hooks.m_setup_bones.enable( ) ||
             !g_hooks.m_estimate_abs_velocity.enable( ) ||
             !g_hooks.m_anim_state_do_animation_event.enable( ) ||
             !g_hooks.m_base_animating_interpolate.enable( ) ||
             !g_hooks.m_update_client_side_animation.enable( ) ||
             !g_hooks.m_build_transformations.enable( ) ||
             !g_hooks.m_compute_aim_yaw.enable( ) ||
             !g_hooks.m_check_for_sequence_change.enable( ) ||
             !g_hooks.m_maintain_sequence_transitions.enable( ) ||
             !g_hooks.m_set_sequence.enable( ) ||
             !g_hooks.m_base_interpolate_part1.enable( ) ||
             !g_hooks.m_interpolate_server_entities.enable( ) ||
             !g_hooks.m_client_state_is_paused.enable( ) ||
             !g_hooks.m_client_state_get_time.enable( ) ||
             !g_hooks.m_net_chan_process_packet.enable( ) ||
             !g_hooks.m_net_chan_set_choked.enable( ) ||
             !g_hooks.m_net_chan_read_sub_channel_data.enable( ) ||
             !g_hooks.m_net_chan_send_net_msg.enable( ) ||
             !g_hooks.m_post_think.enable( ) ||
             !g_hooks.m_do_post_screen_space_effects.enable( ) ||
             !g_hooks.m_override_view.enable( ) ||
             !g_hooks.m_check_for_pure_server_whitelist.enable( ) ||
             !g_hooks.m_data_table_warning.enable( ) ||
             !g_hooks.m_reset_latched.enable( ) ||
             !g_hooks.m_on_client_use_consumable.enable( ) ||
             !g_hooks.m_get_max_item_count.enable( ) ||
             !g_hooks.m_fx_fire_bullets.enable( ) ||
             !g_hooks.m_weapon_item_post_frame.enable( ) ||
             !g_hooks.m_sniper_item_post_frame.enable( ) ||
             !g_hooks.m_sniper_classic_item_post_frame.enable( ) ||
             !g_hooks.m_medigun_item_post_frame.enable( ) ||
             !g_hooks.m_flamethrower_item_post_frame.enable( ) ||
             !g_hooks.m_weapon_item_busy_frame.enable( ) ||
             !g_hooks.m_is_first_time_predicted.enable( ) ||
             !g_hooks.m_get_local_view_angles.enable( ) ||
             !g_hooks.m_fire_event_client_side.enable( ) ||
             !g_hooks.m_get_model_index.enable( ) ||
             !g_hooks.m_apply_attribute_float_wrapper.enable( ) ||
             !g_hooks.m_attrib_hook_value_int.enable( ) ||
             !g_hooks.m_sheen_on_bind.enable( ) ||
             !g_hooks.m_third_person_manager_update.enable( ) ||
             !g_hooks.m_match_invite_on_tick.enable( ) ||
             !g_hooks.m_emit_sound.enable( ) ||
             !g_hooks.m_ragdoll_on_data_changed.enable( ) ||
             !g_hooks.m_tf_is_holiday_active.enable( ) ||
             !g_hooks.m_game_rules_is_holiday_active.enable( ) ||
             !g_hooks.m_calc_is_attack_critical.enable( ) ||
             !g_hooks.m_fire_bullet.enable( ) ||
             !g_hooks.m_util_particle_tracer.enable( ) ||
             !g_hooks.m_calc_view_model_view.enable( ) ||
             !g_hooks.m_calc_view_model_bob_helper.enable( ) ||
             !g_hooks.m_should_flip_view_model.enable( ) ||
             !g_hooks.m_hud_crosshair_get_draw_position.enable( ) ||
             !g_hooks.m_get_view_model_fov.enable( ) ||
             !g_hooks.m_avoid_players.enable( ) ||
             !g_hooks.m_get_client_interp_amount.enable( ) ||
             !g_hooks.m_enable_world_fog.enable( ) ||
             !g_hooks.m_enable_3d_skybox_fog.enable( ) ||
             !g_hooks.m_set_screen_overlay_material.enable( ) ||
             !g_hooks.m_render_view.enable( ) ||
             !g_hooks.m_set_cursor.enable( ) ||
             !g_hooks.m_set_cursor_position.enable( ) ||
             !g_hooks.m_lock_cursor.enable( ) ||
             !g_hooks.m_get_direct_ping_to_datacenter.enable( ) ||
             !g_hooks.m_voice_get_compressed_data.enable( ) ||
             !g_hooks.m_find_command_const.enable( ) ||
             !g_hooks.m_find_command.enable( ) ||
             !g_hooks.m_unregister_con_command.enable( ) ||
             !g_hooks.m_unregister_con_commands.enable( ) ||
             !g_hooks.m_in_cond.enable( ) ||
             !g_hooks.m_paint_traverse.enable( ) ||
             !g_hooks.m_should_draw_view_model.enable( ) ||
             !g_hooks.m_is_player_dominated.enable( ) ||
             !g_hooks.m_player_panel_get_team.enable( ) ||
             !g_hooks.m_set_int.enable( ) ||
             !g_hooks.m_is_player_dominated.enable( ) ||
             !g_hooks.m_set_avatar_steam_id.enable( ) ||
             !g_hooks.m_set_player.enable( ) ||
             !g_hooks.m_update_player_avatar1.enable( ) ||
             !g_hooks.m_update_player_avatar2.enable( ) ||
             !g_hooks.m_update_player_avatar3.enable( ) ||
             !g_hooks.m_update_player_avatar4.enable( ) ||
             !g_hooks.m_get_player_name.enable( ) ||
             !g_hooks.m_insert_and_colorize_text.enable( ) ||
             !g_hooks.m_verify_changed_loadouts_are_valid.enable( ) ||
             !g_hooks.m_generate_equip_region_conflict_mask.enable( ) ||
             !g_hooks.m_get_item_in_loadout_for_class.enable( ) ||
             !g_hooks.m_get_shoot_sound.enable( ) ||
             !g_hooks.m_get_screen_aspect_ratio.enable( ) ||
             !g_hooks.m_flameball_item_post_frame.enable( ) ||
             !g_hooks.m_create_tf_ragdoll.enable( ) ||
             !g_hooks.m_cbuf_execute_command.enable( ) ||
             !g_hooks.m_notification_queue_add.enable( ) )
        {
            RAISE_ERROR( "Failed to allocate required game memory.\nThe cheat may be out of date." );
            return 1;
        }
    }

    I( Sleep )( 100 );

    if ( !exceptions::initialize_own_module( ) )
    {
        RAISE_ERROR( "Failed to initialize error handler.\nIf this error persists, please report it to a dev with CTRL+C." );
        return 1;
    }

    const auto handle = I( AddVectoredExceptionHandler )( 1, exception_filter );
    if ( !handle )
    {
        RAISE_ERROR( "Failed to allocate required game memory.\nThe cheat may be out of date." );
    }

    g_cl.m_handler = handle;

    if ( !g_steam.init( g_tf2.m_steam_api_dll, g_api ) )
    {
        RAISE_ERROR( "Failed to allocate required game memory.\nThe cheat may be out of date." );
        return 1;
    }

    // optional: without the surface only ui.html is missing, and it says so to the script
    g_html.init( g_tf2.m_steam_api_dll );

    if ( !g_api.release_runtime_package( ) )
    {
        g_api.clear_credentials( );
        // RAISE_ERROR( "Failed to release runtime startup data." );
        return 1;
    }

    // only start our workers after the remaining fallible initialization has succeeded
    // TODO: Hoku; We should make the initialization of this non-fatal.
    if ( !g_thread_pool.startup( atl::thread_pool::default_workers( ) ) )
    {
        if ( g_thread_pool.workers( ) != 0 )
        {
            const auto failure{ g_thread_pool.last_failure( ) };
            DBGOUT( "thread pool cleanup failed: operation=%u worker=%i status=0x%08x",
                    static_cast< u32 >( failure.m_kind ), failure.m_worker, static_cast< u32 >( failure.m_status ) );
            RAISE_ERROR( "Failed to allocate required game memory." );
            return 1;
        }

        DBGOUT( "thread pool: no workers, every stage runs on the caller" );
    }

    if ( !g_session.startup( ) )
    {
        I( MessageBoxA )( nullptr, ATLAS_XORSTR( "Runtime authorization failed. Please check your connection and license." ).get( ),
                          ATLAS_XORSTR( "atlas-error" ).get( ), MB_ICONERROR );
        return 1;
    }

    configs::startup( );

    g_scripts.startup( &script_api::open );
    if ( !g_session.poll( ) )
    {
        return 1;
    }

    g_cl.m_is_running = true;

    if ( !g_audio.startup( ) || !g_radio.startup( ) )
    {
        g_session.shutdown( );
        RAISE_ERROR( "Failed to allocate required game memory.\nThe cheat may be out of date." );
        return 1;
    }

    DBGOUT( "Initialized!" );

    push_notification( notify_kind_t::success, ATLAS_XORSTR_STATIC( "Atlas is loaded!" ), ATLAS_XORSTR_STATIC( "Press F3 or insert to open the menu." ) );

    return 0;
}

extern "C" ATLAS_EXPORT u32 ATLAS_STDCALL atlas_start( void *bootstrap )
{
    if ( !atl::init_hidden_threads( util::find_module( ATLAS_XORWSTR( L"ntdll.dll" ).get( ) ) ) ||
         !atl::create_hidden_thread( reinterpret_cast< atl::thread_entry_fn >( atlas_thread ), bootstrap ) )
    {
        return 1;
    }

    return 0;
}

#if defined( ATLAS_DISABLE_CLOUD )
extern "C" i32 ATLAS_STDCALL atlas_entry(
    HMODULE module,
    DWORD   reason,
    LPVOID  reserved )
{
    ATLAS_UNUSED_PARAM( reserved );

    if ( reason == DLL_PROCESS_ATTACH )
    {
        I( DisableThreadLibraryCalls )( module );

        atl::init_hidden_threads( util::find_module( ATLAS_XORWSTR( L"ntdll.dll" ).get( ) ) );
        if ( !atl::create_hidden_thread( reinterpret_cast< atl::thread_entry_fn >( atlas_thread ), nullptr ) )
        {
            return FALSE;
        }

        return TRUE;
    }

    return TRUE;
}
#endif
