template<typename T>
int select_by_value_or_pointer_rvalue_reference(T* value) {
    return value != nullptr;
}

template<typename T>
int select_by_value_or_pointer_rvalue_reference(T*&& value) {
    return value != nullptr;
}

template<typename T>
int select_by_value_or_const_pointer_rvalue_reference(T* value) {
    return value != nullptr;
}

template<typename T>
int select_by_value_or_const_pointer_rvalue_reference(T* const&& value) {
    return value != nullptr;
}

int main() {
    int value = 3;
    int* pointer = &value;
    (void)select_by_value_or_pointer_rvalue_reference(
        static_cast<int*&&>(pointer));
    (void)select_by_value_or_const_pointer_rvalue_reference(
        static_cast<int*&&>(pointer));
    return 0;
}
