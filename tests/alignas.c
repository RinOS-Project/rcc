_Alignas(16) int aligned_global;
_Alignas(long long) int aligned_type_global;
_Alignas(16384) int aligned_global_16384;
_Alignas(16384) int aligned_initialized_global_16384 = 13;

int verified_local_alignment(void)
{
    _Alignas(16) int value = 7;
    _Alignas(16384) int extended_value = 9;
    unsigned long address = (unsigned long)&value;
    unsigned long extended_address = (unsigned long)&extended_value;
    return address % 16 == 0 && value == 7 &&
        extended_address % 16384 == 0 && extended_value == 9 ? 0 : 1;
}

int legacy_aligned_stack_parameter_check(
    int first, int second, int third, int fourth, int fifth,
    int sixth, int seventh, int eighth, int ninth)
{
    _Alignas(16384) int value = 37;
    unsigned long address = (unsigned long)&value;
    return address % 16384 == 0 && value == 37 &&
        first + second + third + fourth + fifth + sixth + seventh +
            eighth + ninth == 45 ? 0 : 1;
}

int main(void)
{
    _Alignas(16) int aligned_local = 7;
    _Alignas(16384) int aligned_local_16384 = 11;
    unsigned long address = (unsigned long)&aligned_local;
    unsigned long extended_address = (unsigned long)&aligned_local_16384;
    if (!(address % 16 == 0 && aligned_local == 7 &&
        extended_address % 16384 == 0 && aligned_local_16384 == 11 &&
        aligned_global == 0 && aligned_global_16384 == 0 &&
        aligned_initialized_global_16384 == 13)) return 1;
    if (verified_local_alignment() != 0) return 2;
    if (legacy_aligned_stack_parameter_check(
            1, 2, 3, 4, 5, 6, 7, 8, 9) != 0) return 3;
    return 0;
}
