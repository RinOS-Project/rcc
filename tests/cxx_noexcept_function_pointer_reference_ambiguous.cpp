void noexcept_function() noexcept {}

int select_noexcept_function(void (*)() noexcept) {
    return 1;
}

int select_noexcept_function(void (&)()) {
    return 2;
}

int main() {
    return select_noexcept_function(noexcept_function);
}
