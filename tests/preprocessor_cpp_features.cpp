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
#ifdef __cpp_generic_lambdas
#error "generic lambdas must not be advertised before C++14"
#endif
#else
#if !defined(__cpp_deprecated) || __cpp_deprecated != 201309L
#error "RCC++ must report C++14 deprecated support"
#endif
#if !defined(__cpp_generic_lambdas) || __cpp_generic_lambdas != 201304L
#if __cplusplus < 202002L
#error "RCC++ must report C++14 generic lambda support"
#endif
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
#else
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
#else
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
#if __cpp_nodiscard != 201907L
#error "RCC++ must report the C++20 nodiscard value"
#endif
#endif

int rcc_cpp_feature_macro_probe(void) {
    return 0;
}

int main() {
    return rcc_cpp_feature_macro_probe();
}
