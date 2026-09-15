template<typename T>
int choose_template(T value) {
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

int main(void) {
    int value = 5;
    return choose_template(&value) == 25 &&
                   choose_template(value) == 10 &&
                   forward_template(choose_template(&value)) == 25
               ? 0
               : 1;
}
