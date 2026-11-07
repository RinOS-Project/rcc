namespace std {
template<class Promise = void> struct coroutine_handle;
template<> struct coroutine_handle<void> {
    void* frame;
};
template<class Promise> struct coroutine_handle : coroutine_handle<void> {};

template<class Return, class... Args> struct coroutine_traits;
}

int rcc_coroutine_awaiter_destroy_count;

struct rcc_coroutine_awaiter_ready {
    bool await_ready() const noexcept { return true; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_coroutine_nontrivial_awaiter {
    bool await_ready() const noexcept { return false; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    int await_resume() const noexcept { return 7; }
    ~rcc_coroutine_nontrivial_awaiter() noexcept {
        ++rcc_coroutine_awaiter_destroy_count;
    }
};

struct rcc_coroutine_awaiter_lifetime_promise {
    void get_return_object() noexcept {}
    rcc_coroutine_awaiter_ready initial_suspend() noexcept { return {}; }
    rcc_coroutine_awaiter_ready final_suspend() noexcept { return {}; }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
};

namespace std {
template<class Return, class... Args>
struct coroutine_traits {
    using promise_type = rcc_coroutine_awaiter_lifetime_promise;
};
}

void rcc_coroutine_nontrivial_awaiter_lifetime() {
    co_await rcc_coroutine_nontrivial_awaiter{};
    co_return;
}
