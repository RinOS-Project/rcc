template<typename T>
concept HasF = requires(T value) { value.f(); };

template<typename T>
concept HasFG = HasF<T> && requires(T value) { value.g(); };

template<template<HasF T> class Template>
struct ConstrainedTemplateHolder {};

template<HasFG T>
struct MoreConstrainedTemplate {};

ConstrainedTemplateHolder<MoreConstrainedTemplate> invalid;
