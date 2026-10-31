double rcc_frounding_math_rounding_probe(void)
{
    return 1.0 + 0x1p-54;
}

double rcc_frounding_math_runtime_probe(double value)
{
    return value + 0x1p-54;
}
