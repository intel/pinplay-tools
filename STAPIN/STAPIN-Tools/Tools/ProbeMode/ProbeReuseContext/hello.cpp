/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#include <stdio.h>
struct s {
    int a;
    float b;
    bool c;
};

int foo(int param1)
{
  int i; double d; struct s mystruct;
  printf("Hello world from foo %d\n", param1);
  return 0;
}

int foo2(int param2)
{
  printf("Hello world from foo2 %d\n", param2);
  return 0;
}

int main()
{
  struct s mystruct;
  printf("Hello world from main\n");
  for( auto i=0; i < 2; i++)
  {
    foo(i);
    foo2(i);
  }
  return 0;
}

