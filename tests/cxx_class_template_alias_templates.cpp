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

template<typename T, typename Tag>
struct PartialAliasOwner {
    template<typename U>
    using Pointer = T*;
};

template<typename T>
struct PartialAliasOwner<T, int> {
    template<typename U>
    using Pointer = T*;

    template<int N>
    using Array = T[N];
};

template<typename T>
struct ExplicitAliasOwner {
    template<typename U>
    using Pointer = T*;
};

template<>
struct ExplicitAliasOwner<int> {
    template<typename U>
    using Pointer = U*;
};

template<typename T>
typename ClassTemplateAliasOwner<T>::template OuterPointer<long>
dependent_alias_owner(T* value) {
    return value;
}

struct AliasTemplatePublicBase {
    template<typename U>
    using Pointer = U*;
};

struct AliasTemplatePublicMiddle : public AliasTemplatePublicBase {};

struct AliasTemplatePublicDerived : public AliasTemplatePublicMiddle {};

struct AliasTemplateProtectedBase {
protected:
    template<typename U>
    using Pointer = U*;
};

struct AliasTemplateProtectedDerived : public AliasTemplateProtectedBase {
    static int read(int* value) {
        Pointer<int> pointer = value;
        return *pointer;
    }
};

struct AliasTemplatePrivateBase {
    template<typename U>
    using Pointer = U*;
};

struct AliasTemplatePrivateDerived : private AliasTemplatePrivateBase {
    static int read(int* value) {
        Pointer<int> pointer = value;
        return *pointer;
    }
};

template<typename T>
struct AliasTemplateGenericBase {
    template<typename U>
    using Pointer = T*;
};

template<typename T>
struct AliasTemplateGenericDerived : public AliasTemplateGenericBase<T> {};

int main() {
    int outer_value = 13;
    long inner_value = 29;
    long partial_value = 47;
    long explicit_value = 53;
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
    PartialAliasOwner<long, int>::Pointer<char> partial_pointer =
        &partial_value;
    PartialAliasOwner<long, int>::Array<2> partial_values = {17, 19};
    ExplicitAliasOwner<int>::Pointer<long> explicit_pointer = &explicit_value;
    int dependent_value = 59;
    int* dependent_pointer = dependent_alias_owner(&dependent_value);
    int inherited_value = 61;
    AliasTemplatePublicDerived::Pointer<int> inherited_pointer =
        &inherited_value;
    AliasTemplateGenericDerived<int>::Pointer<long> generic_inherited_pointer =
        &inherited_value;
    return *outer_pointer == 13 && *defaulted_pointer == 13 &&
                   *inner_pointer == 29 &&
                   wide_arguments == 43 &&
                   values[1] == 8 && integral_values[2] == 5 &&
                   default_values[1] == 11 &&
                   nested_owner_pointer->value == 41 &&
                   *namespace_pointer == 37 && *partial_pointer == 47 &&
                   partial_values[1] == 19 && *explicit_pointer == 53 &&
                   *dependent_pointer == 59 && *inherited_pointer == 61 &&
                   *generic_inherited_pointer == 61 &&
                   AliasTemplateProtectedDerived::read(&inherited_value) == 61 &&
                   AliasTemplatePrivateDerived::read(&inherited_value) == 61
               ? 0 : 1;
}
