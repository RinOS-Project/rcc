struct MissingFunctionTemplateNestedType {};

template <typename T>
typename T::value_type function_template_return_fallback(T*) {
    return 1;
}

template <typename T>
int function_template_return_fallback(const T*) {
    return 2;
}

int main() {
    MissingFunctionTemplateNestedType value;
    const MissingFunctionTemplateNestedType* pointer = &value;
    return function_template_return_fallback(pointer) == 2 ? 0 : 1;
}
