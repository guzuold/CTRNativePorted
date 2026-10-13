# SPDX-License-Identifier: GPL-2.0-only
cmake_minimum_required(VERSION 3.20)
project(CTRAtrac9 LANGUAGES C)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
# Existing pinned shadPS4 ext-LibAtrac9 946e05a9212976626a9f5e52f29c0a7202871f29, MIT.
file(GLOB sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../../engine/shadPS4/externals/LibAtrac9/C/src/*.c")
add_library(ctr_atrac9 STATIC ${sources})
target_compile_definitions(ctr_atrac9 PRIVATE _CRT_SECURE_NO_WARNINGS)
