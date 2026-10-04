int loop_invariant_while_zero(void)
{
    int enabled = 0;
    while (enabled) {
        return 99;
    }
    return 7;
}

int loop_invariant_for_zero(void)
{
    int enabled = 0;
    for (int index = 0; enabled; ++index) {
        return index;
    }
    return 11;
}

int loop_mutates_condition(int input)
{
    int remaining = input;
    while (remaining > 0) {
        --remaining;
    }
    return remaining;
}

int loop_constant_one(void)
{
    int result = 0;
    for (int index = 0; index < 1; ++index) {
        result += 17;
    }
    return result;
}

int loop_constant_one_le(void)
{
    int result = 0;
    for (int index = 3; index <= 3; ++index) {
        result += 19;
    }
    return result;
}
