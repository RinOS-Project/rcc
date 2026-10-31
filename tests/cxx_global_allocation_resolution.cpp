using allocation_size_type = __SIZE_TYPE__;

static int allocation_slot;
static int allocation_calls;
static int deallocation_calls;
static int array_allocation_calls;
static int array_deallocation_calls;
static allocation_size_type array_allocation_last_size;
alignas(8) static unsigned char array_allocation_slot[64];
static int scalar_constructor_allocation_calls;
static int scalar_constructor_deallocation_calls;
alignas(8) static unsigned char scalar_constructor_allocation_slot[64];
static int scalar_placement_state;
static int scalar_placement_argument_evaluations;
static int scalar_placement_new_calls;
static int scalar_placement_delete_calls;
static int scalar_placement_regular_delete_calls;
static int scalar_placement_delete_mismatch;
alignas(8) static unsigned char scalar_placement_allocation_slot[64];
static int global_scalar_placement_argument_evaluations;
static int global_scalar_placement_new_calls;
static int global_scalar_placement_delete_calls;
static int global_scalar_placement_regular_delete_calls;
static int global_scalar_placement_delete_mismatch;
alignas(8) static unsigned char global_scalar_placement_allocation_slot[64];
static int cookie_array_constructor_calls;
static int cookie_array_destructor_calls;
static int placement_array_constructor_calls;
static int placement_array_default_constructor_calls;
static int placement_array_destructor_calls;
static int placement_array_destructor_order;
static int placement_array_new_calls;
static int placement_array_delete_calls;
static int placement_array_regular_delete_calls;
static int placement_array_delete_argument_matches;
static unsigned int placement_array_delete_argument_mismatch;
static int placement_array_marker;
static allocation_size_type placement_array_last_size;
static int placement_array_state;
alignas(8) static unsigned char placement_array_slot[64];
static int aggregate_placement_new_calls;
static int aggregate_placement_delete_calls;
static int aggregate_placement_delete_matches;
static int aggregate_placement_delete_mismatch;
static int aggregate_placement_new_mismatch;
static int aggregate_placement_constructor_calls;
static int aggregate_placement_destructor_calls;
static int aggregate_placement_destructor_order[2];
static int aggregate_placement_regular_delete_calls;
static allocation_size_type aggregate_placement_last_size;
alignas(8) static unsigned char aggregate_placement_slot[64];

struct PlacementRegisterArguments {
    alignas(16) double ratio;
    int marker;
    int trailer;
};

static_assert(alignof(PlacementRegisterArguments) == 16,
              "placement aggregate alignment");
static_assert(sizeof(PlacementRegisterArguments) == 16,
              "placement aggregate size");

static PlacementRegisterArguments make_placement_register_arguments() {
    return PlacementRegisterArguments{2.75, 0x2468, 0x1357};
}

struct PlacementStackArguments {
    double values[3];
};

void* operator new(allocation_size_type size) {
    (void)size;
    ++allocation_calls;
    return &allocation_slot;
}

void operator delete(void* pointer) noexcept {
    if (pointer == &allocation_slot) ++deallocation_calls;
    if (pointer == &global_scalar_placement_allocation_slot[0]) {
        ++global_scalar_placement_regular_delete_calls;
    }
}

void operator delete(void* pointer, allocation_size_type size) noexcept {
    (void)pointer;
    (void)size;
}

void* operator new(allocation_size_type size, int* state, int marker);
void operator delete(void* pointer, int* state, int marker) noexcept;

void* operator new[](allocation_size_type size) {
    array_allocation_last_size = size;
    ++array_allocation_calls;
    return &array_allocation_slot[0];
}

void operator delete[](void* pointer) noexcept {
    if (pointer == &array_allocation_slot[0]) ++array_deallocation_calls;
}

void operator delete[](void* pointer,
                       allocation_size_type size) noexcept {
    (void)pointer;
    (void)size;
}

struct ThrowingScalarAllocation {
    static void* operator new(allocation_size_type size);
    static void operator delete(void* pointer) noexcept;

    ThrowingScalarAllocation() { throw 103; }
};

