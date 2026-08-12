static int verified_helper(int value);

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
