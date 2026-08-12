static int verified_helper(int value);

struct VerifiedPair {
    int first;
    int second;
    int* pointer;
};

struct VerifiedContainer {
    struct VerifiedPair pair;
    int values[3];
    int matrix[2][2];
};

struct VerifiedHalves {
    unsigned short low;
    unsigned short high;
};

union VerifiedUnion {
    unsigned int bits;
    int signed_value;
    struct VerifiedHalves halves;
};

struct VerifiedArgument {
    int first;
    int second;
    int third;
};

struct VerifiedReturnPair {
    int first;
    int second;
};

struct VerifiedLargeReturn {
    int first;
    int second;
    int third;
    int fourth;
    int fifth;
    int sixth;
};

int verified_call(int value)
{
    return verified_helper(value) + 1;
}

static int verified_helper(int value)
{
    return value * 3;
}

int verified_load(int* value)
{
    return *value;
}

int verified_control(int condition, int left, int right)
{
    if (condition) {
        return left;
    }
    return right;
}

int verified_index(int* base, int index)
{
    return base[index];
}

int verified_local_array(int left, int middle, int right)
{
    int values[4] = {left, [2] = {middle}, right};
    return values[0] + *(values + 1) * 5 +
        values[2] * 2 + values[3] * 3;
}

int verified_local_pointer_array(int* left, int* right)
{
    int* values[3] = {left, 0, right};
    return *values[0] + *values[2];
}

int verified_local_string_array(int index)
{
    char text[8] = "RinOS";
    return text[index];
}

int verified_nested_array(void)
{
    int values[2][2] = {{1, 2}, [1] = {3, 4}};
    return values[1][0];
}

int verified_struct(int left, int right, int* value)
{
    struct VerifiedPair pair = {
        .second = right,
        .first = left,
        .pointer = value,
    };
    struct VerifiedPair copy = pair;
    struct VerifiedPair target = {0};
    target = copy = pair;
    struct VerifiedPair* view = &target;
    view->second += *view->pointer;
    return view->first * 100 + view->second +
        (copy.first == pair.first ? 0 : 10000);
}

int verified_struct_copy_pointer(struct VerifiedPair* source)
{
    struct VerifiedPair copy = *source;
    return copy.first * 100 + copy.second + *copy.pointer;
}

int verified_nested_struct(int left, int right, int* value)
{
    struct VerifiedContainer source = {
        .values = {[1] = right},
        .matrix = {{1, 2}, {3, 4}},
        .pair = {.first = left, .pointer = value},
    };
    struct VerifiedContainer copy = source;
    struct VerifiedContainer list[2] = {
        copy,
        {.pair = {.first = 7}, .values = {1, 2, 3},
         .matrix = {{5, 6}, {7, 8}}},
    };
    list[0].values[2] = *list[0].pair.pointer;
    return list[0].matrix[1][0] * 10000 +
        list[0].pair.first * 1000 + list[0].values[1] * 100 +
        list[0].values[2] * 10 + list[1].values[2];
}

int verified_union(unsigned int bits)
{
    union VerifiedUnion source = {.bits = bits};
    union VerifiedUnion copy = source;
    union VerifiedUnion values[2] = {
        {.signed_value = 0},
        {.bits = 0x00070005u},
    };
    values[0] = copy;
    return values[0].halves.low * 10 + values[0].halves.high +
        values[1].halves.low - 5;
}

int verified_compound_struct(int value)
{
    struct VerifiedPair* pair = &(struct VerifiedPair){
        .first = value,
        .second = 4,
        .pointer = &value,
    };
    return pair->first * 100 + pair->second + *pair->pointer;
}

int verified_compound_array(int first, int second)
{
    return ((int[3]){first, second, 9})[1];
}

int verified_compound_scalar(int value)
{
    return (int){value + 2};
}

int verified_struct_parameter(struct VerifiedArgument value, int bias)
{
    return value.first * 100 + value.second * 10 + value.third + bias;
}

int verified_struct_argument_call(int first, int second, int third)
{
    struct VerifiedArgument value = {first, second, third};
    return verified_struct_parameter(value, 4);
}

struct VerifiedReturnPair verified_pair_return(int first, int second)
{
    struct VerifiedReturnPair value = {first, second};
    return value;
}

int verified_pair_return_call(int first, int second)
{
    struct VerifiedReturnPair value = verified_pair_return(first, second);
    return value.first * 10 + value.second;
}

struct VerifiedArgument verified_triple_return(
    int first, int second, int third)
{
    struct VerifiedArgument value = {first, second, third};
    return value;
}

int verified_triple_return_call(int first, int second, int third)
{
    struct VerifiedArgument value =
        verified_triple_return(first, second, third);
    return value.first * 100 + value.second * 10 + value.third;
}

struct VerifiedLargeReturn verified_large_return(
    int first, int second, int third)
{
    struct VerifiedLargeReturn value = {
        first, second, third, first + 1, second + 1, third + 1
    };
    return value;
}

int verified_large_return_call(int first, int second, int third)
{
    struct VerifiedLargeReturn value =
        verified_large_return(first, second, third);
    return value.first * 100 + value.second * 10 + value.third +
        value.fourth + value.fifth + value.sixth;
}

int verified_pointer_add(int* base, int index)
{
    return *(base + index);
}

int verified_pointer_sub(int* base, int index)
{
    return *(base - index);
}

int verified_conditional(int condition, int* value)
{
    return condition ? (*value = *value + 1) : (*value = *value + 3);
}

int verified_logical_and(int condition, int* value)
{
    return condition && (*value = *value + 1);
}

int verified_logical_or(int condition, int* value)
{
    return condition || (*value = *value + 1);
}

int verified_pointer_compound(int** cursor, int step)
{
    *cursor += step;
    return **cursor;
}

int verified_pointer_postincrement(int** cursor)
{
    int* old = (*cursor)++;
    return *old + **cursor;
}

int verified_lvalue_once(int* base, int index)
{
    base[index++] += 5;
    return index * 100 + base[index - 1];
}

long verified_pointer_difference(int* left, int* right)
{
    return left - right;
}

int verified_switch(int value)
{
    int result = 1;
    switch (value) {
        case -1:
            result = 10;
            break;
        case 2:
            result += 20;
        case 3:
            result += 3;
            break;
        default:
            result = 99;
    }
    return result;
}

int verified_nested_switch(int outer, int inner)
{
    int result = 0;
    switch (outer) {
        case 1:
            switch (inner) {
                case 4:
                    result = 14;
                    break;
                default:
                    result = 19;
            }
            result += 100;
            break;
        default:
            result = -1;
    }
    return result;
}

int verified_switch_promotion(unsigned char value)
{
    switch (value) {
        case 255:
            return 1;
        default:
            return 0;
    }
    return 0;
}

int verified_switch_skips_prefix(int value, int* side_effect)
{
    switch (value) {
        *side_effect = *side_effect + 1;
        case 1:
            return *side_effect;
        default:
            return 9;
    }
    return -1;
}
