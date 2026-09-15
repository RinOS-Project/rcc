auto inconsistent_auto_return(int value) {
    if (value) return 1;
    return 2L;
}

auto missing_auto_return_definition(int value);
