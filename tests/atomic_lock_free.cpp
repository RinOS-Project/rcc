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
