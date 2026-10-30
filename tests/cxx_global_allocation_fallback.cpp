using allocation_size_type = __SIZE_TYPE__;

alignas(8) static unsigned char allocation_storage[16];
static int allocation_calls;
static int deallocation_calls;

extern "C" void* rin_malloc(allocation_size_type size) {
    (void)size;
    ++allocation_calls;
    return allocation_storage;
}

extern "C" void rin_free(void* pointer) {
    (void)pointer;
    ++deallocation_calls;
}

int main() {
    int* value = new int(42);
    bool initialized = allocation_calls == 1 && *value == 42;
    delete value;
    int* values = new int[2];
    values[0] = 11;
    values[1] = 29;
    bool array_initialized = allocation_calls == 2 &&
        values[0] == 11 && values[1] == 29;
    delete[] values;
    return initialized && array_initialized && deallocation_calls == 2
        ? 0 : 1;
}
