#Copyright (C) 2022 Intel Corporation
#SPDX-License-Identifier: BSD-3-Clause
CC ?= gcc
CXX ?= g++

all: dotproduct-omp dotproduct-st

dotproduct-omp: dot_product_openmp.cpp
	$(CXX) -g -O3 -fopenmp -o dotproduct-omp dot_product_openmp.cpp 

dotproduct-st: dot_product_st.cpp
	$(CXX) -g -O3 -o dotproduct-st dot_product_st.cpp 

clean:
	rm -rf ./dotproduct-omp ./dotproduct-st ./*.o ./*.S ./base.exe *out *.Data *.pp global.* *.info.* *.txt whole_program.1 run.sde-eventcount*.sh

.PHONY: clean
