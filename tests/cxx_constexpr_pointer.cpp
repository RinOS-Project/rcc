int constexpr_pointer_target = 7;
constexpr int constexpr_pointer_values[2] = {11, 13};

constexpr int* first_pointer = &constexpr_pointer_target;
constexpr int* second_pointer = first_pointer + 1;
constexpr const int* array_pointer = &constexpr_pointer_values[1];
constexpr int dereferenced_value = *array_pointer;

constexpr bool pointer_present(int* value) {
    return value != nullptr;
}

static_assert(first_pointer != nullptr, "address constant must be non-null");
static_assert(first_pointer == &constexpr_pointer_target,
              "address identity must be preserved");
static_assert(second_pointer != first_pointer,
              "pointer arithmetic must retain its offset");
static_assert(second_pointer - first_pointer == 1,
              "pointer subtraction must use the element size");
static_assert(pointer_present(first_pointer),
              "pointer values must survive constexpr function bindings");
static_assert(dereferenced_value == 13,
              "pointer dereference must read constant aggregate storage");

extern "C" int probe_constexpr_pointer(void) {
    return first_pointer == &constexpr_pointer_target &&
                   second_pointer == first_pointer + 1 &&
                   *array_pointer == 13
               ? 0
               : 1;
}
