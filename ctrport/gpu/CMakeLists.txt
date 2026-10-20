# SPDX-License-Identifier: GPL-2.0-or-later
# ctr_gpu: shadPS4 v0.18.0 video core, shader recompiler, GnmDriver, VideoOut and kernel
# event queues built as one DLL for ctrport (same model as bbport's gpu/ library).
# Sources are compiled in place from the pinned engine/shadPS4 tree; files under shim/
# replace the shadPS4 headers that tie those libraries to the emulator core.
cmake_minimum_required(VERSION 3.24)
project(ctr_gpu LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

set(SHAD "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/shadPS4" CACHE PATH "Pinned shadPS4 v0.18.0 source tree")
set(EXT "${SHAD}/externals")
if (NOT EXISTS "${SHAD}/src/video_core/renderer_vulkan/vk_rasterizer.cpp")
    message(FATAL_ERROR "shadPS4 source tree not found at ${SHAD}")
endif()

add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE
                        _SCL_SECURE_NO_WARNINGS _TIMESPEC_DEFINED NTDDI_VERSION=0x0A000006 _WIN32_WINNT=0x0A00
                        WINVER=0x0A00 BOOST_ASIO_STANDALONE)
add_compile_options(-march=x86-64-v3)

# --- externals (shadPS4's own pinned copies) ---
add_subdirectory(${EXT}/ext-boost ext-boost EXCLUDE_FROM_ALL)
add_subdirectory(${EXT}/fmt fmt EXCLUDE_FROM_ALL)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_PIPEWIRE OFF CACHE BOOL "" FORCE)
set(SDL_AVX512F OFF CACHE BOOL "" FORCE)
add_subdirectory(${EXT}/sdl3 sdl3 EXCLUDE_FROM_ALL)
set(VULKAN_HEADERS_ENABLE_MODULE OFF CACHE BOOL "" FORCE)
add_subdirectory(${EXT}/vulkan-headers vulkan-headers EXCLUDE_FROM_ALL)
add_subdirectory(${EXT}/vma vma EXCLUDE_FROM_ALL)
set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
add_subdirectory(${EXT}/glslang glslang EXCLUDE_FROM_ALL)
file(COPY ${EXT}/glslang/SPIRV DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/glslang/glslang FILES_MATCHING PATTERN "*.h")
target_include_directories(glslang INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/glslang")
add_subdirectory(${EXT}/robin-map robin-map EXCLUDE_FROM_ALL)
add_subdirectory(${EXT}/magic_enum magic_enum EXCLUDE_FROM_ALL)
add_library(xxhash STATIC ${EXT}/xxhash/xxhash.c)
target_include_directories(xxhash PUBLIC ${EXT}/xxhash)
add_subdirectory(${EXT}/sirit sirit EXCLUDE_FROM_ALL)
target_compile_options(sirit PRIVATE -Wno-error=unused-command-line-argument)
set(BUILD_SHARED_LIBS OFF)
set(ZYDIS_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${EXT}/zydis zydis EXCLUDE_FROM_ALL)
add_subdirectory(${EXT}/xbyak xbyak EXCLUDE_FROM_ALL)
add_subdirectory(${EXT}/miniz miniz EXCLUDE_FROM_ALL)
add_library(half INTERFACE)
target_include_directories(half INTERFACE ${EXT}/half/include)

# --- host shaders: GLSL sources embedded as strings, compiled at run time by glslang ---
set(HOST_SHADER_SRC ${SHAD}/src/video_core/host_shaders)
set(HOST_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/include/video_core/host_shaders)
set(HOST_SHADER_FILES
    detilers/display_micro_64bpp.comp detilers/macro_32bpp.comp detilers/macro_64bpp.comp
    detilers/macro_8bpp.comp detilers/micro_128bpp.comp detilers/micro_16bpp.comp detilers/micro_32bpp.comp
    detilers/micro_64bpp.comp detilers/micro_8bpp.comp color_to_ms_depth.frag ms_image_blit.frag
    fault_buffer_process.comp fs_tri.vert fsr.comp post_process.frag tiling.comp)
set(HOST_SHADER_HEADERS "")
foreach(FILENAME IN ITEMS ${HOST_SHADER_FILES})
    string(REPLACE "." "_" SHADER_NAME ${FILENAME})
    set(OUT ${HOST_SHADER_DIR}/${SHADER_NAME}.h)
    add_custom_command(OUTPUT ${OUT}
        COMMAND ${CMAKE_COMMAND} -P ${HOST_SHADER_SRC}/StringShaderHeader.cmake ${HOST_SHADER_SRC}/${FILENAME} ${OUT} ${HOST_SHADER_SRC}/source_shader.h.in
        MAIN_DEPENDENCY ${HOST_SHADER_SRC}/${FILENAME} DEPENDS ${HOST_SHADER_SRC}/source_shader.h.in)
    list(APPEND HOST_SHADER_HEADERS ${OUT})
endforeach()
add_custom_target(host_shaders DEPENDS ${HOST_SHADER_HEADERS})

# --- sources ---
# Sources compiled with changes are derived by make_patched.py (exact replacements).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(PATCHED ${CMAKE_CURRENT_BINARY_DIR}/patched)
execute_process(COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/make_patched.py ${SHAD}/src ${PATCHED}
                RESULT_VARIABLE PATCH_RESULT)
if (NOT PATCH_RESULT EQUAL 0)
    message(FATAL_ERROR "make_patched.py failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/make_patched.py)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/upscaling/rasterizer_members.inc
    ${CMAKE_CURRENT_SOURCE_DIR}/upscaling/rasterizer_methods.inc)
file(GLOB_RECURSE VIDEO_CORE CONFIGURE_DEPENDS ${SHAD}/src/video_core/*.cpp)
list(FILTER VIDEO_CORE EXCLUDE REGEX "renderdoc\\.cpp$")
list(FILTER VIDEO_CORE EXCLUDE REGEX "renderer_vulkan/vk_presenter\\.cpp$")
list(APPEND VIDEO_CORE ${PATCHED}/video_core/renderer_vulkan/vk_presenter.cpp)
list(FILTER VIDEO_CORE EXCLUDE REGEX "renderer_vulkan/vk_rasterizer\\.cpp$")
list(APPEND VIDEO_CORE ${PATCHED}/video_core/renderer_vulkan/vk_rasterizer.cpp)
foreach(FILE vk_instance.cpp vk_platform.cpp)
    list(FILTER VIDEO_CORE EXCLUDE REGEX "renderer_vulkan/${FILE}$")
    list(APPEND VIDEO_CORE ${PATCHED}/video_core/renderer_vulkan/${FILE})
endforeach()
list(APPEND VIDEO_CORE ${CMAKE_CURRENT_SOURCE_DIR}/upscaling/ctr_dlss.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/upscaling/vk_dlss_ngx.cpp)
file(GLOB_RECURSE RECOMPILER CONFIGURE_DEPENDS ${SHAD}/src/shader_recompiler/*.cpp)
set(COMMON
    ${SHAD}/src/common/decoder.cpp ${SHAD}/src/common/error.cpp ${SHAD}/src/common/io_file.cpp
    ${SHAD}/src/common/native_clock.cpp ${SHAD}/src/common/ntapi.cpp ${SHAD}/src/common/number_utils.cpp
    ${SHAD}/src/common/path_util.cpp ${SHAD}/src/common/rdtsc.cpp ${SHAD}/src/common/spin_lock.cpp
    ${SHAD}/src/common/recursive_lock.cpp ${SHAD}/src/common/string_util.cpp ${SHAD}/src/common/thread.cpp
    ${SHAD}/src/common/signal_context.cpp)
set(LIBRARIES
    ${SHAD}/src/core/libraries/gnmdriver/gnmdriver.cpp
    ${PATCHED}/core/libraries/videoout/driver.cpp
    ${SHAD}/src/core/libraries/videoout/video_out.cpp
    ${SHAD}/src/core/libraries/kernel/equeue.cpp)
file(GLOB_RECURSE SHIM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shim/*.cpp)

add_library(ctr_gpu SHARED ${VIDEO_CORE} ${RECOMPILER} ${COMMON} ${LIBRARIES} ${SHIM_SOURCES})
add_dependencies(ctr_gpu host_shaders)
target_include_directories(ctr_gpu BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shim)
target_include_directories(ctr_gpu BEFORE PRIVATE ${PATCHED})
target_include_directories(ctr_gpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${SHAD}/src ${SHAD}
                           ${CMAKE_CURRENT_BINARY_DIR}/include ${EXT}/renderdoc)
target_compile_definitions(ctr_gpu PRIVATE CTR_GPU_BUILD)
target_compile_options(ctr_gpu PRIVATE -Wno-invalid-offsetof -Wno-unused-command-line-argument)
target_link_libraries(ctr_gpu PRIVATE Boost::headers fmt::fmt SDL3::SDL3-static Vulkan::Headers
                      GPUOpen::VulkanMemoryAllocator glslang::glslang glslang::SPIRV tsl::robin_map
                      magic_enum::magic_enum xxhash sirit half miniz Zydis xbyak::xbyak mincore ntdll)
# 128-bit helpers (__udivti3 and similar) emitted by clang-cl.
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    get_filename_component(LLVM_BIN ${CMAKE_CXX_COMPILER} DIRECTORY)
    file(GLOB CLANG_RT ${LLVM_BIN}/../lib/clang/*/lib/windows/clang_rt.builtins-x86_64.lib)
    target_link_libraries(ctr_gpu PRIVATE ${CLANG_RT})
endif()

# ctr_hle: system library functions written in the guest SysV ABI (ctrport/hle).
file(GLOB HLE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../hle/*.cpp)
add_library(ctr_hle SHARED ${HLE_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/../online_server/json.cpp)
target_compile_definitions(ctr_hle PRIVATE CTR_HLE_BUILD)
target_link_libraries(ctr_hle PRIVATE user32 gdi32)
target_compile_options(ctr_hle PRIVATE -Wno-unused-command-line-argument)
if (CLANG_RT)
    target_link_libraries(ctr_hle PRIVATE ${CLANG_RT})
endif()

# Unit test of the online services (R8): loads ctr_hle.dll with CTR_ONLINE=1.
add_executable(ctr_hle_online_test ${CMAKE_CURRENT_SOURCE_DIR}/../hle_tests/online_services_test.cpp)
target_link_libraries(ctr_hle_online_test PRIVATE ws2_32 crypt32)