void* ThrowingScalarAllocation::operator new(allocation_size_type size) {
    if (size == sizeof(ThrowingScalarAllocation)) {
        ++scalar_constructor_allocation_calls;
    }
    return &scalar_constructor_allocation_slot[0];
}

void ThrowingScalarAllocation::operator delete(void* pointer) noexcept {
    if (pointer == &scalar_constructor_allocation_slot[0]) {
        ++scalar_constructor_deallocation_calls;
    }
}

static int next_scalar_placement_marker() {
    ++scalar_placement_argument_evaluations;
    return 0x5a31;
}

struct ThrowingScalarPlacement {
    static void* operator new(
        allocation_size_type size, int* state, int marker);
    static void operator delete(
        void* pointer, int* state, int marker) noexcept;
    static void operator delete(void* pointer) noexcept;

    ThrowingScalarPlacement() { throw 109; }
};

void* ThrowingScalarPlacement::operator new(
    allocation_size_type size, int* state, int marker) {
    if (size != sizeof(ThrowingScalarPlacement) ||
        state != &scalar_placement_state || marker != 0x5a31) {
        scalar_placement_delete_mismatch |= 1;
    }
    ++scalar_placement_new_calls;
    return &scalar_placement_allocation_slot[0];
}

void ThrowingScalarPlacement::operator delete(
    void* pointer, int* state, int marker) noexcept {
    if (pointer != &scalar_placement_allocation_slot[0]) {
        scalar_placement_delete_mismatch |= 2;
    }
    if (state != &scalar_placement_state) {
        scalar_placement_delete_mismatch |= 4;
    }
    if (marker != 0x5a31) scalar_placement_delete_mismatch |= 8;
    ++scalar_placement_delete_calls;
}

void ThrowingScalarPlacement::operator delete(void* pointer) noexcept {
    if (pointer == &scalar_placement_allocation_slot[0]) {
        ++scalar_placement_regular_delete_calls;
    }
}

static int next_global_scalar_placement_marker() {
    ++global_scalar_placement_argument_evaluations;
    return 0x6b42;
}

struct ThrowingGlobalScalarPlacement {
    ThrowingGlobalScalarPlacement() { throw 127; }
};

void* operator new(
    allocation_size_type size, int* state, int marker) {
    if (size != sizeof(ThrowingGlobalScalarPlacement) ||
        state != &scalar_placement_state || marker != 0x6b42) {
        global_scalar_placement_delete_mismatch |= 1;
    }
    ++global_scalar_placement_new_calls;
    return &global_scalar_placement_allocation_slot[0];
}

void operator delete(void* pointer, int* state, int marker) noexcept {
    if (pointer != &global_scalar_placement_allocation_slot[0]) {
        global_scalar_placement_delete_mismatch |= 2;
    }
    if (state != &scalar_placement_state) {
        global_scalar_placement_delete_mismatch |= 4;
    }
    if (marker != 0x6b42) global_scalar_placement_delete_mismatch |= 8;
    ++global_scalar_placement_delete_calls;
}

struct GlobalCookieArray {
    int value;

    GlobalCookieArray() noexcept : value(67) {
        ++cookie_array_constructor_calls;
    }
    GlobalCookieArray(int initial_value) noexcept
        : value(initial_value) {
        ++cookie_array_constructor_calls;
    }
    ~GlobalCookieArray() noexcept {
        ++cookie_array_destructor_calls;
    }
};

struct ThrowingPlacementArray {
    int value;

    static void* operator new[](
        allocation_size_type size, int* state, int marker, double ratio,
        float narrow_ratio,
        double factor_a, double factor_b, double factor_c, double factor_d,
        double factor_e, double factor_f, double factor_g, double factor_h,
        unsigned long long cookie,
        int spill_a, int spill_b, int spill_c, int spill_d);
    static void operator delete[](
        void* pointer, int* state, int marker, double ratio,
        float narrow_ratio,
        double factor_a, double factor_b, double factor_c, double factor_d,
        double factor_e, double factor_f, double factor_g, double factor_h,
        unsigned long long cookie,
        int spill_a, int spill_b, int spill_c, int spill_d) noexcept;
    static void operator delete[](void* pointer) noexcept;

