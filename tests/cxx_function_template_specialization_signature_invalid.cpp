template<typename T>
int specialization_signature(T value) {
    return static_cast<int>(value);
}

template<>
long specialization_signature<int>(long value) {
    return value;
}
