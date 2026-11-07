namespace std {
template<class Promise = void> struct coroutine_handle;

template<> struct coroutine_handle<void> {
    void* frame;
};

template<class Promise> struct coroutine_handle : coroutine_handle<void> {};

template<class Return, class... Args> struct coroutine_traits;
}

int rcc_coroutine_return_value_payload;

struct rcc_coroutine_return_value_awaiter {
    bool await_ready() const noexcept { return true; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_coroutine_return_value_promise {
    int get_return_object() noexcept { return 47; }
    rcc_coroutine_return_value_awaiter initial_suspend() noexcept {
        return {};
    }
    rcc_coroutine_return_value_awaiter final_suspend() noexcept {
        return {};
    }
    void return_value(int value) noexcept {
        rcc_coroutine_return_value_payload = value;
    }
    void unhandled_exception() noexcept {}
};

namespace std {
template<class Return, class... Args>
struct coroutine_traits {
    using promise_type = rcc_coroutine_return_value_promise;
};
}

int rcc_coroutine_return_value_payload_test() {
    co_return 73;
}