    ThrowingPlacementArray() : value(0) {
        value = ++placement_array_default_constructor_calls;
        if (value == 3) throw 89;
    }
    ThrowingPlacementArray(int initial_value) : value(initial_value) {
        if (initial_value == 41) throw 73;
        ++placement_array_constructor_calls;
    }
    ~ThrowingPlacementArray() noexcept {
        ++placement_array_destructor_calls;
        placement_array_destructor_order =
            placement_array_destructor_order * 100 + value;
    }
};

struct AggregatePlacementArray {
    int value;

    static void* operator new[](
        allocation_size_type size,
        PlacementRegisterArguments register_arguments,
        PlacementStackArguments stack_arguments);
    static void operator delete[](
        void* pointer,
        PlacementRegisterArguments register_arguments,
        PlacementStackArguments stack_arguments) noexcept;
    static void operator delete[](void* pointer) noexcept;

    AggregatePlacementArray(int initial_value) : value(initial_value) {
        if (initial_value == 43) throw 97;
        ++aggregate_placement_constructor_calls;
    }
    ~AggregatePlacementArray() noexcept {
        if (aggregate_placement_destructor_calls < 2) {
            aggregate_placement_destructor_order[
                aggregate_placement_destructor_calls] = value;
        }
        ++aggregate_placement_destructor_calls;
    }
};

void* ThrowingPlacementArray::operator new[](
    allocation_size_type size, int* state, int marker, double ratio,
    float narrow_ratio,
    double factor_a, double factor_b, double factor_c, double factor_d,
    double factor_e, double factor_f, double factor_g, double factor_h,
    unsigned long long cookie,
    int spill_a, int spill_b, int spill_c, int spill_d) {
    placement_array_last_size = size;
    if (state == &placement_array_state && marker == 0x1357 &&
        ratio == 2.5 && narrow_ratio == 1.25f &&
        factor_a == 1.5 && factor_b == 3.5 &&
        factor_c == 4.5 && factor_d == 5.5 && factor_e == 6.5 &&
        factor_f == 7.5 && factor_g == 8.5 && factor_h == 9.5 &&
        cookie == 0x1122334455667788ULL &&
        spill_a == 11 && spill_b == 13 &&
        spill_c == 17 && spill_d == 19) {
        ++placement_array_new_calls;
    }
    return &placement_array_slot[0];
}

void ThrowingPlacementArray::operator delete[](
    void* pointer, int* state, int marker, double ratio,
    float narrow_ratio,
    double factor_a, double factor_b, double factor_c, double factor_d,
    double factor_e, double factor_f, double factor_g, double factor_h,
    unsigned long long cookie,
    int spill_a, int spill_b, int spill_c, int spill_d) noexcept {
    if (pointer == &placement_array_slot[0]) {
        unsigned int mismatch = 0u;
        ++placement_array_delete_calls;
        if (state != &placement_array_state) mismatch |= 1u << 0;
        if (marker != 0x1357) mismatch |= 1u << 1;
        if (ratio != 2.5) mismatch |= 1u << 2;
        if (narrow_ratio != 1.25f) mismatch |= 1u << 3;
        if (factor_a != 1.5) mismatch |= 1u << 4;
        if (factor_b != 3.5) mismatch |= 1u << 5;
        if (factor_c != 4.5) mismatch |= 1u << 6;
        if (factor_d != 5.5) mismatch |= 1u << 7;
        if (factor_e != 6.5) mismatch |= 1u << 8;
        if (factor_f != 7.5) mismatch |= 1u << 9;
        if (factor_g != 8.5) mismatch |= 1u << 10;
        if (factor_h != 9.5) mismatch |= 1u << 11;
        if (cookie != 0x1122334455667788ULL) mismatch |= 1u << 12;
        if (spill_a != 11) mismatch |= 1u << 13;
        if (spill_b != 13) mismatch |= 1u << 14;
        if (spill_c != 17) mismatch |= 1u << 15;
        if (spill_d != 19) mismatch |= 1u << 16;
        placement_array_delete_argument_mismatch |= mismatch;
        if (mismatch == 0u) {
            ++placement_array_delete_argument_matches;
        }
    }
}

void ThrowingPlacementArray::operator delete[](void* pointer) noexcept {
    if (pointer == &placement_array_slot[0]) {
        ++placement_array_regular_delete_calls;
    }
}

