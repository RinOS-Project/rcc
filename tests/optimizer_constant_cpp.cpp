extern "C" {
#include "optimizer_constant.c"
}

static int inline_noexcept_value(int* value)
{
    return (int)noexcept(value);
}

extern "C" int inlined_noexcept_call(int* value)
{
    return inline_noexcept_value(value);
}
