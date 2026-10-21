#ifndef __RCC__
static unsigned int oracle_atomic_fetch_min_unsigned(
    volatile unsigned int* object, unsigned int operand, int order) {
    unsigned int expected = __atomic_load_n(object, __ATOMIC_RELAXED);
    for (;;) {
        unsigned int desired = expected < operand ? expected : operand;
        if (__atomic_compare_exchange_n(object, &expected, desired, 0,
                                        order, __ATOMIC_RELAXED)) {
            return expected;
        }
    }
}

static unsigned int oracle_atomic_fetch_max_unsigned(
    volatile unsigned int* object, unsigned int operand, int order) {
    unsigned int expected = __atomic_load_n(object, __ATOMIC_RELAXED);
    for (;;) {
        unsigned int desired = expected > operand ? expected : operand;
        if (__atomic_compare_exchange_n(object, &expected, desired, 0,
                                        order, __ATOMIC_RELAXED)) {
            return expected;
        }
    }
}

static int oracle_atomic_fetch_min_signed(
    volatile int* object, int operand, int order) {
    int expected = __atomic_load_n(object, __ATOMIC_RELAXED);
    for (;;) {
        int desired = expected < operand ? expected : operand;
        if (__atomic_compare_exchange_n(object, &expected, desired, 0,
                                        order, __ATOMIC_RELAXED)) {
            return expected;
        }
    }
}

static int oracle_atomic_fetch_max_signed(
    volatile int* object, int operand, int order) {
    int expected = __atomic_load_n(object, __ATOMIC_RELAXED);
    for (;;) {
        int desired = expected > operand ? expected : operand;
        if (__atomic_compare_exchange_n(object, &expected, desired, 0,
                                        order, __ATOMIC_RELAXED)) {
            return expected;
        }
    }
}

static unsigned int oracle_atomic_fetch_uinc(
    volatile unsigned int* object, unsigned int limit, int order) {
    unsigned int expected = __atomic_load_n(object, __ATOMIC_RELAXED);
    for (;;) {
        unsigned int desired = expected < limit ? expected + 1u : 0u;
        if (__atomic_compare_exchange_n(object, &expected, desired, 0,
                                        order, __ATOMIC_RELAXED)) {
            return expected;
        }
    }
}

static unsigned int oracle_atomic_fetch_udec(
    volatile unsigned int* object, unsigned int limit, int order) {
    unsigned int expected = __atomic_load_n(object, __ATOMIC_RELAXED);
    for (;;) {
        unsigned int desired = expected == 0u || expected > limit
            ? limit : expected - 1u;
        if (__atomic_compare_exchange_n(object, &expected, desired, 0,
                                        order, __ATOMIC_RELAXED)) {
            return expected;
        }
    }
}

static unsigned int oracle_sync_swap(
    volatile unsigned int* object, unsigned int desired) {
    return __atomic_exchange_n(object, desired, __ATOMIC_SEQ_CST);
}
#endif

