#include "hello.h"

#include "hello2.h"

int x;

void setX(int value) {
    x = value;
}

int getX() {
   return x;
}

void sayX() {
    printf("%d", x);
}

void sayDoubleX() {
    printf("%d", x * 2);
}
