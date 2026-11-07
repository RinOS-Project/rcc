int rcc_unsupported_coroutines(void) {
    co_await 1;
    co_yield 2;
}

int rcc_unsupported_nested_co_await(void) {
    int result = co_await 4;
    return result;
}
