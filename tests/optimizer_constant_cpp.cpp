extern "C" {
#include "optimizer_constant.c"
}

static int inline_nullptr_parameter_is_equal(int* value, int* other)
{
    return value == other;
}

extern "C" int inline_nullptr_parameter_call(int* other)
{
    return inline_nullptr_parameter_is_equal(nullptr, other);
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

extern "C" int inline_function_pointer_parameter_call(
    int (*callback)(int), int value)
{
    return callback(value);
}

static constexpr int (*inline_constexpr_function_array[])(int) = {
    &inline_constexpr_array_first,
    &inline_constexpr_array_second
};

static int (*const inline_const_function_pointer)(int) =
    &inline_constexpr_array_first;

static int (*const inline_const_function_pointer_array[])(int) = {
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

extern "C" int inline_const_function_pointer_call(int value)
{
    return inline_const_function_pointer(value);
}

extern "C" int inline_const_function_pointer_array_call(int value)
{
    return inline_const_function_pointer_array[1](value);
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

extern "C" int inline_local_const_function_pointer_array_call(int value)
{
    int (*const callbacks[])(int) = {
        &inline_constexpr_array_first,
        &inline_constexpr_array_second
    };
    return callbacks[1](value);
}

extern "C" int inline_local_mutable_function_pointer_call(int value)
{
    int (*callback)(int) = &inline_constexpr_array_first;
    return callback(value);
}

extern "C" int inline_local_function_pointer_copy_call(int value)
{
    int (*callback)(int) = &inline_constexpr_array_first;
    int (*copy)(int) = callback;
    return copy(value);
}

extern "C" int inline_local_function_pointer_chained_copy_call(int value)
{
    int (*callback)(int) = &inline_constexpr_array_first;
    int (*copy)(int) = callback;
    int (*second_copy)(int) = copy;
    return second_copy(value);
}

extern "C" int
inline_local_function_pointer_reaching_copy_chain_call(int value)
{
    int (*callback)(int) = &inline_constexpr_array_first;
    callback = &inline_constexpr_array_second;
    int (*copy)(int) = callback;
    int (*second_copy)(int) = copy;
    return second_copy(value);
}

extern "C" int
inline_local_function_pointer_divergent_reaching_copy_call(int value)
{
    int (*callback)(int) = &inline_constexpr_array_first;
    if (value < 0) {
        callback = &inline_constexpr_array_second;
    }
    int (*copy)(int) = callback;
    return copy(value);
}

extern "C" int
inline_local_function_pointer_reaching_assignment_copy_chain_call(int value)
{
    int (*callback)(int) = &inline_constexpr_array_first;
    callback = &inline_constexpr_array_second;
    int (*copy)(int) = &inline_constexpr_array_first;
    copy = callback;
    int (*second_copy)(int) = &inline_constexpr_array_first;
    second_copy = copy;
    return second_copy(value);
}

extern "C" int
inline_local_function_pointer_divergent_assignment_copy_call(int value)
{
    int (*callback)(int) = &inline_constexpr_array_first;
    if (value < 0) {
        callback = &inline_constexpr_array_second;
    }
    int (*copy)(int) = &inline_constexpr_array_first;
    copy = callback;
    return copy(value);
}

extern "C" int
inline_local_function_pointer_same_target_conditional_copy_call(
    int* evaluations, int value)
{
    int (*copy)(int) = (*evaluations)++
        ? &inline_constexpr_array_first : &inline_constexpr_array_first;
    return copy(value);
}

extern "C" int
inline_local_function_pointer_divergent_conditional_copy_call(
    int* evaluations, int value)
{
    int (*copy)(int) = (*evaluations)++
        ? &inline_constexpr_array_first : &inline_constexpr_array_second;
    return copy(value);
}

extern "C" int
inline_local_function_pointer_copy_after_alias_write_call(int value)
{
    int (*callback)(int) = &inline_constexpr_array_first;
    int (**alias)(int) = &callback;
    *alias = &inline_constexpr_array_second;
    int (*copy)(int) = callback;
    return copy(value);
}

extern "C" int inline_local_reassigned_function_pointer_call(int value)
{
    int (*callback)(int) = &inline_constexpr_array_first;
    int before_reassignment = callback(value);
    callback = &inline_constexpr_array_second;
    return before_reassignment + callback(value);
}

extern "C" int inline_local_branch_reassigned_function_pointer_call(
    int value)
{
    int (*callback)(int) = &inline_constexpr_array_first;
    if (value < 0) {
        callback = &inline_constexpr_array_second;
    }
    return callback(value);
}

extern "C" int inline_local_branch_same_target_function_pointer_call(
    int value)
{
    int (*callback)(int) = &inline_constexpr_array_first;
    if (value < 0) {
        callback = &inline_constexpr_array_second;
    } else {
        callback = &inline_constexpr_array_second;
    }
    return callback(value);
}

extern "C" int inline_local_same_target_reassigned_function_pointer_call(
    int value)
{
    int (*callback)(int) = &inline_constexpr_array_first;
    callback = &inline_constexpr_array_first;
    if (value >= 0) {
        callback = &inline_constexpr_array_first;
    }
    return callback(value);
}

extern "C" int inline_local_mutable_function_pointer_array_call(int value)
{
    int (*callbacks[])(int) = {
        &inline_constexpr_array_first,
        &inline_constexpr_array_second
    };
    return callbacks[1](value);
}

extern "C" int inline_local_mutated_function_pointer_array_call(int value)
{
    int (*callbacks[])(int) = {
        &inline_constexpr_array_first,
        &inline_constexpr_array_second
    };
    callbacks[1] = &inline_constexpr_array_first;
    return callbacks[1](value);
}

extern "C" int inline_local_mutable_function_pointer_array_dynamic_call(
    int index, int value)
{
    int (*callbacks[])(int) = {
        &inline_constexpr_array_first,
        &inline_constexpr_array_second
    };
    return callbacks[index](value);
}

extern "C" int inline_local_uniform_function_pointer_array_dynamic_call(
    int index, int value)
{
    int (*callbacks[])(int) = {
        &inline_constexpr_array_first,
        &inline_constexpr_array_first
    };
    return callbacks[index](value);
}

extern "C" int inline_local_uniform_function_pointer_array_side_effect_index(
    int* index, int value)
{
    int (*callbacks[])(int) = {
        &inline_constexpr_array_first,
        &inline_constexpr_array_first
    };
    return callbacks[(*index)++](value);
}

extern "C" int inline_local_aliased_mutable_function_pointer_call(int value)
{
    int (*callback)(int) = &inline_constexpr_array_first;
    int (**alias)(int) = &callback;
    *alias = &inline_constexpr_array_second;
    return callback(value);
}

extern "C" int
inline_local_aliased_mutable_function_pointer_array_call(int value)
{
    int (*callbacks[])(int) = {
        &inline_constexpr_array_first,
        &inline_constexpr_array_second
    };
    int (**alias)(int) = callbacks;
    *alias = &inline_constexpr_array_second;
    return callbacks[0](value);
}
