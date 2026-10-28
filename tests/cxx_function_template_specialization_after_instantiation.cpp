template<typename T>
int specialized_too_late(T value) {
    return static_cast<int>(value);
}

int instantiate_before_specialization() {
    return specialized_too_late(1);
}

template<>
int specialized_too_late<int>(int value) {
    return value + 1;
}
