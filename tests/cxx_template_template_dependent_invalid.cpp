template<auto N>
struct ValueBox {
    int value;
};

template<template<auto M> class Container, typename T>
struct InvalidDependentValueHolder {
    Container<unknown_value> value;
};

InvalidDependentValueHolder<ValueBox, int> invalid_dependent;

template<unsigned N>
struct UnsignedValueBox {
    int value;
};

template<template<int M> class Container, int N>
struct InvalidSignatureHolder {
    Container<N> value;
};

InvalidSignatureHolder<UnsignedValueBox, 7> invalid_signature;

template<template<typename> class... Templates>
struct TemplatePackTarget {};

template<template<typename> class... Templates>
struct InvalidUnexpandedTemplatePack
    : TemplatePackTarget<Templates> {};

template<template<typename> class Template>
struct InvalidNonPackTemplateExpansion
    : TemplatePackTarget<Template...> {};

int main() {
    return invalid_dependent.value.value;
}
