extern "C" double verified_sysv_variadic_fp_sink(int marker, ...);

extern "C" double verified_sysv_variadic_fp_fallback(double value)
{
    return verified_sysv_variadic_fp_sink(7, value);
}
