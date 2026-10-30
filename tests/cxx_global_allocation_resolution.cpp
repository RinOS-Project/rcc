using allocation_size_type = __SIZE_TYPE__;

static int allocation_slot;
static int allocation_calls;
static int deallocation_calls;
static int array_allocation_calls;
static int array_deallocation_calls;
static allocation_size_type array_allocation_last_size;
alignas(8) static unsigned char array_allocation_slot[64];
static int cookie_array_constructor_calls;
static int cookie_array_destructor_calls;

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

int main() {
    int* value = new int(73);
    bool initialized = allocation_calls == 1 && *value == 73;
    delete value;
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
    return initialized && array_initialized &&
            scalar_array_explicitly_initialized && deallocation_calls == 1 &&
            cookie_array_initialized && cookie_array_explicitly_initialized &&
            cookie_array_destructor_calls == 4 &&
            array_deallocation_calls == 4
        ? 0 : 1;
}
