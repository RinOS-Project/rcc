template<typename Value>
struct AliasTemplateOne {};

template<typename First, typename Second>
struct AliasTemplateTwo {};

template<typename... Values>
struct AliasTemplateList {};

template<template<typename> class... Templates>
using AliasTemplatePackList =
    AliasTemplateList<Templates<int>...>;

template<template<typename, typename> class... Templates>
using InvalidDependentAliasTemplatePack =
    AliasTemplatePackList<Templates...>;

using InvalidAliasTemplatePack =
    InvalidDependentAliasTemplatePack<AliasTemplateTwo>;

InvalidAliasTemplatePack invalid_templates;
