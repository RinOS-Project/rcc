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

static int inline_scope_collision(int value)
{
    return value + 100;
}

namespace {
int inline_scope_collision(int value);

int inline_scope_collision_call(int value)
{
    return inline_scope_collision(value);
}

int inline_scope_collision(int value)
{
    return value + 200;
}
}

extern "C" int inline_scope_collision_entry(int value)
{
    return inline_scope_collision_call(value);
}
