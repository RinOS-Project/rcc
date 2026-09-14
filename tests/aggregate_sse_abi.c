#include <stdarg.h>

struct FloatPair {
    float first;
    float second;
};

struct DoublePair {
    double first;
    double second;
};

struct FloatDouble {
    float first;
    double second;
};

struct MixedFloatInt {
    float first;
    int second;
};

struct FloatPair fixed_float_pair(struct FloatPair value)
{
    return value;
}

double fixed_double_pair(struct DoublePair value)
{
    return value.first + value.second;
}

double fixed_float_double(struct FloatDouble value)
{
    return (double)value.first + value.second;
}

int fixed_mixed_float_int(struct MixedFloatInt value)
{
    return (int)value.first + value.second;
}

struct FloatPair return_float_pair(float first, float second)
{
    struct FloatPair value = { first, second };
    return value;
}

double vararg_float_pair(int ignored, ...)
{
    va_list arguments;
    struct FloatPair value;
    (void)ignored;
    va_start(arguments, ignored);
    value = va_arg(arguments, struct FloatPair);
    va_end(arguments);
    return (double)value.first + (double)value.second;
}

double vararg_float_double(int ignored, ...)
{
    va_list arguments;
    struct FloatDouble value;
    (void)ignored;
    va_start(arguments, ignored);
    value = va_arg(arguments, struct FloatDouble);
    va_end(arguments);
    return (double)value.first + value.second;
}

int vararg_mixed_float_int(int ignored, ...)
{
    va_list arguments;
    struct MixedFloatInt value;
    (void)ignored;
    va_start(arguments, ignored);
    value = va_arg(arguments, struct MixedFloatInt);
    va_end(arguments);
    return (int)value.first + value.second;
}
