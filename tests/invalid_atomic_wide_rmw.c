_Atomic(long long) wide_atomic;

int invalid_atomic_wide_rmw(void) {
    return ++wide_atomic;
}
