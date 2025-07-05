#
# Copyright (C) 2024-2024 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause
#

cmake_minimum_required (VERSION 3.19)
project (STAPINTOOL CXX C)



set(CMAKE_CXX_COMPILER ${PINKIT}/${ARCH}/pinrt/bin/pin-g++)

add_library(STAPINPerf SHARED STAPINPerf.cpp)

target_compile_definitions(STAPINPerf PRIVATE ${PIN_DEFS})

target_compile_features(STAPINPerf PRIVATE cxx_std_17)

target_include_directories(STAPINPerf PRIVATE 
    ${STAPIN_REPO}/lib/include
    ${PINKIT}/source/include/pin
    ${PINKIT}/source/include/pin/gen
    ${PINKIT}/extras/components/include
    ${PINKIT}/extras/xed-${ARCH}/include/xed
    ${PINKIT}/${ARCH}/pinrt/include/adaptor
)

target_compile_options(STAPINPerf PRIVATE -Wall -Wno-unknown-pragmas -fno-stack-protector -funwind-tables -fasynchronous-unwind-tables -fno-rtti -fPIC -DTARGET_LINUX -faligned-new)

target_link_directories(STAPINPerf PRIVATE  ${PINKIT}/${ARCH}/lib  ${PINKIT}/extras/xed-${ARCH}/lib ${STAPIN_REPO}/build/lib/)

target_link_libraries(STAPINPerf stapin pin  pinrt-adaptor-static xed pindwarf dwarf unwind-dynamic)

target_link_options(STAPINPerf PRIVATE -Wl,-Bsymbolic -Wl,--version-script=${PINKIT}/source/include/pin/pintool.ver)


