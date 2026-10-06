extern "C" int cpp_atomic_always_lock_free_value() {
    return __atomic_always_lock_free(8, nullptr) &&
           __atomic_is_lock_free(4, nullptr);
}

extern "C" int cpp_atomic_is_lock_free_value(unsigned count, int* cursor) {
    return __atomic_is_lock_free(count, cursor++);
}

extern "C" int cpp_signed_parameter_value(signed value) {
    return value;
}

extern "C" void cpp_atomic_generic_load(const unsigned* value,
                                          unsigned* result) {
    __atomic_load(value, result, __ATOMIC_ACQUIRE);
}

extern "C" void cpp_atomic_generic_store(unsigned* value,
                                           unsigned* desired) {
    __atomic_store(value, desired, __ATOMIC_RELEASE);
}

extern "C" unsigned cpp_atomic_generic_exchange(unsigned* value,
                                                  unsigned* desired) {
    unsigned previous;
    __atomic_exchange(value, desired, &previous, __ATOMIC_ACQ_REL);
    return previous;
}

extern "C" bool cpp_atomic_generic_compare(unsigned* value,
                                            unsigned* expected,
                                            unsigned* desired) {
    return __atomic_compare_exchange(value, expected, desired, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}
