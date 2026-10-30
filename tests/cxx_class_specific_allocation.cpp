using allocation_size_type = __SIZE_TYPE__;

alignas(8) static unsigned char class_storage[16];
alignas(8) static unsigned char class_array_storage[32];
static void* const class_array_placement_tag = &class_array_storage[24];
alignas(8) static unsigned char inherited_storage[16];
alignas(8) static unsigned char inherited_array_storage[32];
alignas(8) static unsigned char fallback_storage[16];
static int class_allocation_calls;
static int class_deallocation_calls;
static int class_array_allocation_calls;
static int class_array_deallocation_calls;
static int class_array_placement_allocation_calls;
static int class_array_placement_failure_deallocation_calls;
static int class_array_destructor_calls;
static int inherited_allocation_calls;
static int inherited_deallocation_calls;
static int inherited_array_allocation_calls;
static int inherited_array_deallocation_calls;
static allocation_size_type inherited_allocation_last_size;
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

    ClassSpecificArrayAllocation() noexcept : value(0) {}
    ~ClassSpecificArrayAllocation() noexcept {
        ++class_array_destructor_calls;
    }
    void* operator new[](allocation_size_type size);
    void* operator new[](allocation_size_type size, void* placement);
    void operator delete[](void* pointer) noexcept;
    void operator delete[](void* pointer, void* placement) noexcept;
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

struct CompetingInheritedAllocationBase {
    int competitor_value;

    void* operator new(allocation_size_type size);
    void operator delete(void* pointer) noexcept;
    void* operator new[](allocation_size_type size);
    void operator delete[](void* pointer) noexcept;
};

struct MultipleBaseInheritedAllocation : public InheritedAllocationBase,
                                         public CompetingInheritedAllocationBase {
    using InheritedAllocationBase::operator new;
    using InheritedAllocationBase::operator delete;
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
        sizeof(ClassSpecificArrayAllocation) * 2 +
        sizeof(allocation_size_type);
    ++class_array_allocation_calls;
    return &class_array_storage[0];
}

void* ClassSpecificArrayAllocation::operator new[](
    allocation_size_type size, void* placement) {
    class_array_allocation_size_valid = size ==
        sizeof(ClassSpecificArrayAllocation) * 2 +
        sizeof(allocation_size_type);
    if (placement != class_array_placement_tag) {
        class_array_allocation_size_valid = false;
    }
    ++class_array_placement_allocation_calls;
    return &class_array_storage[0];
}

void ClassSpecificArrayAllocation::operator delete[](void* pointer) noexcept {
    if (pointer == &class_array_storage[0]) {
        ++class_array_deallocation_calls;
    }
}

void ClassSpecificArrayAllocation::operator delete[](
    void* pointer, void* placement) noexcept {
    if (pointer == &class_array_storage[0] &&
        placement == class_array_placement_tag) {
        ++class_array_placement_failure_deallocation_calls;
    }
}

void* InheritedAllocationBase::operator new(allocation_size_type size) {
    inherited_allocation_last_size = size;
    inherited_allocation_size_valid =
        size == sizeof(InheritedAllocationDerived) ||
        size == sizeof(MultipleBaseInheritedAllocation);
    ++inherited_allocation_calls;
    return &inherited_storage[0];
}

void InheritedAllocationBase::operator delete(void* pointer) noexcept {
    if (pointer == &inherited_storage[0]) ++inherited_deallocation_calls;
}

void* InheritedAllocationBase::operator new[](allocation_size_type size) {
    inherited_array_allocation_size_valid =
        size == sizeof(InheritedAllocationDerived) * 2 ||
        size == sizeof(MultipleBaseInheritedAllocation) * 2;
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
    bool destroyed_class_array = class_array_destructor_calls == 2;
    ClassSpecificArrayAllocation* placed_values =
        new (class_array_placement_tag) ClassSpecificArrayAllocation[2];
    bool selected_placement_class_array_operator =
        class_array_placement_allocation_calls == 1 &&
        class_array_allocation_size_valid && placed_values[0].value == 0 &&
        placed_values[1].value == 0;
    delete[] placed_values;
    destroyed_class_array = destroyed_class_array &&
        class_array_destructor_calls == 4;
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
    MultipleBaseInheritedAllocation* multiple_base_using =
        new MultipleBaseInheritedAllocation;
    multiple_base_using->base_value = 109;
    multiple_base_using->competitor_value = 113;
    multiple_base_using->value = 127;
    bool selected_multiple_base_using_operator =
        inherited_allocation_calls == 3 && inherited_allocation_size_valid &&
        inherited_allocation_last_size ==
            sizeof(MultipleBaseInheritedAllocation) &&
        multiple_base_using->base_value == 109 &&
        multiple_base_using->competitor_value == 113 &&
        multiple_base_using->value == 127;
    delete multiple_base_using;
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
    MultipleBaseInheritedAllocation* multiple_base_using_values =
        new MultipleBaseInheritedAllocation[2];
    multiple_base_using_values[0].base_value = 131;
    multiple_base_using_values[0].competitor_value = 137;
    multiple_base_using_values[0].value = 139;
    multiple_base_using_values[1].base_value = 149;
    multiple_base_using_values[1].competitor_value = 151;
    multiple_base_using_values[1].value = 157;
    bool selected_multiple_base_array_using_operator =
        inherited_array_allocation_calls == 3 &&
        inherited_array_allocation_size_valid &&
        multiple_base_using_values[0].base_value == 131 &&
        multiple_base_using_values[0].competitor_value == 137 &&
        multiple_base_using_values[0].value == 139 &&
        multiple_base_using_values[1].base_value == 149 &&
        multiple_base_using_values[1].competitor_value == 151 &&
        multiple_base_using_values[1].value == 157;
    delete[] multiple_base_using_values;
    int failure_mask = 0;
    if (!selected_class_operators) failure_mask |= 1;
    if (class_deallocation_calls != 1) failure_mask |= 2;
    if (!selected_class_array_operators) failure_mask |= 4;
    if (class_array_deallocation_calls != 2) failure_mask |= 8;
    if (!destroyed_class_array) failure_mask |= 256;
    if (!selected_placement_class_array_operator ||
        class_array_placement_failure_deallocation_calls != 0) {
        failure_mask |= 512;
    }
    if (!selected_implicit_fallback) failure_mask |= 16;
    if (fallback_deallocation_calls != 1) failure_mask |= 32;
    if (!selected_inherited_operators ||
        !selected_inherited_array_operators ||
        !selected_inherited_using_operator ||
        !selected_inherited_array_using_operator ||
        !selected_multiple_base_using_operator ||
        !selected_multiple_base_array_using_operator) failure_mask |= 64;
    if (inherited_deallocation_calls != 3 ||
        inherited_array_deallocation_calls != 3) failure_mask |= 128;
    return failure_mask;
}
