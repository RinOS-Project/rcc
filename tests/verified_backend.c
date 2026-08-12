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
