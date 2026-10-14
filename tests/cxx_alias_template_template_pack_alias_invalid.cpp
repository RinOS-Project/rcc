template<int Value>
struct IntegralBox {};

template<int Value>
using IntegralBoxAlias = IntegralBox<Value>;

template<typename... Types>
struct PackList {};

template<template<typename> class... Templates>
using ReboundPackList = PackList<Templates<int>...>;

using InvalidAliasPack = ReboundPackList<IntegralBoxAlias>;

InvalidAliasPack invalid_alias_pack;
