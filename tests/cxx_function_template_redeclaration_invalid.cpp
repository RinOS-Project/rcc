template<typename T>
int duplicate_template_definition(T value) {
    return static_cast<int>(value);
}

template<typename U>
int duplicate_template_definition(U value) {
    return static_cast<int>(value) + 1;
}
