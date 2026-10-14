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

template<template<typename> class Container, typename T>
int function_template_accepts(Container<T>*) {
    return 0;
}

int main() {
    Holder<PairBox, int> invalid;
    PairBox<int, int> pair_box{};
    return invalid.value.value +
           function_template_accepts<PairBox, int>(&pair_box);
}
