int pointed_default(int value = 9);

int call_pointer_without_argument() {
    int (*function)(int) = pointed_default;
    return function();
}
