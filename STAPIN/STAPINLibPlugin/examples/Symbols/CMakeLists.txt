#
# Copyright (C) 2024-2024 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause
#

cmake_minimum_required (VERSION 3.19)



set(CMAKE_CXX_COMPILER ${PINKIT}/${ARCH}/pinrt/bin/pin-g++)

add_library(getSymbols SHARED getSymbolsExample.cpp)

target_compile_definitions(getSymbols PRIVATE ${PIN_DEFS})

target_compile_features(getSymbols PRIVATE cxx_std_17)

target_include_directories(getSymbols PRIVATE 
    ${CMAKE_SOURCE_DIR}/lib/include
    ${PINKIT}/source/include/pin
    ${PINKIT}/source/include/pin/gen
    ${PINKIT}/extras/components/include
    ${PINKIT}/extras/xed-${ARCH}/include/xed
    ${PINKIT}/${ARCH}/pinrt/include/adaptor
)

target_compile_options(getSymbols PRIVATE -Wall  -Wno-unknown-pragmas -fno-stack-protector -funwind-tables -fasynchronous-unwind-tables -fno-rtti -fPIC -DTARGET_LINUX -faligned-new)

target_link_directories(getSymbols PRIVATE  ${PINKIT}/${ARCH}/lib  ${PINKIT}/extras/xed-${ARCH}/lib)

target_link_libraries(getSymbols stapin pin  pinrt-adaptor-static xed pindwarf dwarf unwind-dynamic)

target_link_options(getSymbols PRIVATE -Wl,-Bsymbolic -Wl,--version-script=${PINKIT}/source/include/pin/pintool.ver)


