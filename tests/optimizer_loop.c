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

int loop_constant_two(void)
{
    int result = 0;
    for (int index = 0; index < 2; ++index) {
        result += 13;
    }
    return result;
}

int loop_compound_increment(void)
{
    int result = 0;
    for (int index = 0; index < 2; index += 1) {
        result += 23;
    }
    return result;
}

int loop_assignment_increment(void)
{
    int result = 0;
    for (int index = 0; index < 2; index = index + 1) {
        result += 29;
    }
    return result;
}

int loop_volatile_increment(void)
{
    volatile int index = 0;
    int result = 0;
    for (; index < 2; index += 1) {
        result += 31;
    }
    return result;
}

int loop_descending_two(void)
{
    int result = 0;
    for (int index = 3; index > 1; --index) {
        result += 37;
    }
    return result;
}

int loop_descending_assignment_two(void)
{
    int result = 0;
    for (int index = 3; index > 1; index = index - 1) {
        result += 47;
    }
    return result;
}

int loop_descending_two_unsigned(void)
{
    int result = 0;
    for (unsigned index = 2u; index >= 1u; index -= 1u) {
        result += 41;
    }
    return result;
}

int loop_not_equal_two(void)
{
    int result = 0;
    for (unsigned index = 0u; index != 2u; ++index) {
        result += 53;
    }
    return result;
}

int loop_not_equal_descending_two(void)
{
    int result = 0;
    for (int index = 2; index != 0; index -= 1) {
        result += 59;
    }
    return result;
}

int loop_constant_three_le(void)
{
    int result = 0;
    for (unsigned index = 1; index <= 3; ++index) {
        result += 7;
    }
    return result;
}

int loop_constant_zero(void)
{
    int result = 5;
    for (int index = 3; index < 3; ++index) {
        result += 23;
    }
    return result;
}

int loop_constant_zero_le(void)
{
    int result = 7;
    for (int index = 3; index <= 2; ++index) {
        result += 29;
    }
    return result;
}

int loop_constant_zero_unsigned(void)
{
    int result = 11;
    for (unsigned index = 3u; index < 2u; ++index) {
        result += 31;
    }
    return result;
}

int do_constant_zero(void)
{
    int result = 0;
    do {
        result += 37;
    } while (0);
    return result;
}

int do_constant_zero_continue(void)
{
    int result = 41;
    do {
        continue;
    } while (0);
    return result;
}
