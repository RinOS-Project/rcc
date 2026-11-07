namespace std {
template<class Promise = void> struct coroutine_handle;
template<> struct coroutine_handle<void> {
    void* frame;
};
template<class Promise> struct coroutine_handle : coroutine_handle<void> {};

template<class Return, class... Args> struct coroutine_traits;
}

using size_type = __SIZE_TYPE__;

struct rcc_coroutine_aggregate_return_object {
    int low;
    int high;
};

int rcc_coroutine_aggregate_promise_destroy_count;

struct rcc_coroutine_aggregate_awaiter {
    bool await_ready() const noexcept { return true; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_coroutine_aggregate_promise {
    static void* operator new(size_type size) noexcept;
    static rcc_coroutine_aggregate_return_object
    get_return_object_on_allocation_failure() noexcept {
        return {-1, -2};
    }
    rcc_coroutine_aggregate_return_object get_return_object() noexcept {
        return {23, 29};
    }
    rcc_coroutine_aggregate_awaiter initial_suspend() noexcept {
        return {};
    }
    rcc_coroutine_aggregate_awaiter final_suspend() noexcept {
        return {};
    }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
    ~rcc_coroutine_aggregate_promise() noexcept {
        ++rcc_coroutine_aggregate_promise_destroy_count;
    }
};

namespace std {
template<class Return, class... Args>
struct coroutine_traits {
    using promise_type = rcc_coroutine_aggregate_promise;
};
}

rcc_coroutine_aggregate_return_object
rcc_coroutine_aggregate_nonvoid_return_object() {
    co_return;
}
