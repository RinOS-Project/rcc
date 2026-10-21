int main(void)
{
    union { unsigned int bits; float value; } f125;
    union { unsigned int bits; float value; } f25;
    union { unsigned int bits; float value; } f375;
    union { unsigned int bits; float value; } f45;
    union { unsigned int bits; float value; } positive_zero;
    union { unsigned int bits; float value; } negative_zero;
    union { unsigned int bits; float value; } float_one;
    union { unsigned int bits; float value; } representation_expected;
    union { unsigned int bits; float value; } representation_after;
    volatile float generic_object;
    volatile float representation_object;
    float generic_source;
    float generic_result;
    float generic_exchange_source;
    float generic_previous;
    float generic_expected;
    float generic_desired;
    int representation_succeeded;
    int generic_failed;
    int generic_succeeded;

    f125.bits = 0x3fa00000u;
    f25.bits = 0x40200000u;
    f375.bits = 0x40700000u;
    f45.bits = 0x40900000u;
    positive_zero.bits = 0u;
    negative_zero.bits = 0x80000000u;
    float_one.bits = 0x3f800000u;
    generic_object = f125.value;
    generic_source = f25.value;
    generic_exchange_source = f375.value;
    generic_desired = f45.value;
    generic_expected = f25.value;
    __atomic_load(&generic_object, &generic_result, __ATOMIC_ACQUIRE);
    __atomic_store(&generic_object, &generic_source, __ATOMIC_RELEASE);
    __atomic_exchange(&generic_object, &generic_exchange_source,
                      &generic_previous, __ATOMIC_ACQ_REL);
    generic_failed = __atomic_compare_exchange(
        &generic_object, &generic_expected, &generic_desired, 0,
        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    generic_expected = 3.75f;
    generic_succeeded = __atomic_compare_exchange(
        &generic_object, &generic_expected, &generic_desired, 0,
        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    representation_object = negative_zero.value;
    representation_expected.bits = positive_zero.bits;
    representation_succeeded = __atomic_compare_exchange(
        &representation_object, &representation_expected.value,
        &float_one.value, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    representation_after.value = representation_object;

#if defined(__x86_64__)
    {
        union { unsigned long long bits; double value; } d625;
        union { unsigned long long bits; double value; } d75;
        union { unsigned long long bits; double value; } d875;
        union { unsigned long long bits; double value; } d9;
        volatile double double_object;
        double double_source;
        double double_result;
        double double_exchange_source;
        double double_previous;
        double double_expected;
        double double_desired;
        int double_succeeded;

        d625.bits = 0x4019000000000000ULL;
        d75.bits = 0x401e000000000000ULL;
        d875.bits = 0x4021800000000000ULL;
        d9.bits = 0x4022000000000000ULL;
        double_object = d625.value;
        double_source = d75.value;
        double_exchange_source = d875.value;
        double_desired = d9.value;
        double_expected = d75.value;
        __atomic_load(&double_object, &double_result, __ATOMIC_ACQUIRE);
        __atomic_store(&double_object, &double_source, __ATOMIC_RELEASE);
        __atomic_exchange(&double_object, &double_exchange_source,
                          &double_previous, __ATOMIC_ACQ_REL);
        double_expected = d875.value;
        double_succeeded = __atomic_compare_exchange(
            &double_object, &double_expected, &double_desired, 0,
            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
        if (double_result != d625.value || double_previous != d75.value ||
            !double_succeeded || double_object != d9.value ||
            double_expected != d875.value) {
            return 1;
        }
    }
#endif

    return generic_result == f125.value && generic_previous == f25.value &&
        generic_failed == 0 && generic_expected == f375.value &&
        generic_succeeded != 0 && generic_object == f45.value &&
        generic_source == f25.value && !representation_succeeded &&
        representation_expected.bits == 0x80000000u &&
        representation_after.bits == 0x80000000u ? 0 : 1;
}
