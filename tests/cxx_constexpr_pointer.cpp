int constexpr_pointer_target = 7;

constexpr int* first_pointer = &constexpr_pointer_target;
constexpr int* second_pointer = first_pointer + 1;

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

extern "C" int probe_constexpr_pointer(void) {
    return first_pointer == &constexpr_pointer_target &&
                   second_pointer == first_pointer + 1
               ? 0
               : 1;
}
