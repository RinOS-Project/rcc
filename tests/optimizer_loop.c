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

int loop_constant_while_one(void)
{
    int index = 3;
    int result = 0;
    while (index <= 3) {
        result += 43;
        ++index;
    }
    return result;
}

int loop_constant_while_two(void)
{
    int index = 0;
    int result = 0;
    while (index < 2) {
        result += 47;
        ++index;
    }
    return result;
}

int loop_assignment_while_two(void)
{
    int index;
    int result = 0;
    index = 0;
    while (index < 2) {
        result += 53;
        ++index;
    }
    return result;
}

int loop_stride_one(void)
{
    int index = 2;
    int result = 0;
    while (index <= 2) {
        result += 79;
        index += 2;
    }
    return result;
}

int loop_stride_two(void)
{
    int index = 0;
    int result = 0;
    while (index < 4) {
        result += 83;
        index += 2;
    }
    return result;
}

int loop_stride_assignment_two(void)
{
    int index = 0;
    int result = 0;
    while (index != 4) {
        result += 89;
        index = index + 2;
    }
    return result;
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

int loop_stride_for_two(void)
{
    int result = 0;
    for (int index = 0; index < 4; index += 2) {
        result += 31;
    }
    return result;
}

int loop_stride_for_one(void)
{
    int result = 0;
    for (int index = 2; index < 4; index += 2) {
        result += 43;
    }
    return result;
}

int loop_stride_for_assignment_two(void)
{
    int index;
    int result = 0;
    for (index = 0; index != 4; index = index + 2) {
        result += 37;
    }
    return result;
}

int loop_stride_for_descending_two(void)
{
    int result = 0;
    for (int index = 6; index > 2; index -= 2) {
        result += 41;
    }
    return result;
}

int loop_stride_for_unsigned_two(void)
{
    int result = 0;
    for (unsigned index = 4u; index != 0u; index -= 2u) {
        result += 43;
    }
    return result;
}

int loop_assignment_initializer_one(void)
{
    int index;
    int result = 0;
    for (index = 3; index <= 3; ++index) {
        result += 31;
    }
    return result;
}

int loop_assignment_initializer_two(void)
{
    int index;
    int result = 0;
    for (index = 0; index < 2; ++index) {
        result += 37;
    }
    return result;
}

int loop_constant_post_value(void)
{
    int index = 0;
    for (; index < 2; ++index) {
    }
    return index;
}

int loop_single_post_value_with_decl(void)
{
    int index = 0;
    int result = 0;
    for (; index < 1; ++index) {
        int value = 7;
        result += value;
    }
    return result + index;
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

int loop_assignment_initializer_zero(void)
{
    int index;
    int result = 13;
    for (index = 3; index < 3; ++index) {
        result += 41;
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

int do_constant_one(void)
{
    int index = 0;
    int result = 0;
    do {
        result += 61;
        ++index;
    } while (index < 1);
    return result;
}

int do_constant_two(void)
{
    int index = 0;
    int result = 0;
    do {
        result += 67;
        ++index;
    } while (index < 2);
    return result;
}

int do_assignment_not_equal_two(void)
{
    int index;
    int result = 0;
    index = 0;
    do {
        result += 71;
        index += 1;
    } while (index != 2);
    return result;
}

int do_descending_two(void)
{
    int index = 3;
    int result = 0;
    do {
        result += 73;
        --index;
    } while (index > 1);
    return result;
}

int do_stride_two(void)
{
    int index = 0;
    int result = 0;
    do {
        result += 97;
        index += 2;
    } while (index < 4);
    return result;
}

int do_descending_stride_two(void)
{
    int index = 6;
    int result = 0;
    do {
        result += 101;
        index -= 2;
    } while (index > 2);
    return result;
}
