namespace std {
template<class Promise = void> struct coroutine_handle;
template<> struct coroutine_handle<void> {
    void* frame;
};
template<class Promise> struct coroutine_handle : coroutine_handle<void> {};

template<class Return, class... Args> struct coroutine_traits;
}

using size_type = __SIZE_TYPE__;

int rcc_coroutine_return_object_promise_destroy_count;

struct rcc_coroutine_return_object_awaiter {
    bool await_ready() const noexcept { return true; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_coroutine_return_object_promise {
    static void* operator new(size_type size) noexcept;
    static int get_return_object_on_allocation_failure() noexcept {
        return -1;
    }
    int get_return_object() noexcept { return 47; }
    rcc_coroutine_return_object_awaiter initial_suspend() noexcept {
        return {};
    }
    rcc_coroutine_return_object_awaiter final_suspend() noexcept {
        return {};
    }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
    ~rcc_coroutine_return_object_promise() noexcept {
        ++rcc_coroutine_return_object_promise_destroy_count;
    }
};

namespace std {
template<class Return, class... Args>
struct coroutine_traits {
    using promise_type = rcc_coroutine_return_object_promise;
};
}

int rcc_coroutine_nonvoid_return_object() {
    co_return;
}
