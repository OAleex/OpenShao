# Included after code generation and runtime adaptation in CMakeLists.txt.
find_package(Threads REQUIRED)

set(OPENSHAO_LINUX_EXCLUDE "/kernel/files\\.cpp$|/app/[a-z_]+\\.cpp$|/core/input\\.cpp$|/audio/xaudio2\\.cpp$|/gpu/(d3d12|vertex|shaders|render_thread|texturedumper)\\.cpp$|/xdk/(d3d8|xapi|dsound|xnet)\\.cpp$")
list(FILTER XBE_RUNTIME_SOURCES EXCLUDE REGEX "${OPENSHAO_LINUX_EXCLUDE}")

set(OPENSHAO_IMGUI_CORE
    "${imgui_SOURCE_DIR}/imgui.cpp" "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp" "${imgui_SOURCE_DIR}/imgui_widgets.cpp")

set(OPENSHAO_SHARED_SOURCES
    src/host.cpp src/keyboard.cpp src/game_info.cpp src/game_files.cpp src/runtime_paths.cpp
    src/game_setup.cpp src/game_import.cpp src/save_storage.cpp src/performance_overlay.cpp src/texture_pack.cpp src/texture_pack_stats.cpp src/texture_loader.cpp
    src/voice_language.cpp src/scorpion_voice.cpp src/scorpion_preview_audio.cpp src/scorpion_preview_adx.cpp src/scorpion_preview_mix.cpp src/controller.cpp src/launcher.cpp src/launcher_art.cpp src/updater.cpp "${OPENSHAO_LAUNCHER_FONTS}" src/launcher_settings.cpp src/game_preferences.cpp src/brightness.cpp
    src/menu_text.cpp src/menu_renderer.cpp src/system_text.cpp
    src/model_instances.cpp src/audio_load_wait.cpp
    src/recomp_settings_model.cpp src/recomp_settings.cpp src/native_frames.cpp src/frame_rate.cpp src/restart.cpp src/restart_process.cpp src/restart_boot.cpp
    src/mouse_state.cpp src/raster_compat.cpp src/pipeline_precompile.cpp src/pipeline_room.cpp src/pipeline_assets.cpp src/ed_nexus.cpp src/ed_nexus_progress.cpp src/dialogue_restore.cpp src/pause_recomp.cpp src/versus_input.cpp src/strip_orientation.cpp src/strip_orientation_hooks.cpp src/survival5_recovery_hooks.cpp src/aspect_ratio.cpp src/aspect_hooks.cpp src/difficulty.cpp src/difficulty_progress.cpp src/save_pages.cpp src/save_pages_book.cpp src/audio_flags.cpp src/audio_options.cpp
    src/survival_mode.cpp src/survival_session.cpp src/survival_levels.cpp src/survival_unlock.cpp
    src/language_pack.cpp src/language_archive.cpp src/language_credits.cpp src/virtual_file.cpp src/mods/runtime.cpp)

# Vulkan: pinned Khronos headers and glslang's HLSL front end; the loader is opened
# through SDL at run time (no libvulkan link).
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Vulkan.cmake")

