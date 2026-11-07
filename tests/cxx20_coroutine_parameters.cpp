namespace std {
template<class Promise = void> struct coroutine_handle;

template<> struct coroutine_handle<void> {
    void* frame;
};

template<class Promise> struct coroutine_handle : coroutine_handle<void> {};
}

int rcc_coroutine_parameter_suspend_total;

struct rcc_coroutine_parameter_ready_awaiter {
    bool await_ready() const noexcept { return true; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_coroutine_parameter_awaiter {
    int value;

    bool await_ready() const noexcept { return true; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {
        rcc_coroutine_parameter_suspend_total += value;
    }
    void await_resume() const noexcept {}
};

struct rcc_coroutine_parameter_promise {
    void get_return_object() noexcept {}
    rcc_coroutine_parameter_ready_awaiter initial_suspend() noexcept {
        return {};
    }
    rcc_coroutine_parameter_ready_awaiter final_suspend() noexcept {
        return {};
    }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
};

namespace std {
template<class Return, class... Args>
struct coroutine_traits {
    using promise_type = rcc_coroutine_parameter_promise;
};
}

void rcc_coroutine_scalar_parameters(int base, unsigned step) {
    int saved_base = base;
    co_await rcc_coroutine_parameter_awaiter{saved_base};
    unsigned resumed_step = step;
    co_await rcc_coroutine_parameter_awaiter{
        saved_base + (int)resumed_step};
    co_return;
}
