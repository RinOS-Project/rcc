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
#if defined(__x86_64__)
    _Atomic unsigned long long builtin_wide = 0x10000000aULL;
    unsigned long long builtin_wide_old;
    unsigned long long builtin_wide_new;
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
#if defined(__x86_64__)
    builtin_wide_old = __atomic_fetch_sub(
        &builtin_wide, 0x100000000ULL, builtin_order++ % 6);
    builtin_wide_new = __atomic_add_fetch(
        &builtin_wide, 7ULL, builtin_order++ % 6);
#endif

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
        builtin_order == 10 && builtin_wide_old == 0x10000000aULL &&
        builtin_wide_new == 17ULL && builtin_wide == 17ULL &&
#else
        builtin_order == 8 &&
#endif
        builtin_narrow_old == 120 && builtin_signed8 == 110 &&
        builtin_u16_old == 65530u && builtin_u16 == 4u &&
        builtin_signed_old == -30 && builtin_signed_new == -31 &&
        builtin_signed == -31
            ? 0 : 1;
}
