template<auto N>
struct ValueBox {
    int value;
};

template<template<auto M> class Container, typename T>
struct InvalidDependentValueHolder {
    Container<unknown_value> value;
};

InvalidDependentValueHolder<ValueBox, int> invalid_dependent;

int main() {
    return invalid_dependent.value.value;
}
