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
