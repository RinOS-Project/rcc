struct ConversionBaseRoot {};
struct ConversionBaseMiddle : ConversionBaseRoot {};
struct ConversionBaseLeaf : ConversionBaseMiddle {};

template<typename T>
int choose_template(T) {
    return 10;
}

template<typename T>
int choose_template(T* value) {
    return *value + 20;
}

template<typename T>
T forward_template(T value) {
    return value;
}

template<typename T>
int choose_conversion_before_partial_order(T, int) {
    return 10;
}

template<typename T>
int choose_conversion_before_partial_order(T*, long) {
    return 20;
}

template<typename T>
int choose_cv_qualification(const T*) {
    return 30;
}

template<typename T>
int choose_cv_qualification(const volatile T*) {
    return 40;
}

template<typename T>
int choose_reference_binding(const T&) {
    return 51;
}

template<typename T>
int choose_reference_binding(const T&&) {
    return 52;
}

template<typename T>
int choose_pointer_bool(T*, bool) {
    return 61;
}

template<typename T>
int choose_pointer_bool(T*, void*) {
    return 62;
}

template<typename T>
int choose_pointer_subsequence(T*, void*) {
    return 71;
}

template<typename T>
int choose_pointer_subsequence(T*, const void*) {
    return 72;
}

template<typename T>
int choose_nearer_base(T*, ConversionBaseRoot*) {
    return 81;
}

template<typename T>
int choose_nearer_base(T*, ConversionBaseMiddle*) {
    return 82;
}

template<typename T>
int choose_base_over_void(T*, ConversionBaseRoot*) {
    return 83;
}

template<typename T>
int choose_base_over_void(T*, void*) {
    return 84;
}

int main(void) {
    int value = 5;
    int* pointer = &value;
    ConversionBaseLeaf leaf;
    ConversionBaseLeaf* leaf_pointer = &leaf;
    short priority = 0;
    return choose_template(&value) == 25 &&
                   choose_template(value) == 10 &&
                   forward_template(choose_template(&value)) == 25 &&
                   choose_conversion_before_partial_order(&value, priority) == 10 &&
                   choose_cv_qualification(pointer) == 30 &&
                   choose_reference_binding(value) == 51 &&
                   choose_reference_binding(0) == 52 &&
                   choose_pointer_bool(pointer, pointer) == 62 &&
                   choose_pointer_subsequence(pointer, pointer) == 71 &&
                   choose_nearer_base(leaf_pointer, leaf_pointer) == 82 &&
                   choose_base_over_void(leaf_pointer, leaf_pointer) == 83
               ? 0
               : 1;
}
