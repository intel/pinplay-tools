#
# Copyright (C) 2024-2024 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause
#

cmake_minimum_required (VERSION 3.19)


set(STAPIN_PLUGIN_SOURCES stapin-plugin.cpp 
                          stapin-context.cpp 
                          stapin-plugin-symbols-and-expressions-logic.cpp 
                          stapin-plugin-expressions-logic.cpp 
                          stapin-plugin-symbols-logic.cpp 
                          stapin-plugin-image-logic.cpp 
                          stapin-plugin-TCF-and-context-logic.cpp 
                          stapin-plugin-sourceLocation-logic.cpp)

add_library(stapin-plugin SHARED ${STAPIN_PLUGIN_SOURCES})

target_compile_features(stapin-plugin PRIVATE cxx_std_17)

set(TCF_SRC_ROOT ${CMAKE_SOURCE_DIR}/external/tcf.agent/agent)

target_include_directories(stapin-plugin PRIVATE 
                        include
                        ${TCF_SRC_ROOT}
                        ${PINKIT}/${ARCH}/pinrt/include/adaptor
                        )



target_compile_definitions(stapin-plugin PRIVATE TARGET_LINUX  )
if(__intel64__)
target_compile_definitions(stapin-plugin PRIVATE TARGET_IA32E)
elseif(__ia32__)
target_compile_definitions(stapin-plugin PRIVATE TARGET_IA32)
endif()

target_link_libraries(stapin-plugin tcf)

install(TARGETS stapin-plugin
        LIBRARY DESTINATION ${PINKIT}/${ARCH}/pinrt/bin/plugins)



