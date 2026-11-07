int rcc_coroutine_missing_promise(void) {
    co_await 1;
    co_yield 2;
}

int rcc_nested_co_await_missing_promise(void) {
    int result = co_await 4;
    return result;
}
