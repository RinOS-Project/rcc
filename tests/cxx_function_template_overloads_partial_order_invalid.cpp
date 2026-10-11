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

template<typename T>
int select_by_value_or_const_pointer_reference(T* value) {
    return value != nullptr;
}

template<typename T>
int select_by_value_or_const_pointer_reference(T* const& value) {
    return value != nullptr;
}

template<typename T>
int select_by_value_or_volatile_pointer_reference(T* value) {
    return value != nullptr;
}

template<typename T>
int select_by_value_or_volatile_pointer_reference(T* volatile& value) {
    return value != nullptr;
}

template<typename T> requires (sizeof(T) > 0)
int select_incomparable_constraints(T) {
    return 1;
}

template<typename T> requires (sizeof(T) >= 1)
int select_incomparable_constraints(T) {
    return 2;
}

template<typename Element, int Extent>
int select_crossed_array_pointer_extent(Element (*)[Extent],
                                        Element (*)[1]) {
    return 3;
}

template<typename Element, int Extent>
int select_crossed_array_pointer_extent(Element (*)[1],
                                        Element (*)[Extent]) {
    return 4;
}

int main() {
    int value = 3;
    int* pointer = &value;
    int array[1] = { value };
    return (int)select_template((short)1, (short)2) +
           select_incomparable_reference_template(1, value) +
           select_by_value_or_const_reference(value) +
           select_by_value_or_lvalue_reference(value) +
           select_by_value_or_rvalue_reference(3) +
           select_by_value_or_pointer_lvalue_reference(pointer) +
           select_by_value_or_const_pointer_reference(pointer) +
           select_by_value_or_volatile_pointer_reference(pointer) +
           select_incomparable_constraints(value) +
           select_crossed_array_pointer_extent(&array, &array);
}
