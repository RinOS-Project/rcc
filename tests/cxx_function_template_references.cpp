template<typename T>
int read_const_reference(const T& value) {
    return value;
}

template<typename T>
int update_lvalue(T& value) {
    value += 3;
    return value;
}

template<typename T>
int read_rvalue(T&& value) {
    return value;
}

template<typename T>
T copy_from_const_pointer(const T* value) {
    T copy = *value;
    copy += 1;
    return copy;
}

int double_value(int value) {
    return value + value;
}

template<typename R, typename A>
R invoke(R (*function)(A), A value) {
    return function(value);
}

template<typename T, typename U = T>
U default_type_copy(T value) {
    U result{};
    result = value;
    return result;
}

int main() {
    const int constant = 7;
    int mutable_value = 4;
    return read_const_reference(constant) == 7 &&
                   update_lvalue(mutable_value) == 7 &&
                   mutable_value == 7 &&
                   read_rvalue(9) == 9 &&
                   invoke(double_value, 6) == 12 &&
                   copy_from_const_pointer(&constant) == 8 &&
                   default_type_copy(13) == 13 ? 0 : 1;
}
