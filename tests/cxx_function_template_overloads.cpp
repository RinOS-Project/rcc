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

int main(void) {
    int value = 5;
    short priority = 0;
    return choose_template(&value) == 25 &&
                   choose_template(value) == 10 &&
                   forward_template(choose_template(&value)) == 25 &&
                   choose_conversion_before_partial_order(&value, priority) == 10
               ? 0
               : 1;
}
