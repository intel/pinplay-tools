#
# Copyright (C) 2024-2024 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause
#
cmake_minimum_required (VERSION 3.19)

# Set the project name
project(ExpressionsTest CXX)

# Add the executable
add_executable(ExpressionsTest expressionsTest.cpp)

target_compile_options(ExpressionsTest PRIVATE -g)



