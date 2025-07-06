#
# Copyright (C) 2024-2024 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause
#

cmake_minimum_required (VERSION 3.19)

project (STAPIN CXX C)

if (ARCH STREQUAL "intel64")
    set(__intel64__ 1)
elseif (ARCH STREQUAL "ia32")
    set(__ia32__ 1)
else()
    message(FATAL_ERROR, "Unexpected cpu architecture: ${ARCH}, exiting...")
endif()

set(PIN_DEFS TARGET_LINUX)

add_subdirectory(tcf)
add_subdirectory(lib)
add_subdirectory(server)
add_subdirectory(examples/getSourceLocation)
add_subdirectory(examples/Symbols)
# add_subdirectory(CaseStudies/LineNumberTracker)
# add_subdirectory(CaseStudies/SymbolLocationTracker)
add_subdirectory(examples/testApps/TypeName)
add_subdirectory(examples/ExpressionEvaluation)
add_subdirectory(examples/testApps/Expressions)


