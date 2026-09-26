_Atomic(float) floating_atomic;

int invalid_atomic_rmw(void) {
    return ++floating_atomic;
}
