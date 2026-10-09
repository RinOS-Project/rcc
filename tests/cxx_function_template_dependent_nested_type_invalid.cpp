struct MissingFunctionTemplateNestedType {};

struct PrivateFunctionTemplateNestedType {
private:
    using value_type = int;
};

template <typename T>
typename T::value_type function_template_return_type() {
    return 1;
}

template <typename T>
int function_template_parameter_type(typename T::value_type value) {
    return value;
}

int main() {
    int result = function_template_return_type<
        MissingFunctionTemplateNestedType>();
    result += function_template_return_type<
        PrivateFunctionTemplateNestedType>();
    result += function_template_parameter_type<
        MissingFunctionTemplateNestedType>(1);
    result += function_template_parameter_type<
        PrivateFunctionTemplateNestedType>(1);
    return result;
}
