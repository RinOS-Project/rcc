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

extern struct NestedMixed nested_mixed(struct NestedMixed value);
extern struct FloatArray3 nested_float_array(struct FloatArray3 value);
extern union NestedUnion nested_union(union NestedUnion value);
extern double nested_vararg(int ignored, ...);

int main(void)
{
    struct NestedMixed mixed = {{1.5f, 2.5f}, 7};
    struct FloatArray3 array = {{1.0f, 2.0f, 3.0f}};
    union NestedUnion value;
    struct NestedMixed returned_mixed;
    struct FloatArray3 returned_array;
    union NestedUnion returned_union;

    value.integer = 40;
    returned_mixed = nested_mixed(mixed);
    returned_array = nested_float_array(array);
    returned_union = nested_union(value);
    if (returned_mixed.pair.first != 1.5f ||
        returned_mixed.pair.second != 2.5f || returned_mixed.tag != 8 ||
        returned_array.values[0] != 1.0f ||
        returned_array.values[1] != 2.0f ||
        returned_array.values[2] != 4.0f || returned_union.integer != 43) {
        return 1;
    }
    return nested_vararg(0, mixed, array) == 14 ? 0 : 1;
}
