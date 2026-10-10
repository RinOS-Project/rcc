template<typename T>
int select_template(T first, int second) {
    return (int)first + (int)second;
}

template<typename T>
long select_template(int first, T second) {
    return (long)first + (long)second + 100;
}

template<typename T>
int select_incomparable_reference_template(T marker, const int& value) {
    return (int)marker + value;
}

template<typename T>
int select_incomparable_reference_template(T marker, volatile int& value) {
    return (int)marker + value + 100;
}

template<typename T>
int select_by_value_or_const_reference(T value) {
    return (int)value;
}

template<typename T>
int select_by_value_or_const_reference(const T& value) {
    return (int)value + 100;
}

template<typename T>
int select_by_value_or_lvalue_reference(T value) {
    return (int)value;
}

template<typename T>
int select_by_value_or_lvalue_reference(T& value) {
    return (int)value + 200;
}

template<typename T>
int select_by_value_or_rvalue_reference(T value) {
    return (int)value;
}

template<typename T>
int select_by_value_or_rvalue_reference(T&& value) {
    return (int)value + 300;
}

template<typename T>
int select_by_value_or_pointer_lvalue_reference(T* value) {
    return value != nullptr;
}

template<typename T>
int select_by_value_or_pointer_lvalue_reference(T*& value) {
    return value != nullptr;
}

int main() {
    int value = 3;
    int* pointer = &value;
    return (int)select_template((short)1, (short)2) +
           select_incomparable_reference_template(1, value) +
           select_by_value_or_const_reference(value) +
           select_by_value_or_lvalue_reference(value) +
           select_by_value_or_rvalue_reference(3) +
           select_by_value_or_pointer_lvalue_reference(pointer);
}
