template<int N>
constexpr int doubled = N + N;

template<typename T>
constexpr int type_size = sizeof(T);

template<int N>
int mutable_value = N;

template<int N>
int specialized_non_type = N;

template<>
int specialized_non_type<5> = 9;

template<typename... Types>
int type_pack_count = sizeof...(Types);

template<typename... Types>
int specialized_type_pack_count = sizeof...(Types);

template<>
int specialized_type_pack_count<int, char> = 9;

template<int... Values>
int non_type_pack_sum = (0 + ... + Values);

template<typename T>
struct VariableTemplateBox {
    T value;
};

template<typename T>
int specialized_pointer = 1;

template<typename T>
int specialized_pointer<T*> = 2;

template<template<typename> class... Templates>
int template_template_pack_count = sizeof...(Templates);

template<template<typename> class Template>
int specialized_template_template = 1;

template<>
int specialized_template_template<VariableTemplateBox> = 2;

int main(void) {
    if (doubled<21> != 42) return 1;
    if (type_size<int> != 4) return 2;
    if (mutable_value<4> != 4) return 3;
    mutable_value<4> = 8;
    if (mutable_value<4> != 8) return 4;
    if (specialized_non_type<5> != 9) return 5;
    if (specialized_pointer<int> != 1) return 6;
    if (specialized_pointer<int*> != 2) return 7;
    if (specialized_template_template<VariableTemplateBox> != 2) return 8;
    if (specialized_type_pack_count<int> != 1) return 9;
    if (specialized_type_pack_count<int, char> != 9) return 10;
    if (type_pack_count<> != 0) return 11;
    if (type_pack_count<int, char, long> != 3) return 12;
    if (non_type_pack_sum<> != 0) return 13;
    if (non_type_pack_sum<2, 3, 4> != 9) return 14;
    if (template_template_pack_count<> != 0) return 15;
    if (template_template_pack_count<VariableTemplateBox> != 1) return 16;
    if (template_template_pack_count<VariableTemplateBox,
                                     VariableTemplateBox> != 2) return 17;
    return 0;
}