void* AggregatePlacementArray::operator new[](
    allocation_size_type size,
    PlacementRegisterArguments register_arguments,
    PlacementStackArguments stack_arguments) {
    aggregate_placement_last_size = size;
    int mismatch = 0;
    if (register_arguments.ratio != 2.75) mismatch |= 1 << 0;
    if (register_arguments.marker != 0x2468) mismatch |= 1 << 1;
    if (register_arguments.trailer != 0x1357) mismatch |= 1 << 2;
    if (stack_arguments.values[0] != 3.25) mismatch |= 1 << 3;
    if (stack_arguments.values[1] != 4.5) mismatch |= 1 << 4;
    if (stack_arguments.values[2] != 5.75) mismatch |= 1 << 5;
    aggregate_placement_new_mismatch |= mismatch;
    if (mismatch == 0) ++aggregate_placement_new_calls;
    return &aggregate_placement_slot[0];
}

void AggregatePlacementArray::operator delete[](
    void* pointer,
    PlacementRegisterArguments register_arguments,
    PlacementStackArguments stack_arguments) noexcept {
    if (pointer == &aggregate_placement_slot[0]) {
        int mismatch = 0;
        ++aggregate_placement_delete_calls;
        if (register_arguments.ratio != 2.75) mismatch |= 1 << 0;
        if (register_arguments.marker != 0x2468) mismatch |= 1 << 1;
        if (register_arguments.trailer != 0x1357) mismatch |= 1 << 2;
        if (stack_arguments.values[0] != 3.25) mismatch |= 1 << 3;
        if (stack_arguments.values[1] != 4.5) mismatch |= 1 << 4;
        if (stack_arguments.values[2] != 5.75) mismatch |= 1 << 5;
        aggregate_placement_delete_mismatch |= mismatch;
        if (mismatch == 0) ++aggregate_placement_delete_matches;
    }
}

void AggregatePlacementArray::operator delete[](void* pointer) noexcept {
    if (pointer == &aggregate_placement_slot[0]) {
        ++aggregate_placement_regular_delete_calls;
    }
}

