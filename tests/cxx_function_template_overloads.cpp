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

int main(void) {
    int value = 5;
    int* pointer = &value;
    short priority = 0;
    return choose_template(&value) == 25 &&
                   choose_template(value) == 10 &&
                   forward_template(choose_template(&value)) == 25 &&
                   choose_conversion_before_partial_order(&value, priority) == 10 &&
                   choose_cv_qualification(pointer) == 30
               ? 0
               : 1;
}
