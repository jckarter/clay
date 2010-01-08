#ifndef __CLAY_COMMON_HPP
#define __CLAY_COMMON_HPP

typedef long long Int64;
typedef unsigned long long UInt64;

static inline
int
pointerToInt(void *p)
{
    UInt64 v = (UInt64)p;
    return (int)v;
}

#endif
