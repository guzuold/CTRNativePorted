# SPDX-License-Identifier: GPL-2.0-or-later
# ctr_d3d: engine-state D3D11 backend (no shadPS4 sources), and ctr_hle (ctrport/hle) so that the
# backend runs without the ctr_gpu build tree. Build with ctrport/d3d/build.ps1 (clang-cl: the
# guest entry points use the SysV calling convention).
cmake_minimum_required(VERSION 3.24)
project(ctr_d3d LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "ctr_d3d needs clang-cl (sysv_abi guest entry points)")
endif()
add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0A00 WINVER=0x0A00)

get_filename_component(LLVM_BIN ${CMAKE_CXX_COMPILER} DIRECTORY)
file(GLOB CLANG_RT ${LLVM_BIN}/../lib/clang/*/lib/windows/clang_rt.builtins-x86_64.lib)

file(GLOB D3D_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(ctr_d3d SHARED ${D3D_SOURCES})
target_compile_options(ctr_d3d PRIVATE /EHsc -Wno-unused-function -Wno-unused-variable -Wno-unused-command-line-argument)
target_link_libraries(ctr_d3d PRIVATE user32 gdi32 d3d11 dxgi d3dcompiler ${CLANG_RT})

file(GLOB HLE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../hle/*.cpp)
add_library(ctr_hle SHARED ${HLE_SOURCES})
target_compile_definitions(ctr_hle PRIVATE CTR_HLE_BUILD)
target_compile_options(ctr_hle PRIVATE -Wno-unused-command-line-argument)
target_link_libraries(ctr_hle PRIVATE ${CLANG_RT})
