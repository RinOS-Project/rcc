#if !defined(__cpp_attributes) || __cpp_attributes != 200809L
#error "RCC++ must expose the C++ attribute syntax feature macro"
#endif

#if !defined(__cpp_decltype) || __cpp_decltype != 200707L
#error "RCC++ must expose the decltype feature macro"
#endif

#if !defined(__cpp_lambdas) || __cpp_lambdas != 200907L
#error "RCC++ must expose the lambda feature macro"
#endif

#if !defined(__cpp_static_assert) || __cpp_static_assert != 200410L
#error "RCC++ must expose the static_assert feature macro"
#endif

#if __cplusplus < 201402L
#ifdef __cpp_deprecated
#error "deprecated must not be advertised before C++14"
#endif
#ifdef __cpp_init_captures
#error "init-captures must not be advertised before C++14"
#endif
#ifdef __cpp_generic_lambdas
#error "generic lambdas must not be advertised before C++14"
#endif
#ifdef __cpp_variable_templates
#error "variable templates must not be advertised before C++14"
#endif
#else
#if !defined(__cpp_deprecated) || __cpp_deprecated != 201309L
#error "RCC++ must report C++14 deprecated support"
#endif
#if !defined(__cpp_init_captures) || __cpp_init_captures != 201304L
#error "RCC++ must report C++14 init-capture support"
#endif
#if !defined(__cpp_generic_lambdas) || __cpp_generic_lambdas != 201304L
#if __cplusplus < 202002L
#error "RCC++ must report C++14 generic lambda support"
#endif
#endif
#if !defined(__cpp_variable_templates) || __cpp_variable_templates != 201304L
#error "RCC++ must report C++14 variable-template support"
#endif
#endif

#if __cplusplus < 201703L
#ifdef __cpp_if_constexpr
#error "if constexpr must not be advertised before C++17"
#endif
#ifdef __cpp_structured_bindings
#error "structured bindings must not be advertised before C++17"
#endif
#ifdef __cpp_inline_variables
#error "inline variables must not be advertised before C++17"
#endif
#ifdef __cpp_nodiscard
#error "nodiscard must not be advertised before C++17"
#endif
#ifdef __cpp_fold_expressions
#error "fold expressions must not be advertised before C++17"
#endif
#ifdef __cpp_nested_namespace_definitions
#error "nested namespace definitions must not be advertised before C++17"
#endif
#ifdef __cpp_nontype_template_args
#error "auto non-type template arguments must not be advertised before C++17"
#endif
#ifdef __cpp_deduction_guides
#error "class template argument deduction must not be advertised before C++17"
#endif
#else
#if !defined(__cpp_fold_expressions) || __cpp_fold_expressions != 201603L
#error "RCC++ must report C++17 fold-expression support"
#endif
#if !defined(__cpp_nested_namespace_definitions) || __cpp_nested_namespace_definitions != 201411L
#error "RCC++ must report C++17 nested namespace support"
#endif
#if !defined(__cpp_nontype_template_args) || __cpp_nontype_template_args != 201411L
#error "RCC++ must report C++17 auto non-type template support"
#endif
#if !defined(__cpp_deduction_guides) || __cpp_deduction_guides != 201611L
#error "RCC++ must report C++17 class template argument deduction support"
#endif
#if !defined(__cpp_if_constexpr) || __cpp_if_constexpr != 201606L
#error "RCC++ must report C++17 if constexpr support"
#endif
#if !defined(__cpp_structured_bindings) || __cpp_structured_bindings != 201606L
#error "RCC++ must report C++17 structured binding support"
#endif
#if !defined(__cpp_inline_variables) || __cpp_inline_variables != 201606L
#error "RCC++ must report C++17 inline variable support"
#endif
#endif

#if __cplusplus < 201703L
#if __cpp_range_based_for != 0
#error "this bounded profile only advertises the C++17 range-for form"
#endif
#elif !defined(__cpp_range_based_for) || __cpp_range_based_for != 201603L
#error "RCC++ must report C++17 range-for support"
#endif

