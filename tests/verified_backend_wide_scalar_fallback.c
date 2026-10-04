extern unsigned long long verified_wide_scalar_external(void);

unsigned long long verified_wide_scalar_fallback(void)
{
    return verified_wide_scalar_external();
}
