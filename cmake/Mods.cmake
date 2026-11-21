# Shared storage and mod installation, used by both targets and the standalone tests.
FetchContent_Declare(openshao_lua
    URL https://www.lua.org/ftp/lua-5.4.9.tar.gz
    URL_HASH SHA256=2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6)
FetchContent_MakeAvailable(openshao_lua)
file(GLOB OPENSHAO_LUA_SOURCES "${openshao_lua_SOURCE_DIR}/src/*.c")
list(FILTER OPENSHAO_LUA_SOURCES EXCLUDE REGEX "/(lua|luac)\\.c$")
add_library(openshao_lua_runtime STATIC ${OPENSHAO_LUA_SOURCES})
target_include_directories(openshao_lua_runtime PUBLIC "${openshao_lua_SOURCE_DIR}/src")
target_compile_definitions(openshao_lua_runtime PRIVATE _CRT_SECURE_NO_WARNINGS)
FetchContent_Declare(xz_embedded
    URL https://codeload.github.com/tukaani-project/xz-embedded/zip/ae63ae3a36ed01724674e8f3d750dc47bf125410
    URL_HASH SHA256=def1cc76f59db117245670f334599d70406c5f8a3ea99fb79763a84803420abf)
FetchContent_MakeAvailable(xz_embedded)
add_library(openshao_xz STATIC
    "${xz_embedded_SOURCE_DIR}/linux/lib/xz/xz_crc32.c"
    "${xz_embedded_SOURCE_DIR}/linux/lib/xz/xz_crc64.c"
    "${xz_embedded_SOURCE_DIR}/linux/lib/xz/xz_dec_lzma2.c"
    "${xz_embedded_SOURCE_DIR}/linux/lib/xz/xz_dec_stream.c")
target_include_directories(openshao_xz PUBLIC "${xz_embedded_SOURCE_DIR}/linux/include/linux"
    PRIVATE "${xz_embedded_SOURCE_DIR}/userspace")
target_compile_definitions(openshao_xz PUBLIC XZ_USE_CRC64 PRIVATE XZ_DEC_DYNALLOC)
add_library(openshao_storage STATIC src/file_ops.cpp src/path_preferences.cpp src/save_storage.cpp src/legacy_folders.cpp)
target_include_directories(openshao_storage PUBLIC src)
target_link_libraries(openshao_storage PUBLIC tomlplusplus::tomlplusplus)
target_compile_definitions(openshao_storage PRIVATE _CRT_SECURE_NO_WARNINGS)
add_library(openshao_mods STATIC src/mods/sha256.cpp src/mods/json.cpp src/mods/catalog.cpp
    src/mods/vcdiff.cpp src/mods/tar_xz.cpp src/mods/state.cpp src/mods/mod_settings.cpp src/mods/manager.cpp
    src/mods/game_reader.cpp src/mods/http.cpp src/mods/browser.cpp src/mods/activation.cpp src/mods/routines.cpp src/mods/releases.cpp
    src/texture_pack_file.cpp)
target_include_directories(openshao_mods PUBLIC src PRIVATE "${OPENSHAO_RECOMP_SDK}/runtime")
target_link_libraries(openshao_mods PUBLIC openshao_storage PRIVATE openshao_xz openshao_lua_runtime)
target_compile_definitions(openshao_mods PRIVATE _CRT_SECURE_NO_WARNINGS)
if(WIN32)
    target_link_libraries(openshao_storage PUBLIC shell32 ole32 uuid)
    target_link_libraries(openshao_mods PRIVATE winhttp)
else()
    target_link_libraries(openshao_mods PRIVATE ${CMAKE_DL_LIBS})
endif()
enable_testing()
add_executable(recomp_zip64_test tests/recomp_zip64_test.cpp)
target_link_libraries(recomp_zip64_test PRIVATE miniz)
add_test(NAME recomp_zip64 COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/zip64_test.py" "$<TARGET_FILE:recomp_zip64_test>")
add_executable(recomp_path_preferences_test tests/recomp_path_preferences_test.cpp)
target_link_libraries(recomp_path_preferences_test PRIVATE openshao_storage)
add_test(NAME recomp_path_preferences COMMAND recomp_path_preferences_test)
add_executable(recomp_legacy_folders_test tests/recomp_legacy_folders_test.cpp)
target_link_libraries(recomp_legacy_folders_test PRIVATE openshao_storage)
add_test(NAME recomp_legacy_folders COMMAND recomp_legacy_folders_test)
add_executable(recomp_mod_formats_test tests/recomp_mod_formats_test.cpp)
target_link_libraries(recomp_mod_formats_test PRIVATE openshao_mods)
add_test(NAME recomp_mod_formats COMMAND recomp_mod_formats_test)
add_executable(recomp_mod_manager_test tests/recomp_mod_manager_test.cpp)
target_link_libraries(recomp_mod_manager_test PRIVATE openshao_mods)
add_test(NAME recomp_mod_manager COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/mod_manager_test.py" "$<TARGET_FILE:recomp_mod_manager_test>")
add_executable(recomp_mod_game_test tests/recomp_mod_game_test.cpp
    "${OPENSHAO_RECOMP_SDK}/runtime/kernel/disc.cpp" "${OPENSHAO_RECOMP_SDK}/runtime/core/log.cpp")
target_include_directories(recomp_mod_game_test PRIVATE "${OPENSHAO_RECOMP_SDK}/runtime")
target_compile_definitions(recomp_mod_game_test PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(recomp_mod_game_test PRIVATE openshao_mods)
add_executable(recomp_mod_http_test tests/recomp_mod_http_test.cpp)
target_link_libraries(recomp_mod_http_test PRIVATE openshao_mods)
add_executable(recomp_mod_browser_test tests/recomp_mod_browser_test.cpp
    "${OPENSHAO_RECOMP_SDK}/runtime/kernel/disc.cpp" "${OPENSHAO_RECOMP_SDK}/runtime/core/log.cpp")
target_include_directories(recomp_mod_browser_test PRIVATE "${OPENSHAO_RECOMP_SDK}/runtime")
target_compile_definitions(recomp_mod_browser_test PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(recomp_mod_browser_test PRIVATE openshao_mods)
add_test(NAME recomp_mod_browser COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/mod_browser_test.py" "$<TARGET_FILE:recomp_mod_browser_test>" "${OPENSHAO_GAME_XBE}")
add_executable(recomp_mod_routines_test tests/recomp_mod_routines_test.cpp)
target_include_directories(recomp_mod_routines_test PRIVATE "${OPENSHAO_RECOMP_SDK}/runtime")
target_link_libraries(recomp_mod_routines_test PRIVATE openshao_mods)
add_test(NAME recomp_mod_routines COMMAND recomp_mod_routines_test)
add_test(NAME recomp_mod_targets COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/mod_targets_test.py")
