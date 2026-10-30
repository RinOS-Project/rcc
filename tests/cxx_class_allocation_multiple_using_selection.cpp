using allocation_size_type = __SIZE_TYPE__;

alignas(8) static unsigned char int_storage[16];
alignas(8) static unsigned char char_storage[16];
alignas(8) static unsigned char int_array_storage[32];
alignas(8) static unsigned char char_array_storage[32];
alignas(8) static unsigned char global_placement_storage[16];
alignas(8) static unsigned char global_placement_array_storage[16];
static int int_allocation_calls;
static int char_allocation_calls;
static int int_array_allocation_calls;
static int char_array_allocation_calls;
static bool int_allocation_size_valid;
static bool char_allocation_size_valid;
static bool int_array_allocation_size_valid;
static bool char_array_allocation_size_valid;
static bool global_array_allocation_size_valid;

void* operator new(allocation_size_type size, void* storage) noexcept {
    (void)size;
    return storage;
}

void* operator new[](allocation_size_type size, void* storage) noexcept {
    global_array_allocation_size_valid = size == sizeof(int) * 2;
    return storage;
}

struct IntPlacementAllocationBase {
    void* operator new(allocation_size_type size, int tag);
    void* operator new[](allocation_size_type size, int tag);
};

struct CharPlacementAllocationBase {
    void* operator new(allocation_size_type size, char tag);
    void* operator new[](allocation_size_type size, char tag);
};

struct MultipleUsingAllocation : IntPlacementAllocationBase,
                                 CharPlacementAllocationBase {
    using IntPlacementAllocationBase::operator new;
    using CharPlacementAllocationBase::operator new;
    using IntPlacementAllocationBase::operator new[];
    using CharPlacementAllocationBase::operator new[];
    int value;
};

void* IntPlacementAllocationBase::operator new(
    allocation_size_type size, int tag) {
    int_allocation_size_valid =
        size == sizeof(MultipleUsingAllocation) && tag == 7;
    ++int_allocation_calls;
    return &int_storage[0];
}

void* IntPlacementAllocationBase::operator new[](
    allocation_size_type size, int tag) {
    int_array_allocation_size_valid =
        size == sizeof(MultipleUsingAllocation) * 2 && tag == 7;
    ++int_array_allocation_calls;
    return &int_array_storage[0];
}

void* CharPlacementAllocationBase::operator new(
    allocation_size_type size, char tag) {
    char_allocation_size_valid =
        size == sizeof(MultipleUsingAllocation) && tag == 'x';
    ++char_allocation_calls;
    return &char_storage[0];
}

void* CharPlacementAllocationBase::operator new[](
    allocation_size_type size, char tag) {
    char_array_allocation_size_valid =
        size == sizeof(MultipleUsingAllocation) * 2 && tag == 'x';
    ++char_array_allocation_calls;
    return &char_array_storage[0];
}

int main() {
    MultipleUsingAllocation* int_value = new (7) MultipleUsingAllocation;
    MultipleUsingAllocation* char_value = new ('x') MultipleUsingAllocation;
    MultipleUsingAllocation* int_values =
        new (7) MultipleUsingAllocation[2];
    MultipleUsingAllocation* char_values =
        new ('x') MultipleUsingAllocation[2];
    int* global_value =
        new (&global_placement_storage[0]) int(173);
    int* global_values =
        new (&global_placement_array_storage[0]) int[2];
    int_value->value = 163;
    char_value->value = 167;
    int_values[0].value = 179;
    int_values[1].value = 181;
    char_values[0].value = 191;
    char_values[1].value = 193;
    global_values[0] = 197;
    global_values[1] = 199;
    if (int_allocation_calls != 1 || !int_allocation_size_valid ||
        int_value->value != 163) return 1;
    if (char_allocation_calls != 1 || !char_allocation_size_valid ||
        char_value->value != 167) return 2;
    if (int_array_allocation_calls != 1 ||
        !int_array_allocation_size_valid ||
        int_values[0].value != 179 || int_values[1].value != 181) return 8;
    if (char_array_allocation_calls != 1 ||
        !char_array_allocation_size_valid ||
        char_values[0].value != 191 || char_values[1].value != 193) return 16;
    if (global_value != reinterpret_cast<int*>(&global_placement_storage[0]) ||
        *global_value != 173) return 4;
    if (global_values !=
            reinterpret_cast<int*>(&global_placement_array_storage[0]) ||
        !global_array_allocation_size_valid ||
        global_values[0] != 197 || global_values[1] != 199) return 32;
    return 0;
}
