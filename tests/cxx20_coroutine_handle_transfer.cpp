namespace std {
template<class Promise = void> struct coroutine_handle;

template<> struct coroutine_handle<void> {
    void* frame;
};

template<class Promise> struct coroutine_handle : coroutine_handle<void> {};
}

static_assert(sizeof(std::coroutine_handle<void>) == sizeof(void*));

struct rcc_coroutine_handle_ready_awaiter {
    bool await_ready() const noexcept { return true; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_coroutine_handle_transfer_awaiter {
    bool await_ready() const noexcept { return false; }
    template<class Promise>
    std::coroutine_handle<Promise> await_suspend(
        std::coroutine_handle<Promise>) const noexcept;
    void await_resume() const noexcept {}
};

struct rcc_coroutine_handle_transfer_promise {
    void get_return_object() noexcept {}
    rcc_coroutine_handle_ready_awaiter initial_suspend() noexcept {
        return {};
    }
    rcc_coroutine_handle_ready_awaiter final_suspend() noexcept {
        return {};
    }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
};

namespace std {
template<class Return, class... Args>
struct coroutine_traits {
    using promise_type = rcc_coroutine_handle_transfer_promise;
};
}

void rcc_coroutine_handle_transfer() {
    co_await rcc_coroutine_handle_transfer_awaiter{};
    co_return;
}
