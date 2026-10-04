namespace broken {
int malformed() noexcept unexpected tokens;
int declaration_after_error;
}

int valid_after_namespace(int value) {
    return value + 1;
}

int malformed_missing_semicolon() {
    int bad = 1
    bool after_missing = ;
    return after_missing ? bad : 0;
}
