int invalid_reference_init_capture(int* pointer) {
    return [&value = pointer]() {
        return *value;
    }();
}

int main() {
    return invalid_reference_init_capture(nullptr);
}
