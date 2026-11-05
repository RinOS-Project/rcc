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

template<typename T>
concept NestedSignatureHasF = requires(T value) { value.f(); };

template<typename T>
concept NestedSignatureHasFG = NestedSignatureHasF<T> &&
    requires(T value) { value.g(); };

template<template<NestedSignatureHasF T> class Template>
struct ConstrainedTemplateHolder {};

template<template<NestedSignatureHasFG T> class Template>
struct MoreConstrainedTemplateHolder {};

template<template<typename T> class Template>
struct UnconstrainedTemplateHolder {};

template<NestedSignatureHasF T>
struct ConstrainedTemplate {};

template<NestedSignatureHasFG T>
struct MoreConstrainedTemplate {};

template<NestedSignatureHasF T>
using ConstrainedAlias = T;

template<typename T>
struct UnconstrainedTemplate {};

template<template<NestedSignatureHasF T> class Template>
struct NestedConstrainedTemplate {};

template<template<NestedSignatureHasFG T> class Template>
struct NestedMoreConstrainedTemplate {};

template<template<template<NestedSignatureHasF T> class> class Template>
struct NestedConstrainedTemplateHolder {};

using AcceptedNestedSignature =
    NestedSignatureHolder<NestedSignatureOuter>;
using AcceptedNestedValueSignature =
    NestedValueHolder<NestedValueOuter>;
using AcceptedNestedPackSignature =
    NestedPackHolder<NestedPackOuter>;
using AcceptedNestedDependentValuePackSignature =
    NestedDependentValuePackHolder<NestedDependentValuePackOuter>;
using AcceptedEquivalentConstraint =
    ConstrainedTemplateHolder<ConstrainedTemplate>;
using AcceptedConstrainedAlias =
    ConstrainedTemplateHolder<ConstrainedAlias>;
using AcceptedUnconstrainedArgument =
    ConstrainedTemplateHolder<UnconstrainedTemplate>;
using AcceptedStrongerParameter =
    MoreConstrainedTemplateHolder<ConstrainedTemplate>;
using AcceptedIgnoredArgumentConstraint =
    UnconstrainedTemplateHolder<MoreConstrainedTemplate>;
using AcceptedNestedConstraint =
    NestedConstrainedTemplateHolder<NestedConstrainedTemplate>;
using AcceptedNestedStrongerConstraint =
    NestedConstrainedTemplateHolder<NestedMoreConstrainedTemplate>;

int main() {
    return sizeof(AcceptedNestedSignature) == 1 &&
                   sizeof(AcceptedNestedValueSignature) == 1 &&
                   sizeof(AcceptedNestedPackSignature) == 1 &&
                   sizeof(AcceptedNestedDependentValuePackSignature) == 1 &&
                   sizeof(AcceptedEquivalentConstraint) == 1 &&
                   sizeof(AcceptedConstrainedAlias) == 1 &&
                   sizeof(AcceptedUnconstrainedArgument) == 1 &&
                   sizeof(AcceptedStrongerParameter) == 1 &&
                   sizeof(AcceptedIgnoredArgumentConstraint) == 1 &&
                   sizeof(AcceptedNestedConstraint) == 1 &&
                   sizeof(AcceptedNestedStrongerConstraint) == 1
        ? 0 : 1;
}
