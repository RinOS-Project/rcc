extern "C" double verified_sysv_variadic_fp_sink(int marker, ...);

extern "C" double verified_sysv_variadic_fp_fallback(float value)
{
    return verified_sysv_variadic_fp_sink(7, value);
}
