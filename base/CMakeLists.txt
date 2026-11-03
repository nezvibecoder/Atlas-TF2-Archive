set(ATLAS_SE_SOURCES
    steam.cpp
    steam_html.cpp
    audio/audio.cpp
    audio/codecs.cpp
    audio/stream.cpp
    input/input.cpp
    render/font.cpp
    render/gdi.cpp
    render/render.cpp
    hook/allocator.cpp
    hook/easy.cpp
    hook/inline_hook.cpp
    hook/vmt_hook.cpp
    gui/arena.cpp
    gui/gui.cpp
    gui/containers.cpp
    gui/editor.cpp
    gui/elements.cpp
    gui/picker.cpp
    gui/widgets.cpp
    gui/notifications.cpp
    gui/sounds.cpp
    gui/watermark.cpp
    gui/indicator.cpp
    gui/particles.cpp
    runtime/memory.cpp
    runtime/assert.cpp
    runtime/thread.cpp
    runtime/thread_pool.cpp
    runtime/exceptions.cpp
    runtime/integrity.cpp
    base.cpp
    util/crypto.cpp
    util/ed25519.cpp
    util/json.cpp
    util/pe_module.cpp
    api/api.cpp
    script/script.cpp
    script/bind.cpp
    script/ide.cpp
    script/lstr.cpp
)

set(ATLAS_RENDER_D3D9_SOURCES
    render/canvas.cpp
    render/overlay.cpp
)

set(ATLAS_LUA_SOURCES
    script/lua_core.cpp
    script/dtoa.cpp
    script/crt.cpp
)

set(ATLAS_LIBM_SOURCES
    libs/libm/__cos.c
    libs/libm/__math_divzero.c
    libs/libm/__math_invalid.c
    libs/libm/__math_oflow.c
    libs/libm/__math_uflow.c
    libs/libm/__math_xflow.c
    libs/libm/__rem_pio2.c
    libs/libm/__rem_pio2_large.c
    libs/libm/__sin.c
    libs/libm/__tan.c
    libs/libm/acos.c
    libs/libm/asin.c
    libs/libm/atan.c
    libs/libm/atan2.c
    libs/libm/cos.c
    libs/libm/exp.c
    libs/libm/exp_data.c
    libs/libm/fmod.c
    libs/libm/frexp.c
    libs/libm/ldexp.c
    libs/libm/log.c
    libs/libm/log10.c
    libs/libm/log_data.c
    libs/libm/pow.c
    libs/libm/pow_data.c
    libs/libm/scalbn.c
    libs/libm/sin.c
    libs/libm/tan.c
)

set_source_files_properties(${ATLAS_LIBM_SOURCES} PROPERTIES LANGUAGE CXX)

set_property(
    SOURCE ${ATLAS_LUA_SOURCES} ${ATLAS_LIBM_SOURCES}
    APPEND PROPERTY COMPILE_OPTIONS /X "-I${CMAKE_CURRENT_SOURCE_DIR}/libs/libc" $<$<CONFIG:Debug>:/O2> $<$<CONFIG:Debug>:/Ob2>
)

set_property(
    SOURCE ${ATLAS_LUA_SOURCES}
    APPEND PROPERTY COMPILE_OPTIONS /GF-
)

set_property(
    SOURCE script/lua_core.cpp
    APPEND PROPERTY COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/libs/lua"
)

set_property(
    SOURCE ${ATLAS_LIBM_SOURCES}
    APPEND PROPERTY COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/libs/libm" /FIlibm.h -Wno-reorder-init-list
)

add_library(atlas_se OBJECT ${ATLAS_SE_SOURCES} ${ATLAS_RENDER_D3D9_SOURCES} ${ATLAS_LUA_SOURCES} ${ATLAS_LIBM_SOURCES} libs/hde64.cpp)
add_library(atlas_se_x86 OBJECT ${ATLAS_SE_SOURCES} ${ATLAS_RENDER_D3D9_SOURCES} ${ATLAS_LUA_SOURCES} ${ATLAS_LIBM_SOURCES} libs/hde32.cpp)
add_library(atlas_se_d3d11 OBJECT ${ATLAS_SE_SOURCES} render/d3d11.cpp ${ATLAS_LUA_SOURCES} ${ATLAS_LIBM_SOURCES} libs/hde64.cpp)

target_link_libraries(atlas_se PRIVATE atlas_options)
target_link_libraries(atlas_se_x86 PRIVATE atlas_options_x86)
target_link_libraries(atlas_se_d3d11 PRIVATE atlas_options)
target_compile_definitions(atlas_se_d3d11 PRIVATE ATLAS_RENDER_D3D11)

set_target_properties(
    atlas_se atlas_se_x86 atlas_se_d3d11
    PROPERTIES
    CXX_EXTENSIONS OFF
    MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug>:Embedded>"
    POSITION_INDEPENDENT_CODE ON
)
