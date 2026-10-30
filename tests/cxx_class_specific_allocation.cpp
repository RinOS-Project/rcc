using allocation_size_type = __SIZE_TYPE__;

alignas(8) static unsigned char class_storage[16];
alignas(8) static unsigned char class_array_storage[32];
alignas(8) static unsigned char inherited_storage[16];
alignas(8) static unsigned char inherited_array_storage[32];
alignas(8) static unsigned char fallback_storage[16];
static int class_allocation_calls;
static int class_deallocation_calls;
static int class_array_allocation_calls;
static int class_array_deallocation_calls;
static int inherited_allocation_calls;
static int inherited_deallocation_calls;
static int inherited_array_allocation_calls;
static int inherited_array_deallocation_calls;
static bool class_allocation_size_valid;
static bool class_array_allocation_size_valid;
static bool inherited_allocation_size_valid;
static bool inherited_array_allocation_size_valid;
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

struct InheritedAllocationBase {
    int base_value;

    void* operator new(allocation_size_type size);
    void operator delete(void* pointer) noexcept;
    void* operator new[](allocation_size_type size);
    void operator delete[](void* pointer) noexcept;
};

struct InheritedAllocationDerived : public InheritedAllocationBase {
    int value;
};

struct InheritedAllocationUsing : public InheritedAllocationBase {
    using InheritedAllocationBase::operator new;
    using InheritedAllocationBase::operator delete;
    int value;
};

struct InheritedArrayAllocationUsing : public InheritedAllocationBase {
    using InheritedAllocationBase::operator new[];
    using InheritedAllocationBase::operator delete[];
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

void* InheritedAllocationBase::operator new(allocation_size_type size) {
    inherited_allocation_size_valid =
        size == sizeof(InheritedAllocationDerived);
    ++inherited_allocation_calls;
    return &inherited_storage[0];
}

void InheritedAllocationBase::operator delete(void* pointer) noexcept {
    if (pointer == &inherited_storage[0]) ++inherited_deallocation_calls;
}

void* InheritedAllocationBase::operator new[](allocation_size_type size) {
    inherited_array_allocation_size_valid =
        size == sizeof(InheritedAllocationDerived) * 2;
    ++inherited_array_allocation_calls;
    return &inherited_array_storage[0];
}

void InheritedAllocationBase::operator delete[](void* pointer) noexcept {
    if (pointer == &inherited_array_storage[0]) {
        ++inherited_array_deallocation_calls;
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
    InheritedAllocationDerived* inherited =
        new InheritedAllocationDerived;
    inherited->base_value = 59;
    inherited->value = 61;
    bool selected_inherited_operators = inherited_allocation_calls == 1 &&
        inherited_allocation_size_valid && inherited->base_value == 59 &&
        inherited->value == 61;
    delete inherited;
    InheritedAllocationUsing* inherited_using =
        new InheritedAllocationUsing;
    inherited_using->base_value = 83;
    inherited_using->value = 89;
    bool selected_inherited_using_operator =
        inherited_allocation_calls == 2 && inherited_allocation_size_valid &&
        inherited_using->base_value == 83 && inherited_using->value == 89;
    delete inherited_using;
    InheritedAllocationDerived* inherited_values =
        new InheritedAllocationDerived[2];
    inherited_values[0].base_value = 67;
    inherited_values[0].value = 71;
    inherited_values[1].base_value = 73;
    inherited_values[1].value = 79;
    bool selected_inherited_array_operators =
        inherited_array_allocation_calls == 1 &&
        inherited_array_allocation_size_valid &&
        inherited_values[0].base_value == 67 &&
        inherited_values[0].value == 71 &&
        inherited_values[1].base_value == 73 &&
        inherited_values[1].value == 79;
    delete[] inherited_values;
    InheritedArrayAllocationUsing* inherited_using_values =
        new InheritedArrayAllocationUsing[2];
    inherited_using_values[0].base_value = 97;
    inherited_using_values[0].value = 101;
    inherited_using_values[1].base_value = 103;
    inherited_using_values[1].value = 107;
    bool selected_inherited_array_using_operator =
        inherited_array_allocation_calls == 2 &&
        inherited_array_allocation_size_valid &&
        inherited_using_values[0].base_value == 97 &&
        inherited_using_values[0].value == 101 &&
        inherited_using_values[1].base_value == 103 &&
        inherited_using_values[1].value == 107;
    delete[] inherited_using_values;
    int failure_mask = 0;
    if (!selected_class_operators) failure_mask |= 1;
    if (class_deallocation_calls != 1) failure_mask |= 2;
    if (!selected_class_array_operators) failure_mask |= 4;
    if (class_array_deallocation_calls != 1) failure_mask |= 8;
    if (!selected_implicit_fallback) failure_mask |= 16;
    if (fallback_deallocation_calls != 1) failure_mask |= 32;
    if (!selected_inherited_operators ||
        !selected_inherited_array_operators ||
        !selected_inherited_using_operator ||
        !selected_inherited_array_using_operator) failure_mask |= 64;
    if (inherited_deallocation_calls != 2 ||
        inherited_array_deallocation_calls != 2) failure_mask |= 128;
    return failure_mask;
}
