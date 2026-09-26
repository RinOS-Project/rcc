#include <rcc/stdatomic.h>

int atomic_language_surface(void) {
    _Atomic(int) direct_counter;
    _Atomic long long direct_wide;
    _Atomic(void*) direct_pointer;

    direct_counter = 41;
    direct_wide = 0x0000000200000003LL;
    direct_pointer = &direct_counter;
    if (direct_counter != 41) return 1;
    if (direct_wide != 0x0000000200000003LL) return 2;
    if (direct_pointer != &direct_counter) return 3;
    return 0;
}

int atomic_language_deref(_Atomic(int)* value) {
    *value = 7;
    return *value;
}

int atomic_language_rmw(void) {
    _Atomic(int) value;
    value = 1;
    if (++value != 2) return 1;
    if (value++ != 2 || value != 3) return 2;
    value += 4;
    if (value != 7) return 3;
    value -= 2;
    return value == 5 ? 0 : 4;
}
