struct NonStructuralNestedValue {
private:
    int value;
};

template<template<NonStructuralNestedValue Value> class Inner>
struct NestedStructuralHolder {};
