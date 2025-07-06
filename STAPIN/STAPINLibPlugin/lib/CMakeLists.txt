#
# Copyright (C) 2024-2024 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause
#

cmake_minimum_required (VERSION 3.19)

set(CMAKE_CXX_COMPILER ${PINKIT}/${ARCH}/pinrt/bin/pin-g++)

set(STAPIN_SOURCES stapin.cpp buffer_pool.cpp)

add_library(stapin STATIC ${STAPIN_SOURCES})

target_compile_definitions(stapin PRIVATE ${PIN_DEFS} ENABLE_DebugContext=1)

target_compile_features(stapin PRIVATE cxx_std_17)

target_include_directories(stapin PRIVATE
                           include
                           ${CMAKE_SOURCE_DIR}/server/include
                           ${PINKIT}/source/include/pin
                           ${PINKIT}/source/include/pin/gen
                           ${PINKIT}/extras/components/include
                           ${PINKIT}/extras/xed-${ARCH}/include/xed
                           ${PINKIT}/${ARCH}/pinrt/include/adaptor
                           )

target_compile_options(stapin PRIVATE -fPIC)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
