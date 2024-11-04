#Copyright (C) 2022 Intel Corporation
#SPDX-License-Identifier: BSD-3-Clause
CC ?= gcc
CXX ?= g++

all: dotproduct-st

dotproduct-st: dot_product_st.cpp
	$(CXX) -g -O3 -o dotproduct-st dot_product_st.cpp 

clean:
	rm -rf ./dotproduct-st ./*.o ./*.S ./base.exe *out *.Data *.pp global.* *.info.* *.txt whole_program.1 run.sde-eventcount*.sh

.PHONY: clean
