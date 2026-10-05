extern "C" int cpp_atomic_always_lock_free_value() {
    return __atomic_always_lock_free(8, nullptr) &&
           __atomic_is_lock_free(4, nullptr);
}

extern "C" int cpp_atomic_is_lock_free_value(unsigned int count, int* cursor) {
    return __atomic_is_lock_free(count, cursor++);
}
