template<typename T>
int specialized_twice(T value) {
    return static_cast<int>(value);
}

template<>
int specialized_twice<int>(int value) {
    return value + 1;
}

template<>
int specialized_twice<int>(int value) {
    return value + 2;
}
