template<template<template<typename, typename> class> class Outer>
struct NestedTwoParameterHolder {};

template<template<typename> class Inner>
struct NestedOneParameterOuter {};

NestedTwoParameterHolder<NestedOneParameterOuter> invalid_nested_default;