set(OPENSHAO_LINUX_SOURCES
    src/linux/linux_main.cpp src/linux/linux_platform.cpp src/linux/linux_input.cpp
    src/linux/linux_audio.cpp src/linux/linux_launcher_window.cpp src/linux/gpu_select.cpp src/linux/gpu_null.cpp
    src/linux/vk/vk_api.cpp src/linux/vk/vk_window.cpp src/linux/vk/vk_shader.cpp src/linux/vk/vk_backend.cpp src/linux/vk/bc_decode.cpp
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp")

add_executable(OpenShao ${XBE_RUNTIME_SOURCES} ${OPENSHAO_IMGUI_CORE} ${GEN_SOURCES}
    "${HOST_ADAPTED}/boot.cpp" "${HOST_ADAPTED}/d3d8.cpp" "${HOST_ADAPTED}/xapi.cpp" "${HOST_ADAPTED}/dsound.cpp"
    "${HOST_ADAPTED}/vertex.cpp" "${HOST_ADAPTED}/shaders.cpp" "${HOST_ADAPTED}/files.cpp" "${HOST_ADAPTED}/settings.cpp"
    "${HOST_ADAPTED}/render_thread.cpp"
    ${OPENSHAO_SHARED_SOURCES} ${OPENSHAO_LINUX_SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/openshao_xbe.cpp")
add_dependencies(OpenShao openshao_codegen)
target_link_libraries(OpenShao PRIVATE openshao_audio_effects)
set_target_properties(OpenShao PROPERTIES OUTPUT_NAME OpenShao
    BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
target_include_directories(OpenShao PRIVATE src src/linux/compat resources "${OPENSHAO_RECOMP_SDK}/runtime" "${GEN}"
    "${sdl3_SOURCE_DIR}/include")
target_include_directories(OpenShao SYSTEM PRIVATE "${imgui_SOURCE_DIR}")
target_compile_definitions(OpenShao PRIVATE _CRT_SECURE_NO_WARNINGS VK_NO_PROTOTYPES IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
set_source_files_properties(src/linux/linux_main.cpp PROPERTIES COMPILE_DEFINITIONS "OPENSHAO_RECOMP_VERSION=\"${PROJECT_VERSION}\"")
target_compile_options(OpenShao PRIVATE -fno-strict-aliasing -march=x86-64-v2)
# libstdc++ and libgcc go into the executable: the player's distribution only has to supply glibc
target_link_options(OpenShao PRIVATE -fuse-ld=lld -static-libstdc++ -static-libgcc)
target_link_libraries(OpenShao PRIVATE openshao_discord openshao_support openshao_mods miniz tomlplusplus::tomlplusplus SDL3::SDL3-static Threads::Threads
    Vulkan::Headers glslang::glslang glslang::glslang-default-resource-limits ${CMAKE_DL_LIBS})

enable_testing()
function(openshao_linux_test name)
    cmake_parse_arguments(T "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name}_test ${T_SOURCES})
    target_include_directories(${name}_test PRIVATE src src/linux/compat "${OPENSHAO_RECOMP_SDK}/runtime" "${HOST_ADAPTED}")
    target_compile_definitions(${name}_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_options(${name}_test PRIVATE -fuse-ld=lld)
    if(T_LIBS)
        target_link_libraries(${name}_test PRIVATE ${T_LIBS})
    endif()
    add_test(NAME ${name} COMMAND ${name}_test)
endfunction()
add_test(NAME recomp_model_instances COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/recomp_model_instances_test.py"
    --compiler "${CMAKE_CXX_COMPILER}" --sdk "${OPENSHAO_RECOMP_SDK}" --generated "${GEN}")
openshao_linux_test(recomp_virtual_file SOURCES tests/recomp_virtual_file_test.cpp src/virtual_file.cpp)
openshao_linux_test(recomp_texture_pack SOURCES tests/recomp_texture_pack_test.cpp src/texture_pack.cpp src/texture_pack_stats.cpp)
openshao_linux_test(recomp_texture_pack_file SOURCES tests/recomp_texture_pack_file_test.cpp src/texture_pack_file.cpp src/mods/json.cpp src/mods/sha256.cpp LIBS Threads::Threads)
openshao_linux_test(recomp_texture_loader SOURCES tests/recomp_texture_loader_test.cpp src/texture_loader.cpp src/texture_pack_file.cpp src/mods/json.cpp src/mods/sha256.cpp LIBS Threads::Threads)
openshao_linux_test(recomp_language_archive SOURCES tests/recomp_language_archive_test.cpp src/language_archive.cpp src/virtual_file.cpp LIBS miniz)
openshao_linux_test(recomp_language_credits SOURCES tests/recomp_language_credits_test.cpp src/language_credits.cpp)
openshao_linux_test(recomp_language_pack SOURCES tests/recomp_language_pack_test.cpp src/language_pack.cpp src/language_archive.cpp
    src/virtual_file.cpp "${OPENSHAO_RECOMP_SDK}/runtime/kernel/crypto.cpp" LIBS miniz openshao_mods)
openshao_linux_test(recomp_launcher_settings SOURCES tests/recomp_launcher_settings_test.cpp src/launcher_settings.cpp LIBS tomlplusplus::tomlplusplus)
openshao_linux_test(recomp_mouse SOURCES tests/recomp_mouse_test.cpp src/mouse_state.cpp)
openshao_linux_test(recomp_sfd_lock SOURCES tests/recomp_sfd_lock_test.cpp)
openshao_linux_test(recomp_vblank SOURCES tests/recomp_vblank_test.cpp)
openshao_linux_test(recomp_input_mode SOURCES tests/recomp_input_mode_test.cpp)
openshao_linux_test(recomp_versus_input SOURCES tests/recomp_versus_input_test.cpp src/versus_input.cpp)
openshao_linux_test(recomp_aspect SOURCES tests/recomp_aspect_test.cpp)
openshao_linux_test(recomp_aspect_hooks SOURCES tests/recomp_aspect_hooks_test.cpp src/aspect_ratio.cpp src/launcher_settings.cpp LIBS tomlplusplus::tomlplusplus)
openshao_linux_test(recomp_menu_text SOURCES tests/recomp_menu_text_test.cpp src/menu_text.cpp src/mods/json.cpp)
target_include_directories(recomp_menu_text_test PRIVATE "${GEN}")
openshao_linux_test(recomp_menu_renderer SOURCES tests/recomp_menu_renderer_test.cpp src/menu_renderer.cpp src/menu_text.cpp src/mods/json.cpp LIBS tomlplusplus::tomlplusplus)
target_include_directories(recomp_menu_renderer_test PRIVATE "${GEN}")
openshao_linux_test(recomp_settings_model SOURCES tests/recomp_settings_model_test.cpp src/recomp_settings_model.cpp src/launcher_settings.cpp LIBS tomlplusplus::tomlplusplus openshao_mods)
openshao_linux_test(recomp_settings_ui SOURCES tests/recomp_settings_ui_test.cpp src/recomp_settings_model.cpp src/launcher_settings.cpp src/menu_text.cpp src/mods/json.cpp LIBS tomlplusplus::tomlplusplus)
target_include_directories(recomp_settings_ui_test PRIVATE "${GEN}")
openshao_linux_test(recomp_performance_overlay SOURCES tests/recomp_performance_overlay_test.cpp src/performance_overlay.cpp src/texture_pack_stats.cpp ${OPENSHAO_IMGUI_CORE})
target_include_directories(recomp_performance_overlay_test SYSTEM PRIVATE "${imgui_SOURCE_DIR}")
openshao_linux_test(recomp_input_capture SOURCES tests/recomp_input_capture_test.cpp)
add_test(NAME menu_text_catalog COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/menu_text_catalog_test.py")
openshao_linux_test(recomp_ed_nexus_sequence SOURCES tests/recomp_ed_nexus_sequence_test.cpp)
openshao_linux_test(recomp_ed_nexus_hooks SOURCES tests/recomp_ed_nexus_hooks_test.cpp)
openshao_linux_test(recomp_strip_orientation SOURCES tests/recomp_strip_orientation_test.cpp src/strip_orientation.cpp)
openshao_linux_test(recomp_strip_hooks SOURCES tests/recomp_strip_hooks_test.cpp)
openshao_linux_test(recomp_portal SOURCES tests/recomp_portal_test.cpp)
openshao_linux_test(recomp_portal_hooks SOURCES tests/recomp_portal_hooks_test.cpp)
openshao_linux_test(recomp_survival5_recovery SOURCES tests/recomp_survival5_recovery_test.cpp)
add_test(NAME language_pack_preparation COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/language_pack_test.py")
add_test(NAME recomp_package COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/recomp_package_test.py")
add_test(NAME recomp_renderer_launcher COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/recomp_renderer_launcher_test.py" --exe "$<TARGET_FILE:OpenShao>")

install(TARGETS OpenShao RUNTIME DESTINATION .)
install(FILES resources/readme.txt DESTINATION .)
install(FILES LICENSE DESTINATION licenses RENAME LICENSE.txt)
install(DIRECTORY licenses/ DESTINATION licenses)
