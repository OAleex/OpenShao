set(OPENSHAO_SDL_HIDAPI "${sdl3_SOURCE_DIR}/src/joystick/hidapi")
set(OPENSHAO_SDL_PS4_ORIGINAL "${OPENSHAO_SDL_HIDAPI}/SDL_hidapi_ps4.c")
set(OPENSHAO_SDL_PS4_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/adapted/SDL_hidapi_ps4.c")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare_sdl_controller.py"
    --source "${OPENSHAO_SDL_PS4_ORIGINAL}" --output "${OPENSHAO_SDL_PS4_SOURCE}"
    COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare_sdl_controller.py" "${OPENSHAO_SDL_PS4_ORIGINAL}")
set_source_files_properties("${OPENSHAO_SDL_PS4_ORIGINAL}" DIRECTORY "${sdl3_SOURCE_DIR}"
    PROPERTIES HEADER_FILE_ONLY TRUE)
set_source_files_properties("${OPENSHAO_SDL_PS4_SOURCE}" DIRECTORY "${sdl3_SOURCE_DIR}"
    PROPERTIES INCLUDE_DIRECTORIES "${OPENSHAO_SDL_HIDAPI}")
target_sources(SDL3-static PRIVATE "${OPENSHAO_SDL_PS4_SOURCE}")

# Exercise the shipped driver's real report loop with isolated HID I/O.
add_executable(recomp_ps4_transport_test tests/recomp_ps4_transport_test.c)
target_include_directories(recomp_ps4_transport_test PRIVATE
    "${sdl3_SOURCE_DIR}/src" "${OPENSHAO_SDL_HIDAPI}"
    "${sdl3_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>/build_config")
target_compile_definitions(recomp_ps4_transport_test PRIVATE SDL_STATIC_LIB USING_GENERATED_CONFIG_H
    OPENSHAO_SDL_PS4_SOURCE="${OPENSHAO_SDL_PS4_SOURCE}")
target_link_options(recomp_ps4_transport_test PRIVATE -fuse-ld=lld)
target_link_libraries(recomp_ps4_transport_test PRIVATE SDL3::SDL3-static)
add_test(NAME recomp_ps4_transport COMMAND recomp_ps4_transport_test)
