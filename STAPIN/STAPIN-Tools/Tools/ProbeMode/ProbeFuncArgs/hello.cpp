#include <stdio.h>
struct foo {
    int a;
    float b;
    bool c;
};

int func(int param1)
{
  struct foo mystruct;
  printf("Hello world from func %d\n", param1);
  return 0;
}

int main()
{
  struct foo mystruct;
  printf("Hello world from main\n");
  for( auto i=0; i < 10; i++)
    func(i);
  return 0;
}

