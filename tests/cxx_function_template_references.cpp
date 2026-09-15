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

int double_value(int value) {
    return value + value;
}

template<typename R, typename A>
R invoke(R (*function)(A), A value) {
    return function(value);
}

int main() {
    const int constant = 7;
    int mutable_value = 4;
    return read_const_reference(constant) == 7 &&
                   update_lvalue(mutable_value) == 7 &&
                   mutable_value == 7 &&
                   read_rvalue(9) == 9 &&
                   invoke(double_value, 6) == 12 ? 0 : 1;
}
