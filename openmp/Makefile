#Copyright (C) 2022 Intel Corporation
#SPDX-License-Identifier: BSD-3-Clause
CC ?= gcc
CXX ?= g++

all: dotproduct-omp 

dotproduct-omp: dot_product_openmp.cpp
	$(CXX) -g -O3 -fopenmp -o dotproduct-omp dot_product_openmp.cpp 

clean:
	rm -rf ./dotproduct-omp ./*.o ./*.S ./base.exe *out *.Data *.pp global.* *.info.* *.txt whole_program.1 run.sde-eventcount*.sh

.PHONY: clean
