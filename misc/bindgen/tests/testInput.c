#include <stdio.h>

struct myStruct {
    int blah;
    char* blue;
}; 

enum {
    RED = 0,
    BLUE = 1,
    GREEN = 255
};

int globalVariable1 = 0;
char globalVariable2 = 0;
myStruct me;

int testFn(int (*fn) (int x, char* y), int x, char* y)
{
    return fn(x, y);
}

int main() {
  int a = 4 + 5;

  int b = 
  /* Nested /* comments */ 12; // are handled correctly

  int c =
#ifdef __APPLE__
    12
#else
    18
#endif
    ;
}
