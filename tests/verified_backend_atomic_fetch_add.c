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
#if defined(__x86_64__)
    _Atomic unsigned long long builtin_wide = 0x10000000aULL;
    _Atomic unsigned long long exchange_wide = 0x123456789abcdef0ULL;
    unsigned long long builtin_wide_old;
    unsigned long long builtin_wide_new;
    unsigned long long exchange_wide_old;
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
        builtin_signed_old == -30 && builtin_signed_new == -31 &&
        builtin_signed == -31
            ? 0 : 1;
}
