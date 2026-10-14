template<template<typename> class Inner>
struct NestedSignatureOuter {};

template<template<template<typename> class> class Outer>
struct NestedSignatureHolder {};

template<int Value>
struct NestedValueTemplate {};

template<template<int> class Inner>
struct NestedValueOuter {};

template<template<template<int> class> class Outer>
struct NestedValueHolder {};

template<typename... Values>
struct NestedPackTemplate {};

template<template<typename...> class Inner>
struct NestedPackOuter {};

template<template<template<typename...> class> class Outer>
struct NestedPackHolder {};

template<typename T, T... Values>
struct NestedDependentValuePackTemplate {};

template<template<typename T, T... Values> class Inner>
struct NestedDependentValuePackOuter {};

template<template<template<typename T, T... Values> class> class Outer>
struct NestedDependentValuePackHolder {};

using AcceptedNestedSignature =
    NestedSignatureHolder<NestedSignatureOuter>;
using AcceptedNestedValueSignature =
    NestedValueHolder<NestedValueOuter>;
using AcceptedNestedPackSignature =
    NestedPackHolder<NestedPackOuter>;
using AcceptedNestedDependentValuePackSignature =
    NestedDependentValuePackHolder<NestedDependentValuePackOuter>;

int main() {
    return sizeof(AcceptedNestedSignature) == 1 &&
                   sizeof(AcceptedNestedValueSignature) == 1 &&
                   sizeof(AcceptedNestedPackSignature) == 1 &&
                   sizeof(AcceptedNestedDependentValuePackSignature) == 1
        ? 0 : 1;
}
