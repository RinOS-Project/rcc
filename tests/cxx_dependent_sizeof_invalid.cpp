struct IncompleteSizeofType;

constexpr unsigned invalid_direct_incomplete_sizeof =
    sizeof(IncompleteSizeofType);
constexpr unsigned invalid_direct_function_sizeof = sizeof(void());

IncompleteSizeofType *incomplete_sizeof_pointer;
void (*function_sizeof_pointer)();

unsigned invalid_incomplete_sizeof_expression() {
    return sizeof(*incomplete_sizeof_pointer);
}

unsigned invalid_function_sizeof_expression() {
    return sizeof(*function_sizeof_pointer);
}

template<typename T>
constexpr unsigned dependent_sizeof_invalid_type() {
    return sizeof(T);
}

template<typename T>
unsigned dependent_sizeof_invalid_expression(T* pointer) {
    return sizeof(*pointer);
}

constexpr unsigned invalid_incomplete_sizeof =
    dependent_sizeof_invalid_type<IncompleteSizeofType>();
constexpr unsigned invalid_function_sizeof =
    dependent_sizeof_invalid_type<void()>();

void instantiate_invalid_dependent_sizeof_expressions() {
    dependent_sizeof_invalid_expression<IncompleteSizeofType>(nullptr);
    dependent_sizeof_invalid_expression<void()>(nullptr);
}
