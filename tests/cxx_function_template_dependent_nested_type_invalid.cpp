struct MissingFunctionTemplateNestedType {};

struct PrivateFunctionTemplateNestedType {
private:
    using value_type = int;
};

struct FunctionTemplateNestedPrivateLeaf {
private:
    using nested_type = int;
};

struct FunctionTemplateNestedPrivateOuter {
    using value_type = FunctionTemplateNestedPrivateLeaf;
};

struct FunctionTemplateNestedMissingLeaf {};

struct FunctionTemplateNestedMissingOuter {
    using value_type = FunctionTemplateNestedMissingLeaf;
};

template <typename T>
typename T::value_type function_template_return_type() {
    return 1;
}

template <typename T>
int function_template_parameter_type(typename T::value_type value) {
    return value;
}

template <typename T>
typename T::value_type::nested_type function_template_nested_chain() {
    return 2;
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
    result += function_template_nested_chain<
        FunctionTemplateNestedPrivateOuter>();
    result += function_template_nested_chain<
        FunctionTemplateNestedMissingOuter>();
    return result;
}
