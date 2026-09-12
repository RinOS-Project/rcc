struct invalid_integer_operator_value {
    int member;
};

int invalid_remainder(void)
{
    return 5.0 % 2;
}

int invalid_bitnot(void)
{
    return ~1.0;
}

int invalid_shift(void)
{
    return 1.0 << 2;
}

int invalid_logical_not(struct invalid_integer_operator_value value)
{
    return !value;
}