int main(void)
{
    _Atomic unsigned char value8 = 250u;
    _Atomic unsigned short value16 = 65530u;
    _Atomic signed char signed8 = -120;
    _Atomic signed short signed16 = -30000;
    _Atomic signed char builtin_signed8 = 120;
    _Atomic int builtin_signed = -30;
    _Atomic unsigned short builtin_u16 = 65530u;
    _Atomic unsigned int builtin_value = 40u;
    _Atomic unsigned char exchange_u8 = 7u;
    _Atomic unsigned short exchange_u16 = 4660u;
    _Atomic unsigned int exchange_value = 9u;
    _Atomic(int *) exchange_cursor;
    _Atomic unsigned int load_store_value = 51u;
    _Atomic(int *) load_store_cursor;
    _Atomic unsigned char test_set_value = 0u;
    _Atomic unsigned char clear_value = 1u;
    volatile unsigned char compare_exchange8 = 42u;
    volatile unsigned short compare_exchange16 = 0x1234u;
    volatile unsigned int compare_exchange32 = 0x10203040u;
    int * volatile compare_exchange_pointer;
    volatile unsigned int generic_compare_exchange32 = 0x11223344u;
    int * volatile generic_compare_exchange_pointer;
    volatile unsigned int bitwise_value = 0xf0f0u;
    volatile unsigned char bitwise_byte = 0xf3u;
    volatile unsigned short bitwise_word = 0xf0f3u;
    volatile unsigned int nand_value = 0xf0f0u;
    volatile unsigned int minmax_unsigned = 15u;
    volatile int minmax_signed = -10;
    volatile unsigned int wrap_value = 3u;
    volatile unsigned int sync_value = 0x0ff0u;
    volatile unsigned int sync_arithmetic_value = 30u;
    volatile unsigned int sync_lock_value = 0u;
    volatile unsigned int sync_swap_value = 0x1234u;
    volatile unsigned int sync_compare_value = 42u;
#if defined(__x86_64__)
    _Atomic unsigned long long builtin_wide = 0x10000000aULL;
    _Atomic unsigned long long exchange_wide = 0x123456789abcdef0ULL;
    volatile unsigned long long compare_exchange64 =
        0x123456789abcdef0ULL;
    volatile unsigned long long bitwise_wide =
        0xf0f0f0f0f0f0f0f0ULL;
    unsigned long long builtin_wide_old;
    unsigned long long builtin_wide_new;
    unsigned long long exchange_wide_old;
    unsigned long long compare_exchange64_expected =
        0x123456789abcdef0ULL;
    int compare_exchange64_succeeded;
    unsigned long long bitwise_wide_old;
#endif
    volatile int builtin_order = __ATOMIC_RELAXED;
    unsigned int builtin_fetch_sub;
    unsigned int builtin_sub_fetch;
    unsigned int builtin_fetch_add;
    unsigned int builtin_add_fetch;
    signed char builtin_narrow_old;
    unsigned short builtin_u16_old;
    int builtin_signed_old;
    int builtin_signed_new;
    unsigned int exchange_old;
    unsigned int builtin_loaded;
    unsigned char compare_exchange8_expected = 41u;
    unsigned short compare_exchange16_expected = 0x1234u;
    unsigned int compare_exchange32_expected = 7u;
    int *compare_exchange_pointer_expected;
    unsigned int generic_compare_exchange32_expected = 0xdeadbeefu;
    unsigned int generic_compare_exchange32_desired = 0xaabbccddu;
    int *generic_compare_exchange_pointer_expected;
    int *generic_compare_exchange_pointer_desired;
    int compare_exchange8_failed;
    int compare_exchange8_succeeded;
    int compare_exchange16_succeeded;
    int compare_exchange32_failed;
    int compare_exchange_pointer_succeeded;
    int generic_compare_exchange32_failed;
    int generic_compare_exchange32_succeeded;
    int generic_compare_exchange_pointer_failed;
    int generic_compare_exchange_pointer_succeeded;
    unsigned int generic_compare_exchange_object_evaluations = 0u;
    unsigned int generic_compare_exchange_expected_ptr_evaluations = 0u;
    unsigned int generic_compare_exchange_desired_ptr_evaluations = 0u;
    unsigned int generic_compare_exchange_weak_evaluations = 0u;
    unsigned int generic_compare_exchange_success_order_evaluations = 0u;
    unsigned int generic_compare_exchange_failure_order_evaluations = 0u;
    unsigned int compare_exchange_weak_evaluations = 0u;
    unsigned int compare_exchange_success_order_evaluations = 0u;
    unsigned int compare_exchange_failure_order_evaluations = 0u;
    unsigned int bitwise_order_evaluations = 0u;
    unsigned int wrap_order_evaluations = 0u;
    unsigned int bitwise_fetch_and;
    unsigned int bitwise_fetch_or;
    unsigned int bitwise_fetch_xor;
    unsigned int bitwise_and_fetch;
    unsigned int bitwise_or_fetch;
    unsigned int bitwise_xor_fetch;
    unsigned char bitwise_byte_old;
    unsigned short bitwise_word_old;
    unsigned int nand_fetch_old;
    unsigned int nand_fetch_new;
    unsigned int min_unsigned_old;
    unsigned int max_unsigned_old;
    int max_signed_old;
    int min_signed_old;
    unsigned int wrap_uinc_old;
    unsigned int wrap_uinc_limit_old;
    unsigned int wrap_udec_zero_old;
    unsigned int wrap_udec_limit_old;
    unsigned int wrap_udec_excess_old;
    unsigned int sync_fetch_and_old;
    unsigned int sync_and_fetch_new;
    unsigned int sync_fetch_old;
    unsigned int sync_or_fetch_new;
    unsigned int sync_fetch_xor_old;
    unsigned int sync_xor_fetch_new;
    unsigned int sync_fetch_nand_old;
    unsigned int sync_nand_new;
    unsigned int sync_fetch_add_old;
    unsigned int sync_add_fetch_new;
    unsigned int sync_fetch_sub_old;
    unsigned int sync_sub_fetch_new;
    unsigned int sync_operand_evaluations = 0u;
    unsigned int sync_lock_first_old;
    unsigned int sync_lock_second_old;
    unsigned int sync_swap_old;
    int sync_compare_success;
    int sync_compare_failure;
    unsigned int sync_compare_value_failure;
    unsigned int sync_compare_value_success;
    unsigned int sync_compare_expected_evaluations = 0u;
    unsigned int sync_compare_desired_evaluations = 0u;
    void *pointer_fetch_old;
    void *pointer_add_new;
    void *pointer_sub_old;
    void *pointer_sub_new;
    unsigned char exchange_u8_old;
    unsigned short exchange_u16_old;
    int *exchange_pointer_old;
    int *builtin_loaded_pointer;
    _Atomic(void *) fetch_cursor;
    int test_set_first;
    int test_set_second;
    int elements[8] = {0};
    _Atomic(int *) cursor = elements;
    int *old_pointer;
    int *compound_pointer;
    int *subtracted_pointer;
    int *prefix_pointer;
    int *decremented_pointer;
    compare_exchange_pointer = &elements[1];
    compare_exchange_pointer_expected = &elements[1];
    generic_compare_exchange_pointer = &elements[2];
    generic_compare_exchange_pointer_expected = &elements[1];
    generic_compare_exchange_pointer_desired = &elements[5];
    unsigned char old8 = value8++;
    unsigned short old16 = value16++;
    unsigned char added8 = (value8 += 10u);
    unsigned short added16 = (value16 += 10u);
    signed char old_signed8 = signed8++;
    signed short old_signed16 = signed16++;
    signed char added_signed8 = (signed8 += 7);
    signed short added_signed16 = (signed16 += 20);
    old_pointer = cursor++;
    if (old_pointer != &elements[0] || cursor != &elements[1]) return 3;
    compound_pointer = (cursor += 3);
    if (compound_pointer != &elements[4] || cursor != &elements[4]) return 4;
    subtracted_pointer = (cursor -= 2);
    if (subtracted_pointer != &elements[2] || cursor != &elements[2]) return 5;
    prefix_pointer = ++cursor;
    if (prefix_pointer != &elements[3] || cursor != &elements[3]) return 6;
    decremented_pointer = --cursor;
    exchange_cursor = elements;
    builtin_fetch_sub = __atomic_fetch_sub(
        &builtin_value, 7u, builtin_order++ % 6);
    builtin_sub_fetch = __atomic_sub_fetch(
        &builtin_value, 5u, builtin_order++ % 6);
    builtin_fetch_add = __atomic_fetch_add(
        &builtin_value, 4u, builtin_order++ % 6);
    builtin_add_fetch = __atomic_add_fetch(
        &builtin_value, 3u, builtin_order++ % 6);
    builtin_narrow_old = __atomic_fetch_sub(
        &builtin_signed8, 10, builtin_order++ % 6);
    builtin_signed_old = __atomic_fetch_sub(
        &builtin_signed, -4, builtin_order++ % 6);
    builtin_signed_new = __atomic_sub_fetch(
        &builtin_signed, 5, builtin_order++ % 6);
    builtin_u16_old = __atomic_fetch_add(
        &builtin_u16, 10u, builtin_order++ % 6);
    exchange_old = __atomic_exchange_n(
        &exchange_value, 29u, builtin_order++ % 6);
    exchange_u8_old = __atomic_exchange_n(
        &exchange_u8, 19u, builtin_order++ % 6);
    exchange_u16_old = __atomic_exchange_n(
        &exchange_u16, 0xabcdU, builtin_order++ % 6);
    exchange_pointer_old = __atomic_exchange_n(
        &exchange_cursor, &elements[5], builtin_order++ % 6);
    builtin_loaded = __atomic_load_n(
        &load_store_value, builtin_order++ % 3);
    __atomic_store_n(
        &load_store_value, 73u,
        builtin_order++ % 2 ? __ATOMIC_RELEASE : __ATOMIC_RELAXED);
    load_store_cursor = &elements[2];
    builtin_loaded_pointer = __atomic_load_n(
        &load_store_cursor, builtin_order++ % 3);
    __atomic_store_n(
        &load_store_cursor, &elements[7],
        builtin_order++ % 2 ? __ATOMIC_RELEASE : __ATOMIC_RELAXED);
    fetch_cursor = elements;
    pointer_fetch_old = __atomic_fetch_add(
        &fetch_cursor, 2, builtin_order++ % 6);
    pointer_add_new = __atomic_add_fetch(
        &fetch_cursor, 1, builtin_order++ % 6);
    pointer_sub_old = __atomic_fetch_sub(
        &fetch_cursor, 2, builtin_order++ % 6);
    pointer_sub_new = __atomic_sub_fetch(
        &fetch_cursor, 1, builtin_order++ % 6);
    __atomic_thread_fence(builtin_order++ % 6);
    __atomic_signal_fence(builtin_order++ % 6);
#if defined(__x86_64__)
    builtin_wide_old = __atomic_fetch_sub(
        &builtin_wide, 0x100000000ULL, builtin_order++ % 6);
    builtin_wide_new = __atomic_add_fetch(
        &builtin_wide, 7ULL, builtin_order++ % 6);
    exchange_wide_old = __atomic_exchange_n(
        &exchange_wide, 0xfedcba9876543210ULL, builtin_order++ % 6);
#endif
    test_set_first = __atomic_test_and_set(
        &test_set_value, builtin_order++ % 6);
    test_set_second = __atomic_test_and_set(
        &test_set_value, builtin_order++ % 6);
    __atomic_clear(
        &clear_value,
        builtin_order++ % 2 ? __ATOMIC_RELEASE : __ATOMIC_RELAXED);
    __sync_synchronize();

    compare_exchange8_failed = __atomic_compare_exchange_n(
        &compare_exchange8, &compare_exchange8_expected, 43u,
        (compare_exchange_weak_evaluations++, 1),
        (compare_exchange_success_order_evaluations++, __ATOMIC_SEQ_CST),
        (compare_exchange_failure_order_evaluations++, __ATOMIC_ACQUIRE));
    compare_exchange8_expected = 42u;
    compare_exchange8_succeeded = __atomic_compare_exchange_n(
        &compare_exchange8, &compare_exchange8_expected, 43u,
        (compare_exchange_weak_evaluations++, 1),
        (compare_exchange_success_order_evaluations++, __ATOMIC_SEQ_CST),
        (compare_exchange_failure_order_evaluations++, __ATOMIC_ACQUIRE));
    compare_exchange16_succeeded = __atomic_compare_exchange_n(
        &compare_exchange16, &compare_exchange16_expected, 0x5678u, 0,
        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    compare_exchange32_failed = __atomic_compare_exchange_n(
        &compare_exchange32, &compare_exchange32_expected, 0x50607080u, 0,
        __ATOMIC_SEQ_CST, __ATOMIC_RELAXED);
    compare_exchange_pointer_succeeded = __atomic_compare_exchange_n(
        &compare_exchange_pointer, &compare_exchange_pointer_expected,
        &elements[3], 1, __ATOMIC_SEQ_CST, __ATOMIC_ACQUIRE);
    generic_compare_exchange32_failed = __atomic_compare_exchange(
        (generic_compare_exchange_object_evaluations++,
         &generic_compare_exchange32),
        (generic_compare_exchange_expected_ptr_evaluations++,
         &generic_compare_exchange32_expected),
        (generic_compare_exchange_desired_ptr_evaluations++,
         &generic_compare_exchange32_desired),
        (generic_compare_exchange_weak_evaluations++, 1),
        (generic_compare_exchange_success_order_evaluations++,
         __ATOMIC_SEQ_CST),
        (generic_compare_exchange_failure_order_evaluations++,
         __ATOMIC_ACQUIRE));
    generic_compare_exchange32_succeeded = __atomic_compare_exchange(
        (generic_compare_exchange_object_evaluations++,
         &generic_compare_exchange32),
        (generic_compare_exchange_expected_ptr_evaluations++,
         &generic_compare_exchange32_expected),
        (generic_compare_exchange_desired_ptr_evaluations++,
         &generic_compare_exchange32_desired),
        (generic_compare_exchange_weak_evaluations++, 0),
        (generic_compare_exchange_success_order_evaluations++,
         __ATOMIC_ACQ_REL),
        (generic_compare_exchange_failure_order_evaluations++,
         __ATOMIC_ACQUIRE));
    generic_compare_exchange_pointer_failed = __atomic_compare_exchange(
        &generic_compare_exchange_pointer,
        &generic_compare_exchange_pointer_expected,
        &generic_compare_exchange_pointer_desired, 0,
        __ATOMIC_SEQ_CST, __ATOMIC_ACQUIRE);
    generic_compare_exchange_pointer_succeeded = __atomic_compare_exchange(
        &generic_compare_exchange_pointer,
        &generic_compare_exchange_pointer_expected,
        &generic_compare_exchange_pointer_desired, 0,
        __ATOMIC_SEQ_CST, __ATOMIC_ACQUIRE);
#if defined(__x86_64__)
    compare_exchange64_succeeded = __atomic_compare_exchange_n(
        &compare_exchange64, &compare_exchange64_expected,
        0xfedcba9876543210ULL, 0, __ATOMIC_SEQ_CST, __ATOMIC_ACQUIRE);
#endif
    bitwise_fetch_and = __atomic_fetch_and(
        &bitwise_value, 0x0ff0u,
        (bitwise_order_evaluations++, __ATOMIC_SEQ_CST));
    bitwise_fetch_or = __atomic_fetch_or(
        &bitwise_value, 0x8001u,
        (bitwise_order_evaluations++, __ATOMIC_SEQ_CST));
    bitwise_fetch_xor = __atomic_fetch_xor(
        &bitwise_value, 0x00ffu,
        (bitwise_order_evaluations++, __ATOMIC_SEQ_CST));
    bitwise_and_fetch = __atomic_and_fetch(
        &bitwise_value, 0x0f0fu,
        (bitwise_order_evaluations++, __ATOMIC_SEQ_CST));
    bitwise_or_fetch = __atomic_or_fetch(
        &bitwise_value, 0x0100u,
        (bitwise_order_evaluations++, __ATOMIC_SEQ_CST));
    bitwise_xor_fetch = __atomic_xor_fetch(
        &bitwise_value, 0xffffu,
        (bitwise_order_evaluations++, __ATOMIC_SEQ_CST));
    bitwise_byte_old = __atomic_fetch_xor(
        &bitwise_byte, 0xffu, __ATOMIC_SEQ_CST);
    bitwise_word_old = __atomic_fetch_and(
        &bitwise_word, 0x0ff0u, __ATOMIC_SEQ_CST);
    nand_fetch_old = __atomic_fetch_nand(
        &nand_value, 0x0ff0u,
        (bitwise_order_evaluations++, __ATOMIC_SEQ_CST));
    nand_fetch_new = __atomic_nand_fetch(
        &nand_value, 0x0f00u,
        (bitwise_order_evaluations++, __ATOMIC_SEQ_CST));
#if defined(__RCC__)
    min_unsigned_old = __atomic_fetch_min(
        &minmax_unsigned, 7u,
        (bitwise_order_evaluations++, __ATOMIC_SEQ_CST));
    max_unsigned_old = __atomic_fetch_max(
        &minmax_unsigned, 20u,
        (bitwise_order_evaluations++, __ATOMIC_SEQ_CST));
    max_signed_old = __atomic_fetch_max(
        &minmax_signed, 5,
        (bitwise_order_evaluations++, __ATOMIC_SEQ_CST));
    min_signed_old = __atomic_fetch_min(
        &minmax_signed, -20,
        (bitwise_order_evaluations++, __ATOMIC_SEQ_CST));
#else
    min_unsigned_old = oracle_atomic_fetch_min_unsigned(
        &minmax_unsigned, 7u,
        (bitwise_order_evaluations++, __ATOMIC_SEQ_CST));
    max_unsigned_old = oracle_atomic_fetch_max_unsigned(
        &minmax_unsigned, 20u,
        (bitwise_order_evaluations++, __ATOMIC_SEQ_CST));
    max_signed_old = oracle_atomic_fetch_max_signed(
        &minmax_signed, 5,
        (bitwise_order_evaluations++, __ATOMIC_SEQ_CST));
    min_signed_old = oracle_atomic_fetch_min_signed(
        &minmax_signed, -20,
        (bitwise_order_evaluations++, __ATOMIC_SEQ_CST));
#endif
#if defined(__x86_64__)
    bitwise_wide_old = __atomic_fetch_xor(
        &bitwise_wide, 0x0f0f0f0f0f0f0f0fULL, __ATOMIC_SEQ_CST);
#endif
#if defined(__RCC__)
    wrap_uinc_old = __atomic_fetch_uinc(
        &wrap_value, 4u, (wrap_order_evaluations++, __ATOMIC_SEQ_CST));
    wrap_uinc_limit_old = __atomic_fetch_uinc(
        &wrap_value, 4u, (wrap_order_evaluations++, __ATOMIC_SEQ_CST));
    wrap_udec_zero_old = __atomic_fetch_udec(
        &wrap_value, 4u, (wrap_order_evaluations++, __ATOMIC_SEQ_CST));
    wrap_udec_limit_old = __atomic_fetch_udec(
        &wrap_value, 4u, (wrap_order_evaluations++, __ATOMIC_SEQ_CST));
    wrap_value = 5u;
    wrap_udec_excess_old = __atomic_fetch_udec(
        &wrap_value, 4u, (wrap_order_evaluations++, __ATOMIC_SEQ_CST));
#else
    wrap_uinc_old = oracle_atomic_fetch_uinc(
        &wrap_value, 4u, (wrap_order_evaluations++, __ATOMIC_SEQ_CST));
    wrap_uinc_limit_old = oracle_atomic_fetch_uinc(
        &wrap_value, 4u, (wrap_order_evaluations++, __ATOMIC_SEQ_CST));
    wrap_udec_zero_old = oracle_atomic_fetch_udec(
        &wrap_value, 4u, (wrap_order_evaluations++, __ATOMIC_SEQ_CST));
    wrap_udec_limit_old = oracle_atomic_fetch_udec(
        &wrap_value, 4u, (wrap_order_evaluations++, __ATOMIC_SEQ_CST));
    wrap_value = 5u;
    wrap_udec_excess_old = oracle_atomic_fetch_udec(
        &wrap_value, 4u, (wrap_order_evaluations++, __ATOMIC_SEQ_CST));
#endif
    sync_fetch_and_old = __sync_fetch_and_and(&sync_value, 0x0fffu);
    sync_and_fetch_new = __sync_and_and_fetch(&sync_value, 0x00f0u);
    sync_fetch_old = __sync_fetch_and_or(&sync_value, 0x8001u);
    sync_or_fetch_new = __sync_or_and_fetch(&sync_value, 0x000eu);
    sync_fetch_xor_old = __sync_fetch_and_xor(&sync_value, 0x00ffu);
    sync_xor_fetch_new = __sync_xor_and_fetch(&sync_value, 0x0f00u);
    sync_fetch_nand_old = __sync_fetch_and_nand(&sync_value, 0x8f00u);
    sync_nand_new = __sync_nand_and_fetch(&sync_value, 0x00ffu);
    sync_fetch_add_old = __sync_fetch_and_add(
        &sync_arithmetic_value, (sync_operand_evaluations++, 4u));
    sync_add_fetch_new = __sync_add_and_fetch(
        &sync_arithmetic_value, (sync_operand_evaluations++, 3u));
    sync_fetch_sub_old = __sync_fetch_and_sub(
        &sync_arithmetic_value, (sync_operand_evaluations++, 5u));
    sync_sub_fetch_new = __sync_sub_and_fetch(
        &sync_arithmetic_value, (sync_operand_evaluations++, 7u));
    sync_lock_first_old = __sync_lock_test_and_set(&sync_lock_value, 7u);
    sync_lock_second_old = __sync_lock_test_and_set(&sync_lock_value, 9u);
    __sync_lock_release(&sync_lock_value);
#if defined(__RCC__)
    sync_swap_old = __sync_swap(&sync_swap_value, 0xa5a5u);
#else
    sync_swap_old = oracle_sync_swap(&sync_swap_value, 0xa5a5u);
#endif
    sync_compare_success = __sync_bool_compare_and_swap(
        &sync_compare_value,
        (sync_compare_expected_evaluations++, 42u),
        (sync_compare_desired_evaluations++, 77u));
    sync_compare_failure = __sync_bool_compare_and_swap(
        &sync_compare_value,
        (sync_compare_expected_evaluations++, 42u),
        (sync_compare_desired_evaluations++, 88u));
    sync_compare_value_failure = __sync_val_compare_and_swap(
        &sync_compare_value,
        (sync_compare_expected_evaluations++, 42u),
        (sync_compare_desired_evaluations++, 99u));
    sync_compare_value_success = __sync_val_compare_and_swap(
        &sync_compare_value,
        (sync_compare_expected_evaluations++, 77u),
        (sync_compare_desired_evaluations++, 99u));

    return old8 == 250u && value8 == 5u && added8 == 5u &&
        old16 == 65530u && value16 == 5u && added16 == 5u &&
        old_signed8 == -120 && signed8 == -112 && added_signed8 == -112 &&
        old_signed16 == -30000 && signed16 == -29979 &&
        added_signed16 == -29979 &&
        decremented_pointer == &elements[2] && cursor == &elements[2] &&
        builtin_fetch_sub == 40u && builtin_sub_fetch == 28u &&
        builtin_fetch_add == 28u && builtin_add_fetch == 35u &&
        builtin_value == 35u &&
#if defined(__x86_64__)
        builtin_order == 28 && builtin_wide_old == 0x10000000aULL &&
        builtin_wide_new == 17ULL && builtin_wide == 17ULL &&
        exchange_wide_old == 0x123456789abcdef0ULL &&
        exchange_wide == 0xfedcba9876543210ULL &&
#else
        builtin_order == 25 &&
#endif
        builtin_narrow_old == 120 && builtin_signed8 == 110 &&
        builtin_u16_old == 65530u && builtin_u16 == 4u &&
        exchange_old == 9u && exchange_value == 29u &&
        exchange_u8_old == 7u && exchange_u8 == 19u &&
        exchange_u16_old == 4660u && exchange_u16 == 0xabcdU &&
        exchange_pointer_old == &elements[0] &&
        exchange_cursor == &elements[5] &&
        builtin_loaded == 51u && load_store_value == 73u &&
        builtin_loaded_pointer == &elements[2] &&
        load_store_cursor == &elements[7] &&
        pointer_fetch_old == (void*)elements &&
        pointer_add_new == (void*)((unsigned char*)elements + 3) &&
        pointer_sub_old == (void*)((unsigned char*)elements + 3) &&
        pointer_sub_new == (void*)elements && fetch_cursor == (void*)elements &&
        test_set_first == 0 && test_set_second == 1 && test_set_value == 1u &&
        clear_value == 0u &&
        compare_exchange8_failed == 0 && compare_exchange8_expected == 42u &&
        compare_exchange8_succeeded == 1 && compare_exchange8 == 43u &&
        compare_exchange16_succeeded == 1 &&
        compare_exchange16 == 0x5678u &&
        compare_exchange32_failed == 0 &&
        compare_exchange32_expected == 0x10203040u &&
        compare_exchange_pointer_succeeded == 1 &&
        compare_exchange_pointer == &elements[3] &&
        compare_exchange_pointer_expected == &elements[1] &&
        generic_compare_exchange32_failed == 0 &&
        generic_compare_exchange32_expected == 0x11223344u &&
        generic_compare_exchange32_succeeded == 1 &&
        generic_compare_exchange32 == 0xaabbccddu &&
        generic_compare_exchange_pointer_failed == 0 &&
        generic_compare_exchange_pointer_expected == &elements[2] &&
        generic_compare_exchange_pointer_succeeded == 1 &&
        generic_compare_exchange_pointer == &elements[5] &&
        generic_compare_exchange_object_evaluations == 2u &&
        generic_compare_exchange_expected_ptr_evaluations == 2u &&
        generic_compare_exchange_desired_ptr_evaluations == 2u &&
        generic_compare_exchange_weak_evaluations == 2u &&
        generic_compare_exchange_success_order_evaluations == 2u &&
        generic_compare_exchange_failure_order_evaluations == 2u &&
        compare_exchange_weak_evaluations == 2u &&
        compare_exchange_success_order_evaluations == 2u &&
        compare_exchange_failure_order_evaluations == 2u &&
        bitwise_fetch_and == 0xf0f0u && bitwise_fetch_or == 0x00f0u &&
        bitwise_fetch_xor == 0x80f1u && bitwise_and_fetch == 0x000eu &&
        bitwise_or_fetch == 0x010eu && bitwise_xor_fetch == 0xfef1u &&
        bitwise_value == 0xfef1u && bitwise_byte_old == 0xf3u &&
        bitwise_byte == 0x0cu && bitwise_word_old == 0xf0f3u &&
        bitwise_word == 0x00f0u && nand_fetch_old == 0xf0f0u &&
        nand_fetch_new == 0xfffff0ffu && nand_value == 0xfffff0ffu &&
        min_unsigned_old == 15u && max_unsigned_old == 7u &&
        minmax_unsigned == 20u && max_signed_old == -10 &&
        min_signed_old == 5 && minmax_signed == -20 &&
        bitwise_order_evaluations == 12u &&
        wrap_uinc_old == 3u && wrap_uinc_limit_old == 4u &&
        wrap_udec_zero_old == 0u && wrap_udec_limit_old == 4u &&
        wrap_udec_excess_old == 5u && wrap_value == 4u &&
        wrap_order_evaluations == 5u &&
        sync_fetch_and_old == 0x0ff0u && sync_and_fetch_new == 0x00f0u &&
        sync_fetch_old == 0x00f0u && sync_or_fetch_new == 0x80ffu &&
        sync_fetch_xor_old == 0x80ffu && sync_xor_fetch_new == 0x8f00u &&
        sync_fetch_nand_old == 0x8f00u && sync_value == 0xffffff00u &&
        sync_nand_new == 0xffffff00u &&
        sync_fetch_add_old == 30u && sync_add_fetch_new == 37u &&
        sync_fetch_sub_old == 37u && sync_sub_fetch_new == 25u &&
        sync_arithmetic_value == 25u && sync_operand_evaluations == 4u &&
        sync_lock_first_old == 0u && sync_lock_second_old == 7u &&
        sync_lock_value == 0u && sync_swap_old == 0x1234u &&
        sync_swap_value == 0xa5a5u &&
        sync_compare_success == 1 && sync_compare_failure == 0 &&
        sync_compare_value_failure == 77u &&
        sync_compare_value_success == 77u && sync_compare_value == 99u &&
        sync_compare_expected_evaluations == 4u &&
        sync_compare_desired_evaluations == 4u &&
#if defined(__x86_64__)
        bitwise_wide_old == 0xf0f0f0f0f0f0f0f0ULL &&
        bitwise_wide == 0xffffffffffffffffULL &&
        compare_exchange64_succeeded == 1 &&
        compare_exchange64 == 0xfedcba9876543210ULL &&
        compare_exchange64_expected == 0x123456789abcdef0ULL &&
#endif
        builtin_signed_old == -30 && builtin_signed_new == -31 &&
        builtin_signed == -31
            ? 0 : 1;
}
