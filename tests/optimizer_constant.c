int folded_arithmetic(void)
{
    return (2 + 3) * 4 - (8 / 2) + (7 % 4);
}

int folded_choice(int fallback)
{
    return ((1 < 2) && (4 != 5)) ? 42 : fallback;
}

int folded_short_circuit(int* value)
{
    return (0 && (*value = 7)) + (1 || (*value = 9));
}

unsigned long long folded_unsigned_wrap(void)
{
    return ((0xffffffffffffffffULL + 2ULL) * 3ULL) ^ 0ULL;
}

unsigned long long folded_unsigned_divmod(void)
{
    return (0xfffffffffffffff0ULL / 16ULL) +
           (0xffffffffffffffffULL % 16ULL);
}

unsigned long long folded_unsigned_shift(void)
{
    return (0x8000000000000000ULL >> 63) | (1ULL << 63);
}

unsigned int folded_unsigned_32(void)
{
    return ((0xffffffffU + 2U) * 3U) >> 1;
}

unsigned int folded_unsigned_narrow(void)
{
    return (unsigned char)(250U + 10U) +
           (unsigned short)(65535U + 2U);
}

unsigned long long folded_unsigned_unary(void)
{
    return (-1ULL) ^ (~0ULL);
}

int folded_mixed_unsigned_comparison(void)
{
    return -1LL < 1ULL;
}

int removed_after_return(int* value)
{
    return 7;
    *value = 91;
}

int removed_after_goto(int* value)
{
    goto done;
    *value = 92;
done:
    return *value;
}

int preserved_nested_label(int value)
{
    goto nested;
    if (value) {
nested:
        return 23;
    }
    return 0;
}

int removed_after_break(int* value)
{
    while (*value < 5) {
        *value += 1;
        break;
        *value = 93;
    }
    return *value;
}

int removed_after_continue(int* value)
{
    while (*value < 3) {
        *value += 1;
        continue;
        *value = 94;
    }
    return *value;
}

int preserved_case_after_break(int choice)
{
    switch (choice) {
        while (1) {
            break;
            return 99;
        case 3:
            return 33;
        }
    }
    return 0;
}

int folded_branch(int* value)
{
    if ((2 + 2) == 4) {
        return 5;
    } else {
        *value = 9;
        return *value;
    }
}

int removed_loop(int* value)
{
    while (3 < 2) {
        *value = 8;
    }
    return *value;
}

int removed_for_loop(int* value)
{
    for (*value += 2; 3 < 2; *value = 99) {
        *value = 88;
    }
    return *value;
}

int preserved_case_loop(int choice)
{
    switch (choice) {
        while (0) {
            case 1:
                return 17;
        }
    }
    return 0;
}

int preserved_case_for(int choice)
{
    switch (choice) {
        for (; 0; ) {
            case 2:
                return 29;
        }
    }
    return 0;
}
