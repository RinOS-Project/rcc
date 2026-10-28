template<typename T>
int explicitly_specialized(T value) noexcept {
    return static_cast<int>(value) + 1;
}

template<typename T>
int conditionally_noexcept(T value) noexcept(sizeof(T) == sizeof(int)) {
    return static_cast<int>(value);
}

template<>
int explicitly_specialized<int>(int value) noexcept;

template<>
int explicitly_specialized<int>(int value) noexcept {
    return value + 40;
}

template<>
int conditionally_noexcept<int>(int value) noexcept(true) {
    return value + 10;
}

template<>
int conditionally_noexcept<long long>(long long value) noexcept(false) {
    return static_cast<int>(value) + 20;
}

template<typename T>
int select_explicit_specialization(T value) {
    return value ? 10 : 11;
}

template<typename T>
int select_explicit_specialization(T* value) {
    return value ? 20 : 21;
}

template<typename T>
int mixed_template_argument_kinds(int value) {
    return value + 1;
}

template<int Value>
int mixed_template_argument_kinds(int value) {
    return value + Value;
}

template<typename... Types>
int mixed_parameter_pack_shape(int value) {
    return value + 10;
}

template<int Base, typename... Types>
int mixed_parameter_pack_shape(int value) {
    return value + Base + 20;
}

template<typename... Types>
int mixed_pack_argument_kinds(int value) {
    return value + 30;
}

template<int... Values>
int mixed_pack_argument_kinds(int value) {
    return value + 40;
}

template<typename T>
struct explicit_template_argument_holder {};
template<typename T>
struct second_explicit_template_argument_holder {};

template<template<typename> class Template>
int mixed_template_template_argument_kinds(int value) {
    return value + 50;
}

template<int Value>
int mixed_template_template_argument_kinds(int value) {
    return value + Value;
}

template<template<typename> class... Templates>
int mixed_template_template_pack_argument_kinds(int value) {
    return value + 60;
}

template<typename... Types>
int mixed_template_template_pack_argument_kinds(int value) {
    return value + 70;
}

template<typename... First, typename... Second>
int mixed_multiple_explicit_type_packs(int value) {
    return value + sizeof...(First) * 10 + sizeof...(Second);
}

template<int... Values>
int mixed_multiple_explicit_type_packs(int value) {
    return value + 100 + sizeof...(Values);
}

template<typename... Types, int Bias = 5>
int mixed_pack_with_trailing_default(int value) {
    return value + sizeof...(Types) + Bias;
}

template<int... Values>
int mixed_pack_with_trailing_default(int value) {
    return value + 100 + sizeof...(Values);
}

template<>
int select_explicit_specialization<int*>(int* value) {
    return value ? 30 : 31;
}

template<>
int mixed_template_argument_kinds<short>(int value) {
    return value + 30;
}

template<>
int mixed_template_argument_kinds<7>(int value) {
    return value + 40;
}

template<typename T>
int dependent_mixed_type_argument(T value) {
    return mixed_template_argument_kinds<short>(value);
}

template<typename T>
int dependent_mixed_value_argument(T value) {
    return mixed_template_argument_kinds<7>(value);
}

template<typename T>
int dependent_mixed_template_template_argument(T value) {
    return mixed_template_template_argument_kinds<
        explicit_template_argument_holder>(value);
}

template<typename T>
int dependent_mixed_template_template_pack_argument(T value) {
    return mixed_template_template_pack_argument_kinds<
        explicit_template_argument_holder,
        second_explicit_template_argument_holder>(value);
}

namespace explicit_specialization_scope {
template<typename T, int Factor = 2>
int scale(T value) {
    return static_cast<int>(value) * Factor;
}

template<typename T, int Factor = 2>
int scale_with_default(T value) {
    return static_cast<int>(value) * Factor;
}

template<>
int scale<int, 3>(int value) {
    return value * 7;
}

template<>
int scale_with_default<int>(int value) {
    return value * 6;
}
}

template<>
int explicit_specialization_scope::scale<long, 3>(long value) {
    return static_cast<int>(value * 9);
}

int main() {
    int value = 5;
    return explicitly_specialized(2) == 42 &&
                   explicitly_specialized(2.0) == 3 &&
                   conditionally_noexcept(2) == 12 &&
                   conditionally_noexcept(2LL) == 22 &&
                   select_explicit_specialization<int*>(&value) == 30 &&
                   mixed_template_argument_kinds<short>(12) == 42 &&
                   mixed_template_argument_kinds<7>(2) == 42 &&
                   mixed_parameter_pack_shape<short>(2) == 12 &&
                   mixed_parameter_pack_shape<7, short>(1) == 28 &&
                   mixed_pack_argument_kinds<short>(12) == 42 &&
                   mixed_pack_argument_kinds<7>(2) == 42 &&
                   mixed_template_template_argument_kinds<
                       explicit_template_argument_holder>(2) == 52 &&
                   mixed_template_template_argument_kinds<7>(2) == 9 &&
                   mixed_template_template_pack_argument_kinds<
                       explicit_template_argument_holder>(2) == 62 &&
                   mixed_template_template_pack_argument_kinds<
                       explicit_template_argument_holder,
                       second_explicit_template_argument_holder>(2) == 62 &&
                   mixed_multiple_explicit_type_packs<short, char>(1) == 21 &&
                   mixed_multiple_explicit_type_packs<1, 2>(1) == 103 &&
                   mixed_pack_with_trailing_default<short, char>(1) == 8 &&
                   mixed_pack_with_trailing_default<1, 2>(1) == 103 &&
                   dependent_mixed_type_argument(9) == 39 &&
                   dependent_mixed_value_argument(9) == 49 &&
                   dependent_mixed_template_template_argument(9) == 59 &&
                   dependent_mixed_template_template_pack_argument(9) == 69 &&
                   explicit_specialization_scope::scale<int, 3>(6) == 42 &&
                   explicit_specialization_scope::scale_with_default<int>(7) == 42 &&
                   explicit_specialization_scope::scale<long, 3>(5L) == 45
               ? 0
               : 1;
}
