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
