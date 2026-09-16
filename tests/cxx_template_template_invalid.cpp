template<typename U>
struct Box {
    U value;
};

template<typename U, typename V>
struct PairBox {
    U value;
    V other;
};

template<template<typename> class Container, typename T>
struct Holder {
    Container<T> value;
};

int main() {
    Holder<PairBox, int> invalid;
    return invalid.value.value;
}
