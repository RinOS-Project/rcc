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

template<template<typename> class Container = Box, typename T = int>
struct DefaultHolder {
    Container<T> value;

    int get() {
        return value.value;
    }
};

int main() {
    Holder<Box, int> holder;
    holder.value.value = 41;
    DefaultHolder<> default_holder;
    default_holder.value.value = 43;
    return holder.get() == 41 && default_holder.get() == 43 ? 0 : 1;
}
