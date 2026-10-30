using allocation_size_type = __SIZE_TYPE__;

static int allocation_slot;
static int allocation_calls;
static int deallocation_calls;
static int array_allocation_calls;
static int array_deallocation_calls;

void* operator new(allocation_size_type size) {
    (void)size;
    ++allocation_calls;
    return &allocation_slot;
}

void operator delete(void* pointer) noexcept {
    if (pointer == &allocation_slot) ++deallocation_calls;
}

void operator delete(void* pointer, allocation_size_type size) noexcept {
    (void)pointer;
    (void)size;
}

void* operator new[](allocation_size_type size) {
    (void)size;
    ++array_allocation_calls;
    return &allocation_slot;
}

void operator delete[](void* pointer) noexcept {
    if (pointer == &allocation_slot) ++array_deallocation_calls;
}

void operator delete[](void* pointer,
                       allocation_size_type size) noexcept {
    (void)pointer;
    (void)size;
}

int main() {
    int* value = new int(73);
    bool initialized = allocation_calls == 1 && *value == 73;
    delete value;
    int* values = new int[2];
    values[0] = 19;
    values[1] = 23;
    bool array_initialized = array_allocation_calls == 1 &&
        values[0] == 19 && values[1] == 23;
    delete[] values;
    return initialized && array_initialized && deallocation_calls == 1 &&
            array_deallocation_calls == 1
        ? 0 : 1;
}
