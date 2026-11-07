namespace std {
template<class Promise = void> struct coroutine_handle;
template<> struct coroutine_handle<void> {
    void* frame;
};
template<class Promise> struct coroutine_handle : coroutine_handle<void> {};

template<class Return, class... Args> struct coroutine_traits;
}

int rcc_coroutine_local_destroy_order;

struct rcc_coroutine_local_ready_awaiter {
    bool await_ready() const noexcept { return true; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_coroutine_local_suspending_awaiter {
    bool await_ready() const noexcept { return false; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_coroutine_local_promise {
    void get_return_object() noexcept {}
    rcc_coroutine_local_ready_awaiter initial_suspend() noexcept {
        return {};
    }
    rcc_coroutine_local_ready_awaiter final_suspend() noexcept {
        return {};
    }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
};

namespace std {
template<class Return, class... Args>
struct coroutine_traits {
    using promise_type = rcc_coroutine_local_promise;
};
}

struct rcc_coroutine_tracked_local {
    int value;
    ~rcc_coroutine_tracked_local() noexcept {
        rcc_coroutine_local_destroy_order =
            rcc_coroutine_local_destroy_order * 10 + value;
    }
};

void rcc_coroutine_local_lifetimes() {
    rcc_coroutine_tracked_local first{1};
    co_await rcc_coroutine_local_suspending_awaiter{};
    rcc_coroutine_tracked_local second{2};
    co_await rcc_coroutine_local_suspending_awaiter{};
    co_return;
}
