FetchContent_Declare(discord_rpc
    URL https://codeload.github.com/discord/discord-rpc/zip/963aa9f3e5ce81a4682c6ca3d136cddda614db33
    URL_HASH SHA256=a91ec58748b6c55e2e91ab6288ad24a98306da99e24637a2ea97b19c37fc7ad2
    SOURCE_SUBDIR openshao-download-only)
FetchContent_Declare(discord_rapidjson
    URL https://codeload.github.com/Tencent/rapidjson/zip/24b5e7a8b27f42fa16b96fc70aade9106cf7102f
    URL_HASH SHA256=df07f5ddfebbc2940181039f6c939ec2764a7303ef79b17958d9792a364306bb
    SOURCE_SUBDIR openshao-download-only)
# Define the static target here to keep upstream's unverified download and install rules out of the build.
FetchContent_MakeAvailable(discord_rpc discord_rapidjson)
find_package(Threads REQUIRED)
add_library(openshao_discord_rpc STATIC
    "${discord_rpc_SOURCE_DIR}/src/discord_rpc.cpp"
    "${discord_rpc_SOURCE_DIR}/src/rpc_connection.cpp"
    "${discord_rpc_SOURCE_DIR}/src/serialization.cpp")
target_include_directories(openshao_discord_rpc PUBLIC "${discord_rpc_SOURCE_DIR}/include")
target_include_directories(openshao_discord_rpc SYSTEM PRIVATE "${discord_rapidjson_SOURCE_DIR}/include")
target_link_libraries(openshao_discord_rpc PRIVATE Threads::Threads)
target_compile_definitions(openshao_discord_rpc PRIVATE _CRT_SECURE_NO_WARNINGS)
if(WIN32)
    target_sources(openshao_discord_rpc PRIVATE "${discord_rpc_SOURCE_DIR}/src/connection_win.cpp"
        "${discord_rpc_SOURCE_DIR}/src/discord_register_win.cpp")
    target_compile_definitions(openshao_discord_rpc PRIVATE DISCORD_WINDOWS)
    target_link_libraries(openshao_discord_rpc PRIVATE psapi advapi32)
else()
    target_sources(openshao_discord_rpc PRIVATE "${discord_rpc_SOURCE_DIR}/src/connection_unix.cpp"
        "${discord_rpc_SOURCE_DIR}/src/discord_register_linux.cpp")
    target_compile_definitions(openshao_discord_rpc PRIVATE DISCORD_LINUX)
endif()

add_library(openshao_discord STATIC src/discord_presence.cpp)
target_include_directories(openshao_discord PUBLIC src)
target_compile_definitions(openshao_discord PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(openshao_discord PRIVATE openshao_discord_rpc)

function(openshao_isolate_discord_tests)
    get_property(checks DIRECTORY PROPERTY TESTS)
    foreach(check IN LISTS checks)
        # This unit test replaces the external API. Every executable test must avoid the real client.
        if(NOT check STREQUAL "recomp_discord_presence")
            set_property(TEST ${check} APPEND PROPERTY ENVIRONMENT "OPENSHAO_DISABLE_DISCORD=1")
        endif()
    endforeach()
endfunction()
