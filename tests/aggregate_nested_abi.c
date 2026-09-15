#include <stdarg.h>

struct NestedPair {
    float first;
    float second;
};

struct NestedMixed {
    struct NestedPair pair;
    int tag;
};

struct FloatArray3 {
    float values[3];
};

union NestedUnion {
    struct NestedPair pair;
    long long integer;
};

struct NestedMixed nested_mixed(struct NestedMixed value)
{
    value.tag += (int)value.pair.first;
    return value;
}

struct FloatArray3 nested_float_array(struct FloatArray3 value)
{
    value.values[2] += value.values[0];
    return value;
}

union NestedUnion nested_union(union NestedUnion value)
{
    value.integer += 3;
    return value;
}

double nested_vararg(int ignored, ...)
{
    va_list arguments;
    struct NestedMixed mixed;
    struct FloatArray3 array;
    (void)ignored;
    va_start(arguments, ignored);
    mixed = va_arg(arguments, struct NestedMixed);
    array = va_arg(arguments, struct FloatArray3);
    va_end(arguments);
    return (double)mixed.pair.first + (double)mixed.pair.second +
           (double)mixed.tag + (double)array.values[2];
}
