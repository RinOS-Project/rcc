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

template<int N>
struct IntValueBox {
    int value;
};

template<template<int M> class Container, int N>
struct DependentIntValueHolder {
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
    DependentIntValueHolder<IntValueBox, 53> dependent_int_holder;
    dependent_int_holder.value.value = 53;
    return holder.get() == 41 && default_holder.get() == 43 &&
                   dependent_holder.get() == 47 &&
                   dependent_int_holder.get() == 53 ? 0 : 1;
}
