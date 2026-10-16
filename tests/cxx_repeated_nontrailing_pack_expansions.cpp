template<typename... Types>
struct RepeatedPackExpansionBox {};

template<typename... Types>
int repeat_pack_expansion(
    RepeatedPackExpansionBox<Types..., int, Types...>,
    RepeatedPackExpansionBox<Types...>) {
    return static_cast<int>(sizeof...(Types));
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
    if (repeat_value_pack_expansion(
            RepeatedValuePackExpansionBox<1, 2, 7, 1, 2>{},
            RepeatedValuePackExpansionBox<1, 2>{}) != 2) {
        return 3;
    }
    if (repeat_value_pack_expansion(RepeatedValuePackExpansionBox<7>{},
                                    RepeatedValuePackExpansionBox<>{}) != 0) {
        return 4;
    }
    if (repeat_template_template_pack_expansion(
            RepeatedTemplatePackExpansionBox<
                RepeatedTemplatePackExpansionItem,
                RepeatedTemplatePackExpansionFixed,
                RepeatedTemplatePackExpansionItem>{},
            RepeatedTemplatePackExpansionBox<
                RepeatedTemplatePackExpansionItem>{}) != 1) {
        return 5;
    }
    if (repeat_template_template_pack_expansion(
            RepeatedTemplatePackExpansionBox<
                RepeatedTemplatePackExpansionFixed>{},
            RepeatedTemplatePackExpansionBox<>{}) != 0) {
        return 6;
    }
    return 0;
}
