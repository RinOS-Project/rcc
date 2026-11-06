struct invalid_static_operator {
    static int operator co_await() {
        return 0;
    }
};

int operator co_await(int value) {
    return value;
}

int operator co_await(int left, int right) {
    return left + right;
}
