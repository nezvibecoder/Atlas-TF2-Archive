add_library(atlas_tf2 SHARED
    entry.cpp
    global.cpp
    hooks.inl
    menu.cpp
    menu.hpp
    preview.cpp
    config.cpp
    config_storage.cpp
    config.hpp
    script_api.cpp
    sdk.cpp
    entities.cpp
    netvars.cpp

    features/anti_aim.cpp
    features/anti_backstab.cpp
    features/strafe-prediction.cpp
    features/air_blast.cpp
    features/anims.cpp
    features/auto_det.cpp
    features/auto_rocketjmp.cpp
    features/auto_vacc.cpp
    features/bone_setup.cpp
    features/lagcomp.cpp
    features/extrapolate.cpp
    features/fakelag.cpp
    features/fake_latency.cpp
    features/fast_queue.cpp
    features/prediction.cpp
    features/datamaps.cpp
    features/doors.cpp
    features/fall_damage.cpp
    features/misc.cpp
    features/player_data.cpp
    features/player_cloud.cpp
    features/player_perf.cpp
    features/party_network.cpp
    features/entity_cache.cpp
    features/esp.cpp
    features/esp_builder.cpp
    features/chams.cpp
    features/model_render.cpp
    features/glow.cpp
    features/hitmarker.cpp
    features/compression.cpp
    features/event_notifiers.cpp
    features/crits.cpp
    features/events.cpp
    features/loading.cpp
    features/networking.cpp
    features/tickbase.cpp
    features/nospread.cpp
    features/proj_camera.cpp
    features/proj_sim.cpp
    features/projectile_warning.cpp
    features/splash.cpp
    features/move_sim.cpp
    features/movement_recorder.cpp
    features/radio.cpp
    features/ragebot.cpp
    features/resolver.cpp
    features/melee.cpp
    features/holdaim_mgr.cpp
    features/interp_mgr.cpp
    features/skybox.cpp
    features/soundboard.cpp
    features/session.cpp
    features/skin_share.cpp
    features/skins.cpp
    features/streamer_mode.cpp
    features/spectator_list.cpp
    features/tracers.cpp
    features/trajectory.cpp
    features/viewmodel.cpp
    features/watchdog.cpp
    features/weather.cpp
    features/world_modulation.cpp
    features/post_process.cpp
    features/medbot.cpp

    $<TARGET_OBJECTS:atlas_se>
)

target_include_directories(atlas_tf2 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(atlas_tf2 PRIVATE atlas_options)
target_compile_definitions(atlas_tf2 PUBLIC ATLAS_TF2=1)

set(ATLAS_TF2_CHANNEL "Beta" CACHE STRING "Atlas TF2 release channel")
set_property(CACHE ATLAS_TF2_CHANNEL PROPERTY STRINGS Stable Beta)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(atlas_tf2 PRIVATE /ENTRY:atlas_entry)
    target_sources(atlas_tf2 PRIVATE dev_tables.cpp features/server_hitboxes.cpp)

    # im doing this so we can see what the performance will actually be on release builds.
    set_property(SOURCE
        features/ragebot.cpp
        features/proj_sim.cpp
        features/splash.cpp
        features/move_sim.cpp
        features/strafe-prediction.cpp
        APPEND PROPERTY COMPILE_OPTIONS /O2 /Ob2
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_link_options(atlas_tf2 PRIVATE /ENTRY:atlas_entry)
    target_sources(atlas_tf2 PRIVATE dev_tables.cpp)
else ()
    target_compile_definitions(atlas_tf2 PRIVATE ATLAS_SHIPPING)
    target_link_options(atlas_tf2 PRIVATE /NOENTRY "/MAP:${CMAKE_BINARY_DIR}/atlas-tf2.map")

    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    option(ATLAS_TF2_VMPROTECT "Virtualize TF2 startup and session code before ATLX packing" OFF)
    set(atlas_tf2_pack_options)
    set(atlas_tf2_pack_outputs "${CMAKE_BINARY_DIR}/atlas-tf2.atlx")
    if (ATLAS_TF2_VMPROTECT)
        find_program(ATLAS_VMP_CONSOLE
            NAMES VMProtect_Con.exe
            HINTS "$ENV{USERPROFILE}/Desktop/VMProtect Ultimate" "$ENV{PROGRAMFILES}/VMProtect Ultimate"
            REQUIRED
        )
        list(APPEND atlas_tf2_pack_options
            --vmprotect "${ATLAS_VMP_CONSOLE}"
            --map "${CMAKE_BINARY_DIR}/atlas-tf2.map"
            --protected-output "${CMAKE_BINARY_DIR}/atlas-tf2.vmp.dll"
        )
        list(APPEND atlas_tf2_pack_outputs
            "${CMAKE_BINARY_DIR}/atlas-tf2.vmp.dll"
            "${CMAKE_BINARY_DIR}/atlas-tf2.dll.vmp"
        )
    endif ()
    set_property(
        TARGET atlas_tf2
        PROPERTY LINK_DEPENDS "${PROJECT_SOURCE_DIR}/tools/atlas_pack.py"
    )

    add_custom_command(
        TARGET atlas_tf2
        POST_BUILD
        BYPRODUCTS ${atlas_tf2_pack_outputs}
        COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/atlas_pack.py"
        --input "$<TARGET_FILE:atlas_tf2>"
        --output "${CMAKE_BINARY_DIR}/atlas-tf2.atlx"
        ${atlas_tf2_pack_options}
        COMMENT "Packing atlas_tf2 ATLX v2 payload"
        VERBATIM
    )
endif ()

if (ATLAS_TF2_CHANNEL STREQUAL "Stable")
    target_compile_definitions(atlas_tf2 PRIVATE ATLAS_CHANNEL_STABLE)
elseif (ATLAS_TF2_CHANNEL STREQUAL "Beta")
    target_compile_definitions(atlas_tf2 PRIVATE ATLAS_CHANNEL_BETA)
else ()
    message(FATAL_ERROR "ATLAS_TF2_CHANNEL must be Stable or Beta.")
endif ()

set_target_properties(
    atlas_tf2
    PROPERTIES
    CXX_EXTENSIONS OFF
    MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug>:Embedded>"
    OUTPUT_NAME atlas-tf2
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
