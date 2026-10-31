#pragma STDC FENV_ACCESS ON

double rcc_fenv_access_rounding_probe(void)
{
    return 1.0 + 0x1p-54;
}

double rcc_fenv_access_runtime_probe(double value)
{
    return value + 0x1p-54;
}
