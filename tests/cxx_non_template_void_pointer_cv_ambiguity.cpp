int select_incomparable_void_pointer_cv(const void*) {
    return 1;
}

int select_incomparable_void_pointer_cv(volatile void*) {
    return 2;
}

int main() {
    int value = 0;
    return select_incomparable_void_pointer_cv(&value);
}
