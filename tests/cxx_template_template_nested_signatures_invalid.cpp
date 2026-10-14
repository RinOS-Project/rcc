template<template<typename, typename> class Inner>
struct NestedSignatureOuterTwo {};

template<template<template<typename> class> class Outer>
struct NestedSignatureHolder {};

using InvalidNestedSignature =
    NestedSignatureHolder<NestedSignatureOuterTwo>;

InvalidNestedSignature invalid_nested_signature;
