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

int update_rvalue_reference_local(int&& value) {
    int&& local = static_cast<int&&>(value);
    local += 2;
    return value;
}

int& return_lvalue_reference(int& value) {
    return static_cast<int&>(value);
}

int&& return_rvalue_reference(int&& value) {
    return static_cast<int&&>(value);
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

template<typename T>
int read_array_element(T* value) {
    return value[1];
}

int main() {
    const int constant = 7;
    int mutable_value = 4;
    int values[2] = {5, 6};
    if (read_const_reference(constant) != 7) return 1;
    if (update_lvalue(mutable_value) != 7) return 2;
    if (mutable_value != 7) return 3;
    if (update_rvalue_reference_local(
            static_cast<int&&>(mutable_value)) != 9) return 4;
    if (mutable_value != 9) return 5;
    int& lvalue_result = return_lvalue_reference(mutable_value);
    lvalue_result += 1;
    if (mutable_value != 10) return 6;
    int&& rvalue_result = return_rvalue_reference(
            static_cast<int&&>(mutable_value));
    rvalue_result += 2;
    if (mutable_value != 12) return 7;
    if (read_rvalue(9) != 9) return 8;
    if (read_rvalue(mutable_value) != 12) return 9;
    if (invoke(double_value, 6) != 12) return 10;
    if (copy_from_const_pointer(&constant) != 8) return 11;
    if (default_type_copy(13) != 13) return 12;
    if (read_array_element(values) != 6) return 13;
    return 0;
}
