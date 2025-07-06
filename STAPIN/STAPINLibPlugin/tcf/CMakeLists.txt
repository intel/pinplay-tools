#
# Copyright (C) 2024-2024 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause
#

cmake_minimum_required (VERSION 3.19)

if(NOT __machine__)
    set(__machine__ ${CMAKE_SYSTEM_PROCESSOR})
endif()

set(TCF_SRC_ROOT ${CMAKE_SOURCE_DIR}/external/tcf.agent/agent)

set(TCF_SOURCES_DIRS 
    system/PINOS/tcf
    machine/${__machine__}/tcf
    ${TCF_SRC_ROOT}/tcf
    ${TCF_SRC_ROOT}/tcf/framework
    ${TCF_SRC_ROOT}/tcf/services
    ${TCF_SRC_ROOT}/tcf/main
    $(TCF_SRC_ROOT)/tcf/http
)

# add default sources
list(TRANSFORM TCF_SOURCES_DIRS APPEND /*.c OUTPUT_VARIABLE BASE_GLOBS)
file(GLOB TCF_SOURCES ${BASE_GLOBS})

# remove non-library files
file(GLOB TCF_MAIN_SRCS ${TCF_SRC_ROOT}
	${TCF_SRC_ROOT}/tcf/main/main*.c
    ${TCF_SRC_ROOT}/tcf/framework/context-dispatcher.c)
list(REMOVE_ITEM TCF_SOURCES ${TCF_MAIN_SRCS})


set(TCF_INC_DIRS
    ${TCF_SRC_ROOT}
    system/PINOS
    machine/${__machine__}
)

add_library(tcf STATIC ${TCF_SOURCES})

target_include_directories(tcf PRIVATE ${TCF_INC_DIRS})

target_compile_definitions(tcf PRIVATE ENABLE_SSL=0 USE_uuid_generate=0 ENABLE_HttpServer=0 ENABLE_LibWebSockets=0 ENABLE_DebugContext=1)

target_compile_options(tcf PRIVATE -fPIC)