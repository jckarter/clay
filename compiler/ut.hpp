#pragma once

#include <stdio.h>


namespace clay {

typedef void (*TestFunc)();

void register_test(const char* name, TestFunc);

#define CLAY_UNITTEST(NAME) \
    void NAME ## _testImpl (); \
    struct TestRegistrator_ ## NAME { \
        TestRegistrator_ ## NAME () { \
            ::clay::register_test(#NAME, &NAME ## _testImpl); \
        } \
    }; \
    static TestRegistrator_ ## NAME testRegistrator_ ## NAME; \
    void NAME ## _testImpl()


struct AssertionError {};


#define UT_FAIL() do { \
        printf("failure in %s at %s:%d\n", __func__, __FILE__, __LINE__); \
        throw AssertionError(); \
    } while (0)


#define UT_ASSERT(COND) do { \
        if (!(COND)) { \
            printf("condition %s failed in %s at %s:%d\n", #COND, __func__, __FILE__, __LINE__); \
            throw AssertionError(); \
        } \
    } while (0)


}
