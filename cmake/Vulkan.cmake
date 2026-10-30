# Pinned shader compiler and headers shared by both desktop targets. The
# system Vulkan loader is opened at runtime only when the renderer is used.
include_guard(GLOBAL)
FetchContent_Declare(vulkan_headers GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG 2cd90f9d20df57eac214c148f3aed885372ddcfe)  # v1.4.321
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL ON CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glslang GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
    GIT_TAG 8a85691a0740d390761a1008b4696f57facd02c4)  # 15.4.0
FetchContent_MakeAvailable(vulkan_headers glslang)