#if __cplusplus < 202002L
#ifdef __cpp_consteval
#error "consteval must not be advertised before C++20"
#endif
#ifdef __cpp_constinit
#error "constinit must not be advertised before C++20"
#endif
#ifdef __cpp_concepts
#error "concepts must not be advertised before C++20"
#endif
#ifdef __cpp_char8_t
#error "char8_t must not be advertised before C++20"
#endif
#ifdef __cpp_using_enum
#error "using enum must not be advertised before C++20"
#endif
#ifdef __cpp_designated_initializers
#error "designated initializers must not be advertised before C++20"
#endif
#ifdef __cpp_conditional_explicit
#error "conditional explicit must not be advertised before C++20"
#endif
#ifdef __cpp_aggregate_paren_init
#error "aggregate paren initialization must not be advertised before C++20"
#endif
#else
#if !defined(__cpp_aggregate_paren_init) || __cpp_aggregate_paren_init != 201902L
#error "RCC++ must report C++20 aggregate paren initialization support"
#endif
#if !defined(__cpp_consteval) || __cpp_consteval != 201811L
#error "RCC++ must report C++20 consteval support"
#endif
#if !defined(__cpp_constinit) || __cpp_constinit != 201907L
#error "RCC++ must report C++20 constinit support"
#endif
#if !defined(__cpp_concepts) || __cpp_concepts != 201907L
#error "RCC++ must report C++20 concepts support"
#endif
#if !defined(__cpp_char8_t) || __cpp_char8_t != 201811L
#error "RCC++ must report C++20 char8_t support"
#endif
#if !defined(__cpp_using_enum) || __cpp_using_enum != 201907L
#error "RCC++ must report C++20 using-enum support"
#endif
#if !defined(__cpp_designated_initializers) || __cpp_designated_initializers != 201707L
#error "RCC++ must report C++20 designated initializer support"
#endif
#if !defined(__cpp_conditional_explicit) || __cpp_conditional_explicit != 201806L
#error "RCC++ must report C++20 conditional explicit support"
#endif
#if __cpp_nodiscard != 201907L
#error "RCC++ must report the C++20 nodiscard value"
#endif
#endif

#if __cplusplus >= 201402L
int rcc_generic_lambda_feature(void) {
    return [](auto value) { return value + 1; }(4);
}

int rcc_lambda_init_capture_feature(void) {
    return [value = 4]() { return value; }();
}
#endif

#if __cplusplus >= 201703L
inline int rcc_inline_variable_feature = 3;

int rcc_constexpr_lambda_feature(void) {
    return []() constexpr { return 7; }();
}

template<typename... Values>
int rcc_fold_feature(Values... values) {
    return (values + ...);
}

template<auto Value>
int rcc_auto_nttp_feature(void) {
    return Value;
}

int rcc_selection_initializer_feature(void) {
    if (int value = rcc_inline_variable_feature; value) return value;
    switch (int value = 0; value) {
    case 0:
        return 0;
    default:
        return value;
    }
}
#endif

#if __cplusplus >= 202002L
struct RccDesignatedFeature {
    int value;
};

int rcc_designated_initializer_feature(void) {
    RccDesignatedFeature value{.value = 5};
    return value.value;
}

int rcc_consteval_lambda_feature(void) {
    return []() consteval { return 8; }();
}
#endif

#if __cplusplus >= 201703L
namespace rcc::versioned_feature {
int nested_namespace_feature(void) {
    return 6;
}
}
#endif

int rcc_cpp_feature_macro_probe(void) {
    int result = 0;
#if __cplusplus >= 201402L
    result += rcc_generic_lambda_feature();
    result += rcc_lambda_init_capture_feature();
#endif
#if __cplusplus >= 201703L
    result += rcc_selection_initializer_feature();
    result += rcc_fold_feature(1, 2, 3);
    result += rcc_auto_nttp_feature<4>();
    result += rcc_constexpr_lambda_feature();
#endif
#if __cplusplus >= 202002L
    result += rcc_designated_initializer_feature();
#endif
#if __cplusplus >= 201703L
    result += rcc::versioned_feature::nested_namespace_feature();
#endif
#if __cplusplus >= 202002L
    result += rcc_consteval_lambda_feature();
#endif
    return result == 0;
}

int main() {
    return rcc_cpp_feature_macro_probe();
}
