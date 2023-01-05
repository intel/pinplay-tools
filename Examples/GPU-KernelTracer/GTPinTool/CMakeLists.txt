############################ begin_copyright_notice ############################
# Copyright (C) 2022 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
############################ end_copyright_notice ##############################

cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)   # required for setting MSVC type

project (GTPin_Shim)

set ( CMAKE_CXX_STANDARD 11 )
set ( CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" )

if (WIN32)
     set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /WX" )
elseif (LINUX)
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror" )
endif ()

###### setup architecture ######
#ARCH is not specified setting it to intel64
if (NOT DEFINED ARCH)
    set ( ARCH "intel64")
endif()
message( STATUS "ARCH = ${ARCH}")

if (WIN32)
    add_definitions(-DTARGET_WINDOWS)
elseif ( UNIX ) #linux and mac
    if ( MAC )
        add_definitions(-DTARGET_MAC)
    else ()
        add_definitions(-DTARGET_LINUX)
    endif ()
endif()

###### setup GTPin kit ######
if ( DEFINED ENV{GTPIN_KIT} )
    message( STATUS "GTPIN kit used from env: $ENV{GTPIN_KIT}" )
    set ( GTPIN_KIT ENV{GTPIN_KIT} )
    get_filename_component ( GTPIN_KIT ENV{GTPIN_KIT} ABSOLUTE )
elseif ( DEFINED GTPIN_KIT )
    message( STATUS "GTPIN kit used: ${GTPIN_KIT}" )
else ()
    message( FATAL_ERROR "Missing GTPin kit" )
endif ()

set ( INSTALL_TRG ${CMAKE_BINARY_DIR}/Bin )
if (WIN32)
    set (RUNTIME "RUNTIME") # used for install only dll's on windows
endif ()

set ( EXAMPLES          GTPinShim )

include_directories ( ../Include )
include_directories ( $ENV{GTPIN_KIT}/Examples/utils )
include_directories ( $ENV{GTPIN_KIT}/Include )
include_directories ( $ENV{GTPIN_KIT}/Include/api )
include_directories ( $ENV{GTPIN_KIT}/Include/ged/${ARCH} )
include_directories ( $ENV{GTPIN_KIT}/common )
link_directories    ( $ENV{GTPIN_KIT}/Lib/${ARCH} )

set (UTILS
            $ENV{GTPIN_KIT}/Examples/utils/gtpin_crt_utils.cpp
            $ENV{GTPIN_KIT}/Examples/utils/knob_parser.cpp
            $ENV{GTPIN_KIT}/Examples/utils/gen_send_decoder.cpp
            $ENV{GTPIN_KIT}/Examples/utils/kernel_exec_config.cpp
            )
add_library( gtpintool_utils STATIC ${UTILS} )
target_link_libraries ( gtpintool_utils gtpin ged )

set_property(TARGET gtpintool_utils PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library( GTPinShim SHARED GTPinShim.cpp )


# set required link libraries
foreach (trg ${EXAMPLES} )
    target_link_libraries ( ${trg} gtpintool_utils )
    target_link_libraries ( ${trg} gtpin ged )
    target_sources( ${trg} PUBLIC ${RESOURCE_FILE} )
endforeach (trg)

# remove auto generated 'lib' prefix for shared object
if (NOT WIN32)
    set_property( TARGET ${EXAMPLES} PROPERTY PREFIX "")
endif ()

if( ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" )
    set_property ( TARGET ${EXAMPLES}
                   PROPERTY INSTALL_RPATH "$ORIGIN/../../Lib/${ARCH}" )
endif()

install ( TARGETS ${EXAMPLES} ${RUNTIME} DESTINATION ${INSTALL_TRG} )
