#include <assert.h>

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

extern struct FloatPair fixed_float_pair(struct FloatPair value);
extern double fixed_double_pair(struct DoublePair value);
extern double fixed_float_double(struct FloatDouble value);
extern int fixed_mixed_float_int(struct MixedFloatInt value);
extern struct FloatPair return_float_pair(float first, float second);
extern double vararg_float_pair(int ignored, ...);
extern double vararg_float_double(int ignored, ...);
extern int vararg_mixed_float_int(int ignored, ...);

int main(void)
{
    struct FloatPair pair = { 1.25f, 2.5f };
    struct DoublePair doubles = { 1.25, 2.5 };
    struct FloatDouble mixed = { 1.25f, 2.5 };
    struct MixedFloatInt mixed_integer = { 4.0f, 9 };
    struct FloatPair returned;

    returned = fixed_float_pair(pair);
    assert(returned.first > 1.249f && returned.first < 1.251f);
    assert(returned.second > 2.499f && returned.second < 2.501f);
    assert(fixed_double_pair(doubles) > 3.749 &&
           fixed_double_pair(doubles) < 3.751);
    assert(fixed_float_double(mixed) > 3.749 &&
           fixed_float_double(mixed) < 3.751);
    assert(fixed_mixed_float_int(mixed_integer) == 13);
    returned = return_float_pair(3.5f, 4.25f);
    assert(returned.first > 3.499f && returned.first < 3.501f);
    assert(returned.second > 4.249f && returned.second < 4.251f);
    assert(vararg_float_pair(0, pair) > 3.749 &&
           vararg_float_pair(0, pair) < 3.751);
    assert(vararg_float_double(0, mixed) > 3.749 &&
           vararg_float_double(0, mixed) < 3.751);
    assert(vararg_mixed_float_int(0, mixed_integer) == 13);
    return 0;
}
