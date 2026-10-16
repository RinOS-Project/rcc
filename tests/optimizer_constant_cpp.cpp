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

static int inline_constexpr_array_first(int value)
{
    return value + 23;
}

static int inline_constexpr_array_second(int value)
{
    return value * 3 - 4;
}

static constexpr int (*inline_constexpr_function_array[])(int) = {
    &inline_constexpr_array_first,
    &inline_constexpr_array_second
};

extern "C" int inline_constexpr_array_first_call(int value)
{
    return inline_constexpr_function_array[0](value);
}

extern "C" int inline_constexpr_array_second_call(int value)
{
    return inline_constexpr_function_array[1](value);
}

extern "C" int inline_constexpr_array_dynamic_call(int index, int value)
{
    return inline_constexpr_function_array[index](value);
}

extern "C" int inline_local_constexpr_function_pointer_call(int value)
{
    constexpr int (*callback)(int) = &inline_constexpr_array_first;
    return callback(value);
}

extern "C" int inline_local_const_function_pointer_call(int value)
{
    int (*const callback)(int) = &inline_constexpr_array_second;
    return callback(value);
}
