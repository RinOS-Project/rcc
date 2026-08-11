namespace broken {
int malformed() noexcept unexpected tokens;
int declaration_after_error;
}

int valid_after_namespace(int value) {
    return value + 1;
}
