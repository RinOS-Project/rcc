struct co_await_probe {
    int value;

    co_await_probe operator co_await() const {
        return *this;
    }
};

co_await_probe operator co_await(co_await_probe value) {
    return value;
}
