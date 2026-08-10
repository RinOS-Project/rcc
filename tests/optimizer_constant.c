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
