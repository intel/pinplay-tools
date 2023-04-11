#!/bin/bash
# Copyright (C) 2022 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
cmake .. -DCMAKE_BUILD_TYPE=Debug -DARCH=intel64 -DGTPIN_KIT=$GTPIN_KIT
make clean; make
