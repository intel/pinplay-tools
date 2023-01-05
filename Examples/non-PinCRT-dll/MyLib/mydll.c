// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
void MyFunction(const char *ptr)
{
  printf("MyFunction called\n");
  printf("\t argument '%s'\n", ptr);
}

