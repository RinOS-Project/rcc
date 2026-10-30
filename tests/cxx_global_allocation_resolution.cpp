using allocation_size_type = __SIZE_TYPE__;

static int allocation_slot;
static int allocation_calls;
static int deallocation_calls;

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

int main() {
    int* value = new int(73);
    bool initialized = allocation_calls == 1 && *value == 73;
    delete value;
    return initialized && deallocation_calls == 1 ? 0 : 1;
}
