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
