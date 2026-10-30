using allocation_size_type = __SIZE_TYPE__;

alignas(8) static unsigned char class_storage[16];
alignas(8) static unsigned char class_array_storage[32];
alignas(8) static unsigned char fallback_storage[16];
static int class_allocation_calls;
static int class_deallocation_calls;
static int class_array_allocation_calls;
static int class_array_deallocation_calls;
static bool class_allocation_size_valid;
static bool class_array_allocation_size_valid;
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

struct ClassSpecificArrayAllocation {
    int value;

    void* operator new[](allocation_size_type size);
    void operator delete[](void* pointer) noexcept;
};

struct ImplicitFallbackArrayAllocation {
    int value;
};

void* ClassSpecificAllocation::operator new(allocation_size_type size) {
    class_allocation_size_valid =
        size == sizeof(ClassSpecificAllocation);
    ++class_allocation_calls;
    return &class_storage[0];
}

void ClassSpecificAllocation::operator delete(void* pointer) noexcept {
    if (pointer == &class_storage[0]) ++class_deallocation_calls;
}

void* ClassSpecificArrayAllocation::operator new[](allocation_size_type size) {
    class_array_allocation_size_valid = size ==
        sizeof(ClassSpecificArrayAllocation) * 2;
    ++class_array_allocation_calls;
    return &class_array_storage[0];
}

void ClassSpecificArrayAllocation::operator delete[](void* pointer) noexcept {
    if (pointer == &class_array_storage[0]) {
        ++class_array_deallocation_calls;
    }
}

int main() {
    ClassSpecificAllocation* value = new ClassSpecificAllocation;
    value->value = 53;
    bool selected_class_operators = class_allocation_calls == 1 &&
        class_allocation_size_valid &&
        value->value == 53;
    delete value;
    ClassSpecificArrayAllocation* values =
        new ClassSpecificArrayAllocation[2];
    values[0].value = 37;
    values[1].value = 41;
    bool selected_class_array_operators =
        class_array_allocation_calls == 1 &&
        class_array_allocation_size_valid && values[0].value == 37 &&
        values[1].value == 41;
    delete[] values;
    ImplicitFallbackArrayAllocation* fallback_values =
        new ImplicitFallbackArrayAllocation[2];
    fallback_values[0].value = 31;
    fallback_values[1].value = 47;
    bool selected_implicit_fallback = fallback_allocation_calls == 1 &&
        fallback_values[0].value == 31 && fallback_values[1].value == 47;
    delete[] fallback_values;
    return selected_class_operators && class_deallocation_calls == 1 &&
            selected_class_array_operators &&
            class_array_deallocation_calls == 1 &&
            selected_implicit_fallback && fallback_deallocation_calls == 1
        ? 0 : 1;
}
