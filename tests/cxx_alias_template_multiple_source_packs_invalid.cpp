template<int Left, int Right>
struct AliasValuePair {};

template<typename... Values>
struct AliasValueList {};

template<int... OwnerValues>
struct AliasValuePackOwner {
    template<int... AliasValues>
    using Zip = AliasValueList<
        AliasValuePair<OwnerValues * 2, AliasValues + 1>...>;
};

using InvalidZippedAliasValues =
    AliasValuePackOwner<1, 2>::Zip<4, 5, 6>;

InvalidZippedAliasValues invalid_values;

template<typename... Values>
struct ForwardedAliasPackList {};

template<typename Value>
struct ForwardedAliasPackOne {};

template<typename... OwnerTypes>
struct ForwardedAliasPackOwner {
    template<template<typename> class... Templates>
    using Zip = ForwardedAliasPackList<Templates<OwnerTypes>...>;
};

template<typename Owner, template<typename> class... Templates>
using ForwardedNestedAliasPack =
    typename Owner::template Zip<Templates...>;

using InvalidForwardedNestedAliasPack = ForwardedNestedAliasPack<
    ForwardedAliasPackOwner<int>, ForwardedAliasPackOne,
    ForwardedAliasPackOne>;

InvalidForwardedNestedAliasPack invalid_forwarded_nested_alias_pack;
