void* invalid_pointer_fetch_add(void* volatile* value) {
    return __atomic_fetch_add(value, 1, __ATOMIC_SEQ_CST);
}

void* invalid_pointer_fetch_xor(void* volatile* value) {
    return __atomic_fetch_xor(value, 1, __ATOMIC_SEQ_CST);
}
