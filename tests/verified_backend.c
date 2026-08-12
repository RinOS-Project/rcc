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
