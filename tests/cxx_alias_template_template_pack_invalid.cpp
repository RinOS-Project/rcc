template<typename Value>
struct AliasTemplateOne {};

template<typename First, typename Second>
struct AliasTemplateTwo {};

template<typename... Values>
struct AliasTemplateList {};

template<template<typename> class... Templates>
using AliasTemplatePackList =
    AliasTemplateList<Templates<int>...>;

using InvalidAliasTemplatePack = AliasTemplatePackList<AliasTemplateTwo>;

InvalidAliasTemplatePack invalid_templates;
