using allocation_size_type = __SIZE_TYPE__;

alignas(8) static unsigned char class_storage[16];
alignas(8) static unsigned char fallback_storage[16];
static int class_allocation_calls;
static int class_deallocation_calls;
static int fallback_allocation_calls;
static int fallback_deallocation_calls;

extern "C" void* rin_malloc(allocation_size_type size) {
    (void)size;
    ++fallback_allocation_calls;
    return fallback_storage;
}

extern "C" void rin_free(void* pointer) {
    (void)pointer;
    ++fallback_deallocation_calls;
}

struct ClassSpecificAllocation {
    int value;

    void* operator new(allocation_size_type size);
    void operator delete(void* pointer) noexcept;
};

void* ClassSpecificAllocation::operator new(allocation_size_type size) {
    (void)size;
    ++class_allocation_calls;
    return &class_storage[0];
}

void ClassSpecificAllocation::operator delete(void* pointer) noexcept {
    if (pointer == &class_storage[0]) ++class_deallocation_calls;
}

int main() {
    ClassSpecificAllocation* value = new ClassSpecificAllocation;
    value->value = 53;
    bool selected_class_operators = class_allocation_calls == 1 &&
        value->value == 53;
    delete value;
    return selected_class_operators && class_deallocation_calls == 1 &&
            fallback_allocation_calls == 0 &&
            fallback_deallocation_calls == 0
        ? 0 : 1;
}
