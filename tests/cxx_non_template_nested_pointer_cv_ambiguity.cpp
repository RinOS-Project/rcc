int select_incomparable_nested_pointer_cv(const int* const*) {
    return 1;
}

int select_incomparable_nested_pointer_cv(volatile int* const*) {
    return 2;
}

int main() {
    int value = 0;
    int* pointer = &value;
    int** source = &pointer;
    return select_incomparable_nested_pointer_cv(source);
}
