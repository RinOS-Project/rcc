float verified_rintf(float value) {
    return __builtin_rintf(value);
}

double verified_rint(double value) {
    return __builtin_rint(value);
}

double verified_rintl_as_double(double value) {
    return (double)__builtin_rintl((long double)value);
}

void verified_rint_discard(double value) {
    (void)__builtin_rint(value);
}
