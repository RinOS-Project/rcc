namespace std {
template<class Promise = void> struct coroutine_handle;

template<> struct coroutine_handle<void> {
    void* frame;
};

template<class Promise> struct coroutine_handle : coroutine_handle<void> {};
}

int rcc_coroutine_final_suspend_should_suspend;

struct rcc_coroutine_initial_awaiter {
    bool await_ready() const noexcept { return true; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_coroutine_final_awaiter {
    bool await_ready() const noexcept { return false; }
    template<class Promise>
    bool await_suspend(std::coroutine_handle<Promise>) const noexcept {
        return rcc_coroutine_final_suspend_should_suspend != 0;
    }
    void await_resume() const noexcept {}
};

struct rcc_coroutine_final_bool_promise {
    void get_return_object() noexcept {}
    rcc_coroutine_initial_awaiter initial_suspend() noexcept { return {}; }
    rcc_coroutine_final_awaiter final_suspend() noexcept { return {}; }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
};

namespace std {
template<class Return, class... Args>
struct coroutine_traits {
    using promise_type = rcc_coroutine_final_bool_promise;
};
}

void rcc_coroutine_final_suspend_bool() {
    co_return;
}