int main() {
    int* value = new int(73);
    bool initialized = allocation_calls == 1 && *value == 73;
    delete value;
    int scalar_constructor_exception = 0;
    try {
        (void)new ThrowingScalarAllocation;
    } catch (int exception_value) {
        scalar_constructor_exception = exception_value;
    }
    bool scalar_constructor_cleanup = scalar_constructor_exception == 103 &&
        scalar_constructor_allocation_calls == 1 &&
        scalar_constructor_deallocation_calls == 1;
    int scalar_placement_exception = 0;
    try {
        (void)new (&scalar_placement_state,
                   next_scalar_placement_marker()) ThrowingScalarPlacement;
    } catch (int exception_value) {
        scalar_placement_exception = exception_value;
    }
    bool scalar_placement_cleanup = scalar_placement_exception == 109 &&
        scalar_placement_argument_evaluations == 1 &&
        scalar_placement_new_calls == 1 &&
        scalar_placement_delete_calls == 1 &&
        scalar_placement_regular_delete_calls == 0 &&
        scalar_placement_delete_mismatch == 0;
    int global_scalar_placement_exception = 0;
    try {
        (void)new (&scalar_placement_state,
                   next_global_scalar_placement_marker())
            ThrowingGlobalScalarPlacement;
    } catch (int exception_value) {
        global_scalar_placement_exception = exception_value;
    }
    bool global_scalar_placement_cleanup =
        global_scalar_placement_exception == 127 &&
        global_scalar_placement_argument_evaluations == 1 &&
        global_scalar_placement_new_calls == 1 &&
        global_scalar_placement_delete_calls == 1 &&
        global_scalar_placement_regular_delete_calls == 0 &&
        global_scalar_placement_delete_mismatch == 0;
    int* values = new int[2];
    values[0] = 19;
    values[1] = 23;
    bool array_initialized = array_allocation_calls == 1 &&
        array_allocation_last_size == sizeof(int) * 2 &&
        values[0] == 19 && values[1] == 23;
    delete[] values;
    int* initialized_values = new int[4]{29, 31};
    bool scalar_array_explicitly_initialized =
        array_allocation_calls == 2 &&
        array_allocation_last_size == sizeof(int) * 4 &&
        initialized_values[0] == 29 && initialized_values[1] == 31 &&
        initialized_values[2] == 0 && initialized_values[3] == 0;
    delete[] initialized_values;
    GlobalCookieArray* objects = new GlobalCookieArray[2];
    bool cookie_array_initialized = array_allocation_calls == 3 &&
        array_allocation_last_size == sizeof(GlobalCookieArray) * 2 +
            sizeof(allocation_size_type) &&
        cookie_array_constructor_calls == 2 &&
        objects[0].value == 67 && objects[1].value == 67;
    delete[] objects;
    GlobalCookieArray* initialized_objects =
        new GlobalCookieArray[2]{31, 41};
    bool cookie_array_explicitly_initialized =
        array_allocation_calls == 4 &&
        array_allocation_last_size == sizeof(GlobalCookieArray) * 2 +
            sizeof(allocation_size_type) &&
        cookie_array_constructor_calls == 4 &&
        initialized_objects[0].value == 31 &&
        initialized_objects[1].value == 41;
    delete[] initialized_objects;
    int placement_array_exception = 0;
    placement_array_marker = 0x1357;
    try {
        ThrowingPlacementArray* throwing_objects =
            new (&placement_array_state, placement_array_marker, 2.5,
                 1.25f, 1.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5,
                 0x1122334455667788ULL,
                 11, 13, 17, 19)
                ThrowingPlacementArray[3]{
                17, 31, 41};
        delete[] throwing_objects;
    } catch (int exception_value) {
        placement_array_exception = exception_value;
    }
    bool explicit_placement_array_cleanup =
        placement_array_exception == 73 &&
        placement_array_new_calls == 1 &&
        placement_array_last_size == sizeof(ThrowingPlacementArray) * 3 +
            sizeof(allocation_size_type) &&
        placement_array_constructor_calls == 2 &&
        placement_array_destructor_calls == 2 &&
        placement_array_destructor_order == 3117 &&
        placement_array_delete_calls == 1 &&
        placement_array_delete_argument_matches == 1 &&
        placement_array_regular_delete_calls == 0;
    if (!explicit_placement_array_cleanup) {
        if (placement_array_exception != 73) return 21;
        if (placement_array_new_calls != 1) return 22;
        if (placement_array_last_size != sizeof(ThrowingPlacementArray) * 3 +
                sizeof(allocation_size_type)) return 23;
        if (placement_array_constructor_calls != 2) return 24;
        if (placement_array_destructor_calls != 2 ||
            placement_array_delete_calls != 1) {
            return 30 + placement_array_destructor_calls * 10 +
                placement_array_delete_calls;
        }
        if (placement_array_delete_argument_mismatch != 0u) {
            for (unsigned int index = 0u; index < 17u; ++index) {
                if ((placement_array_delete_argument_mismatch &
                     (1u << index)) != 0u) {
                    return 70 + (int)index;
                }
            }
        }
        if (placement_array_delete_argument_matches != 1) return 50;
        if (placement_array_regular_delete_calls != 0) return 28;
        return 29;
    }
    placement_array_new_calls = 0;
    placement_array_default_constructor_calls = 0;
    placement_array_destructor_calls = 0;
    placement_array_destructor_order = 0;
    placement_array_delete_calls = 0;
    placement_array_regular_delete_calls = 0;
    placement_array_delete_argument_matches = 0;
    placement_array_delete_argument_mismatch = 0u;
    placement_array_exception = 0;
    try {
        ThrowingPlacementArray* throwing_default_objects =
            new (&placement_array_state, placement_array_marker, 2.5,
                 1.25f, 1.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5,
                 0x1122334455667788ULL,
                 11, 13, 17, 19)
                ThrowingPlacementArray[3];
        delete[] throwing_default_objects;
    } catch (int exception_value) {
        placement_array_exception = exception_value;
    }
    bool default_placement_array_cleanup =
        placement_array_exception == 89 &&
        placement_array_new_calls == 1 &&
        placement_array_last_size == sizeof(ThrowingPlacementArray) * 3 +
            sizeof(allocation_size_type) &&
        placement_array_default_constructor_calls == 3 &&
        placement_array_destructor_calls == 2 &&
        placement_array_destructor_order == 201 &&
        placement_array_delete_calls == 1 &&
        placement_array_delete_argument_matches == 1 &&
        placement_array_regular_delete_calls == 0;
    if (!default_placement_array_cleanup) {
        if (placement_array_exception != 89) return 61;
        if (placement_array_new_calls != 1) return 62;
        if (placement_array_default_constructor_calls != 3) return 63;
        if (placement_array_destructor_calls != 2 ||
            placement_array_delete_calls != 1) return 64;
        if (placement_array_delete_argument_mismatch != 0u) {
            for (unsigned int index = 0u; index < 17u; ++index) {
                if ((placement_array_delete_argument_mismatch &
                     (1u << index)) != 0u) {
                    return 90 + (int)index;
                }
            }
        }
        if (placement_array_delete_argument_matches != 1) return 65;
        if (placement_array_regular_delete_calls != 0) return 66;
        return 67;
    }
    PlacementRegisterArguments register_arguments =
        make_placement_register_arguments();
    PlacementStackArguments stack_arguments;
    stack_arguments.values[0] = 3.25;
    stack_arguments.values[1] = 4.5;
    stack_arguments.values[2] = 5.75;
    int aggregate_placement_exception = 0;
    try {
        AggregatePlacementArray* aggregate_objects =
            new (register_arguments, stack_arguments)
                AggregatePlacementArray[3]{17, 31, 43};
        delete[] aggregate_objects;
    } catch (int exception_value) {
        aggregate_placement_exception = exception_value;
    }
    bool aggregate_placement_cleanup =
        aggregate_placement_exception == 97 &&
        aggregate_placement_new_calls == 1 &&
        aggregate_placement_last_size ==
            sizeof(AggregatePlacementArray) * 3 +
                sizeof(allocation_size_type) &&
        aggregate_placement_constructor_calls == 2 &&
        aggregate_placement_destructor_calls == 2 &&
        aggregate_placement_destructor_order[0] == 31 &&
        aggregate_placement_destructor_order[1] == 17 &&
        aggregate_placement_delete_calls == 1 &&
        aggregate_placement_delete_matches == 1 &&
        aggregate_placement_delete_mismatch == 0 &&
        aggregate_placement_regular_delete_calls == 0;
    if (!aggregate_placement_cleanup) {
        if (aggregate_placement_exception != 97) return 4;
        if (aggregate_placement_new_calls != 1) {
            if (aggregate_placement_new_mismatch != 0) {
                for (int index = 0; index < 6; ++index) {
                    if ((aggregate_placement_new_mismatch &
                         (1 << index)) != 0) return 20 + index;
                }
            }
            return 5;
        }
        if (aggregate_placement_last_size !=
            sizeof(AggregatePlacementArray) * 3 +
                sizeof(allocation_size_type)) return 6;
        if (aggregate_placement_constructor_calls != 2) return 7;
        if (aggregate_placement_destructor_calls != 2) return 8;
        if (aggregate_placement_destructor_order[0] != 31 ||
            aggregate_placement_destructor_order[1] != 17) return 13;
        if (aggregate_placement_delete_calls != 1) return 9;
        if (aggregate_placement_delete_mismatch != 0) {
            for (int index = 0; index < 6; ++index) {
                if ((aggregate_placement_delete_mismatch &
                     (1 << index)) != 0) return 30 + index;
            }
        }
        if (aggregate_placement_delete_matches != 1) return 10;
        if (aggregate_placement_regular_delete_calls != 0) return 11;
        return 12;
    }
    bool existing_checks = initialized && array_initialized &&
        scalar_array_explicitly_initialized && deallocation_calls == 1 &&
        scalar_constructor_cleanup &&
        scalar_placement_cleanup &&
        global_scalar_placement_cleanup &&
        cookie_array_initialized && cookie_array_explicitly_initialized &&
        cookie_array_destructor_calls == 4 && array_deallocation_calls == 4;
    if (!existing_checks) return 100;
    if (!explicit_placement_array_cleanup) return 101;
    if (!default_placement_array_cleanup) return 102;
    return 0;
}
