#include "../external_test.hpp"
#include <stdint.h>

struct ReturnByReg { int x; };
struct SingleFloat { double x; };
struct MultiReg { int64_t x; double y; };

extern "C" {

int an_int(void)
{
    return 101;
}

ReturnByReg return_by_reg(void)
{
    ReturnByReg r = { 202 };
    return r;
}

SingleFloat single_float(void)
{
    SingleFloat r = { 303.5 };
    return r;
}

MultiReg multi_reg(void)
{
    MultiReg r = { 404, 505.5 };
    return r;
}

} // extern "C"

