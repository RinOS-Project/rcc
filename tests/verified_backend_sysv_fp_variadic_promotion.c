extern double verified_sysv_variadic_fp_sink(int marker, ...);

double verified_sysv_variadic_fp_promotion(float value)
{
    return verified_sysv_variadic_fp_sink(7, value);
}
