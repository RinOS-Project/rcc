template<typename... Types>
struct Pack {};

template<typename Left, typename Right>
struct Pair {};

template<typename First, typename Second>
struct Duo {};

template<typename Value>
struct Envelope {};

template<typename Value, int Tag>
struct Tagged {};

template<int First, int Second>
struct ValuePair {};

template<typename Value>
struct WrapA {};

template<typename Value>
struct WrapB {};

template<template<typename> class Template, typename Value>
struct Apply {};

template<template<typename> class Template, typename Value, int Tag>
struct TemplateTagged {};

struct NotPair {};
struct NotTagged {};
struct NotApply {};
struct NotTemplateTagged {};

template<typename... Left, typename... Right>
int choose_nested_independent_packs(
    Pair<Pack<Left...>, Pack<Right...>>) {
    return static_cast<int>(sizeof...(Left) * 10 + sizeof...(Right));
}

template<typename... Left, typename... Right>
int choose_separate_independent_packs(Pack<Left...>, Pack<Right...>) {
    return static_cast<int>(sizeof...(Left) * 10 + sizeof...(Right));
}

template<typename... Left, typename... Right>
int choose_zipped_independent_packs(Pair<Left, Right>...) {
    return static_cast<int>(sizeof...(Left) * 10 + sizeof...(Right));
}

template<typename... Left, typename... Right>
int choose_zipped_compound_packs(Pair<Left*, Right>...) {
    return static_cast<int>(sizeof...(Left) * 10 + sizeof...(Right));
}

template<typename... Left, typename... Right>
int choose_zipped_nested_packs(Envelope<Duo<Left, Right>>...) {
    return static_cast<int>(sizeof...(Left) * 10 + sizeof...(Right));
}

template<typename... Types, int... Values>
int choose_zipped_mixed_packs(Tagged<Types, Values>...) {
    return static_cast<int>(sizeof...(Types) * 10 + sizeof...(Values));
}

template<int... Left, int... Right>
int choose_zipped_nontype_packs(ValuePair<Left, Right>...) {
    return static_cast<int>(sizeof...(Left) * 10 + sizeof...(Right));
}

template<template<typename> class... Templates, typename... Types>
int choose_zipped_template_template_packs(Apply<Templates, Types>...) {
    return static_cast<int>(sizeof...(Templates) * 10 + sizeof...(Types));
}

template<template<typename> class... Templates, typename... Types,
         int... Tags>
int choose_zipped_template_type_value_packs(
    TemplateTagged<Templates, Types, Tags>...) {
    return static_cast<int>(sizeof...(Templates) * 100 +
                            sizeof...(Types) * 10 + sizeof...(Tags));
}

int main() {
#if defined(RCC_TEST_INVALID_UNEXPANDED_TEMPLATE_TEMPLATE_PACK)
    return 0;
#elif defined(RCC_TEST_INVALID_ZIPPED_PACK)
    return choose_zipped_independent_packs(NotPair{}) +
           choose_zipped_compound_packs(Pair<int, char>{}) +
           choose_zipped_nested_packs(NotPair{}) +
           choose_zipped_mixed_packs(NotTagged{}) +
           choose_zipped_nontype_packs(NotTagged{}) +
           choose_zipped_template_template_packs(NotApply{}) +
           choose_zipped_template_type_value_packs(NotTemplateTagged{});
#else
    using EmptyPair = Pair<Pack<>, Pack<> >;
    using UnequalPair = Pair<Pack<char>, Pack<int, long> >;
    using WidePair = Pair<Pack<int, char>, Pack<double, long, short> >;
    return choose_nested_independent_packs(EmptyPair{}) == 0 &&
                   choose_nested_independent_packs(UnequalPair{}) == 12 &&
                   choose_nested_independent_packs(WidePair{}) == 23 &&
                   choose_separate_independent_packs(
                       Pack<int, char>{}, Pack<double, long, short>{}) == 23 &&
                   choose_zipped_independent_packs() == 0 &&
                   choose_zipped_independent_packs(
                       Pair<int, char>{}, Pair<long, double>{}) == 22 &&
                   choose_zipped_compound_packs() == 0 &&
                   choose_zipped_compound_packs(
                       Pair<int*, char>{}, Pair<long*, double>{}) == 22 &&
                   choose_zipped_nested_packs() == 0 &&
                   choose_zipped_nested_packs(
                       Envelope<Duo<int, char>>{},
                       Envelope<Duo<long, double>>{}) == 22 &&
                   choose_zipped_mixed_packs() == 0 &&
                   choose_zipped_mixed_packs(
                       Tagged<int, 1>{}, Tagged<long, 2>{}) == 22 &&
                   choose_zipped_nontype_packs() == 0 &&
                   choose_zipped_nontype_packs(
                       ValuePair<2, 3>{}, ValuePair<5, 7>{}) == 22 &&
                   choose_zipped_template_template_packs() == 0 &&
                   choose_zipped_template_template_packs(
                       Apply<WrapA, int>{}, Apply<WrapB, long>{}) == 22 &&
                   choose_zipped_template_type_value_packs() == 0 &&
                   choose_zipped_template_type_value_packs(
                       TemplateTagged<WrapA, int, 3>{},
                       TemplateTagged<WrapB, long, 7>{}) == 222
        ? 0 : 1;
#endif
}

#if defined(RCC_TEST_INVALID_UNEXPANDED_TEMPLATE_TEMPLATE_PACK)
template<template<typename> class... Templates>
int invalid_unexpanded_template_template_pack(
    Apply<Templates, int>) {
    return 0;
}
#endif
