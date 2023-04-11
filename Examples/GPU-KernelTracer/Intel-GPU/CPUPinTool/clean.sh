#!/bin/bash
# Copyright (C) 2022 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
set -x
rm -r -f pin.log obj-*
cd ../GTPinTool/build
./clean.sh
