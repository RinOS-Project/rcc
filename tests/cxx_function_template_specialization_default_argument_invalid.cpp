template<typename T>
int specialization_default(T value = 1) {
    return static_cast<int>(value);
}

template<>
int specialization_default<int>(int value = 2) {
    return value;
}
