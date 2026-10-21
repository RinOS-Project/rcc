int main(void)
{
    volatile float generic_object = 1.25f;
    float generic_source = 2.5f;
    float generic_result = 0.0f;
    float generic_exchange_source = 3.75f;
    float generic_previous = 0.0f;
    float generic_expected = 2.5f;
    float generic_desired = 4.5f;
    int generic_failed;
    int generic_succeeded;

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

#if defined(__x86_64__)
    {
        volatile double double_object = 6.25;
        double double_source = 7.5;
        double double_result = 0.0;
        double double_exchange_source = 8.75;
        double double_previous = 0.0;
        double double_expected = 7.5;
        double double_desired = 9.0;
        int double_succeeded;

        __atomic_load(&double_object, &double_result, __ATOMIC_ACQUIRE);
        __atomic_store(&double_object, &double_source, __ATOMIC_RELEASE);
        __atomic_exchange(&double_object, &double_exchange_source,
                          &double_previous, __ATOMIC_ACQ_REL);
        double_expected = 8.75;
        double_succeeded = __atomic_compare_exchange(
            &double_object, &double_expected, &double_desired, 0,
            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
        if (double_result != 6.25 || double_previous != 7.5 ||
            !double_succeeded || double_object != 9.0 ||
            double_expected != 8.75) {
            return 1;
        }
    }
#endif

    return generic_result == 1.25f && generic_previous == 2.5f &&
        generic_failed == 0 && generic_expected == 3.75f &&
        generic_succeeded != 0 && generic_object == 4.5f &&
        generic_source == 2.5f ? 0 : 1;
}
