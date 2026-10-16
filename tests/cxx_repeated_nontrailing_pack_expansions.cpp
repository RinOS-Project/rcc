template<typename... Types>
struct RepeatedPackExpansionBox {};

template<typename... Types>
int repeat_pack_expansion(
    RepeatedPackExpansionBox<Types..., int, Types...>,
    RepeatedPackExpansionBox<Types...>) {
    return static_cast<int>(sizeof...(Types));
}

template<typename... FirstTypes, typename... SecondTypes>
int interleave_pack_sources(
    RepeatedPackExpansionBox<FirstTypes..., int, SecondTypes...>,
    RepeatedPackExpansionBox<SecondTypes...>) {
    return static_cast<int>(sizeof...(FirstTypes) * 10 +
                            sizeof...(SecondTypes));
}

template<typename... FirstTypes, typename... SecondTypes>
int independent_pack_partition(
    RepeatedPackExpansionBox<FirstTypes...>,
    RepeatedPackExpansionBox<SecondTypes...>) {
    return static_cast<int>(sizeof...(FirstTypes) * 10 +
                            sizeof...(SecondTypes));
}

template<int... Values>
struct RepeatedValuePackExpansionBox {};

template<int... Values>
int repeat_value_pack_expansion(
    RepeatedValuePackExpansionBox<Values..., 7, Values...>,
    RepeatedValuePackExpansionBox<Values...>) {
    return static_cast<int>(sizeof...(Values));
}

template<template<typename> class... Templates>
struct RepeatedTemplatePackExpansionBox {};

template<typename Type>
struct RepeatedTemplatePackExpansionItem {};

template<typename Type>
struct RepeatedTemplatePackExpansionFixed {};

template<template<typename> class... Templates>
int repeat_template_template_pack_expansion(
    RepeatedTemplatePackExpansionBox<
        Templates..., RepeatedTemplatePackExpansionFixed, Templates...>,
    RepeatedTemplatePackExpansionBox<Templates...>) {
    return static_cast<int>(sizeof...(Templates));
}

int main() {
    if (repeat_pack_expansion(
            RepeatedPackExpansionBox<char, short, int, char, short>{},
            RepeatedPackExpansionBox<char, short>{}) != 2) {
        return 1;
    }
    if (repeat_pack_expansion(RepeatedPackExpansionBox<int>{},
                              RepeatedPackExpansionBox<>{}) != 0) {
        return 2;
    }
    if (interleave_pack_sources<char>(
            RepeatedPackExpansionBox<char, int, short, double>{},
            RepeatedPackExpansionBox<short, double>{}) != 12) {
        return 3;
    }
    if (interleave_pack_sources<>(RepeatedPackExpansionBox<int>{},
                                  RepeatedPackExpansionBox<>{}) != 0) {
        return 4;
    }
    if (interleave_pack_sources(
            RepeatedPackExpansionBox<int, short, double>{},
            RepeatedPackExpansionBox<short, double>{}) != 2) {
        return 5;
    }
    if (independent_pack_partition(RepeatedPackExpansionBox<char>{},
                                   RepeatedPackExpansionBox<short>{}) != 11) {
        return 6;
    }
    if (independent_pack_partition(
            RepeatedPackExpansionBox<char, short>{},
            RepeatedPackExpansionBox<>{}) != 20) {
        return 7;
    }
    if (independent_pack_partition(
            RepeatedPackExpansionBox<>{},
            RepeatedPackExpansionBox<char, short>{}) != 2) {
        return 8;
    }
    if (repeat_value_pack_expansion(
            RepeatedValuePackExpansionBox<1, 2, 7, 1, 2>{},
            RepeatedValuePackExpansionBox<1, 2>{}) != 2) {
        return 9;
    }
    if (repeat_value_pack_expansion(RepeatedValuePackExpansionBox<7>{},
                                    RepeatedValuePackExpansionBox<>{}) != 0) {
        return 10;
    }
    if (repeat_template_template_pack_expansion(
            RepeatedTemplatePackExpansionBox<
                RepeatedTemplatePackExpansionItem,
                RepeatedTemplatePackExpansionFixed,
                RepeatedTemplatePackExpansionItem>{},
            RepeatedTemplatePackExpansionBox<
                RepeatedTemplatePackExpansionItem>{}) != 1) {
        return 11;
    }
    if (repeat_template_template_pack_expansion(
            RepeatedTemplatePackExpansionBox<
                RepeatedTemplatePackExpansionFixed>{},
            RepeatedTemplatePackExpansionBox<>{}) != 0) {
        return 12;
    }
    return 0;
}
