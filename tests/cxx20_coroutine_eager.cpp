namespace std {
template<class Promise = void> struct coroutine_handle;

template<> struct coroutine_handle<void> {
    void* frame;
};

template<class Promise> struct coroutine_handle : coroutine_handle<void> {};
}

int rcc_coroutine_return_count;

struct rcc_eager_awaiter {
    bool await_ready() const noexcept { return true; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_eager_promise {
    void get_return_object() noexcept {}
    rcc_eager_awaiter initial_suspend() noexcept { return {}; }
    rcc_eager_awaiter final_suspend() noexcept { return {}; }
    void return_void() noexcept { ++rcc_coroutine_return_count; }
    void unhandled_exception() noexcept {}
};

static_assert(sizeof(std::coroutine_handle<rcc_eager_promise>) ==
              sizeof(void*));

namespace std {
template<class Return, class... Args>
struct coroutine_traits {
    using promise_type = rcc_eager_promise;
};
}

void rcc_eager_void_coroutine() {
    co_return;
}
