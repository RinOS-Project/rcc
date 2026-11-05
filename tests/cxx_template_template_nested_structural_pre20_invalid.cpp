struct StructuralNestedValue {
    int value;
};

template<template<StructuralNestedValue Value> class Inner>
struct NestedStructuralHolder {};
