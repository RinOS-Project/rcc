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

template<auto N>
struct ValueBox {
    int value;
};

template<template<auto M> class Container, int N>
struct DependentValueHolder {
    Container<N> value;

    int get() {
        return value.value;
    }
};

int main() {
    Holder<Box, int> holder;
    holder.value.value = 41;
    DefaultHolder<> default_holder;
    default_holder.value.value = 43;
    DependentValueHolder<ValueBox, 47> dependent_holder;
    dependent_holder.value.value = 47;
    return holder.get() == 41 && default_holder.get() == 43 &&
                   dependent_holder.get() == 47 ? 0 : 1;
}
