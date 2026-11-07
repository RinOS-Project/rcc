namespace std {
template<class Promise = void> struct coroutine_handle;
template<> struct coroutine_handle<void> {
    void* handle_;
};
template<class Promise> struct coroutine_handle : coroutine_handle<void> {
    using promise_type = Promise;
    Promise& promise() const;
};

struct rcc_test_awaiter {
    bool await_ready() const noexcept { return true; }
    template<class Promise>
    void await_suspend(coroutine_handle<Promise>) const noexcept {}
    int await_resume() const noexcept { return 1; }
};
}

struct rcc_test_promise {
    void get_return_object() noexcept {}
    std::rcc_test_awaiter initial_suspend() noexcept { return {}; }
    std::rcc_test_awaiter final_suspend() noexcept { return {}; }
    std::rcc_test_awaiter yield_value(int) noexcept { return {}; }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
};

namespace std {
template<class Return, class... Args> struct coroutine_traits {
    using promise_type = rcc_test_promise;
};
}

struct rcc_test_awaitable {};

std::rcc_test_awaiter operator co_await(rcc_test_awaitable) noexcept {
    return {};
}

void rcc_coroutine_sema_probe() {
    int value = co_await rcc_test_awaitable{};
    co_yield value;
    co_return;
}
