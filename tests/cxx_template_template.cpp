template<typename U>
struct Box {
    U value;
};

template<template<typename> class Container, typename T>
struct Holder {
    Container<T> value;

    int get() {
        return value.value;
    }
};

int main() {
    Holder<Box, int> holder;
    holder.value.value = 41;
    return holder.get() == 41 ? 0 : 1;
}
