template<typename T>
struct ClassTemplateAliasOwner {
    template<typename U>
    using OuterPointer = T*;

    template<typename U = T>
    using DefaultedPointer = U*;

    template<typename A0, typename A1, typename A2, typename A3,
             typename A4, typename A5, typename A6, typename A7,
             typename A8, typename A9, typename A10, typename A11,
             typename A12, typename A13, typename A14, typename A15,
             typename A16, typename A17, typename A18, typename A19,
             typename A20, typename A21, typename A22, typename A23,
             typename A24, typename A25, typename A26, typename A27,
             typename A28, typename A29, typename A30, typename A31,
             typename A32>
    using WideArguments = int;

    template<typename U>
    using InnerPointer = U*;

    template<int M>
    using Array = T[M];
};

template<int N>
struct IntegralTemplateAliasOwner {
    template<typename T>
    using Array = T[N];

    template<typename T, int M = N>
    using DefaultArray = T[M];
};

template<typename T>
struct ClassTemplateAliasWrapper {
    T value;
};

namespace ClassTemplateAliasNamespace {
template<typename T>
struct Owner {
    template<typename U>
    using Pointer = T*;
};
}

int main() {
    int outer_value = 13;
    long inner_value = 29;
    int namespace_value = 37;
    typename ClassTemplateAliasOwner<int>::template OuterPointer<long>
        outer_pointer = &outer_value;
    ClassTemplateAliasOwner<int>::DefaultedPointer<> defaulted_pointer =
        &outer_value;
    ClassTemplateAliasOwner<int>::WideArguments<
        int, int, int, int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int, int, int, int>
        wide_arguments = 43;
    ClassTemplateAliasOwner<int>::InnerPointer<long> inner_pointer =
        &inner_value;
    ClassTemplateAliasOwner<int>::Array<2> values = {5, 8};
    IntegralTemplateAliasOwner<3>::template Array<unsigned> integral_values =
        {2, 3, 5};
    IntegralTemplateAliasOwner<2>::DefaultArray<unsigned> default_values =
        {7, 11};
    ClassTemplateAliasWrapper<int> wrapped_value = {41};
    ClassTemplateAliasOwner<ClassTemplateAliasWrapper<int>>::OuterPointer<long>
        nested_owner_pointer = &wrapped_value;
    ClassTemplateAliasNamespace::Owner<int>::Pointer<long> namespace_pointer =
        &namespace_value;
    return *outer_pointer == 13 && *defaulted_pointer == 13 &&
                   *inner_pointer == 29 &&
                   wide_arguments == 43 &&
                   values[1] == 8 && integral_values[2] == 5 &&
                   default_values[1] == 11 &&
                   nested_owner_pointer->value == 41 &&
                   *namespace_pointer == 37
               ? 0 : 1;
}
