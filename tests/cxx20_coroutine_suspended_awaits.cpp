namespace std {
template<class Promise = void> struct coroutine_handle;

template<> struct coroutine_handle<void> {
    void* frame;
};

template<class Promise> struct coroutine_handle : coroutine_handle<void> {};
}

int rcc_coroutine_suspended_await_count;
int rcc_coroutine_suspended_await_result_count;
int rcc_coroutine_multi_await_sum;

struct rcc_coroutine_ready_awaiter {
    bool await_ready() const noexcept { return true; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_coroutine_manual_awaiter {
    bool await_ready() const noexcept { return false; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {
        ++rcc_coroutine_suspended_await_count;
    }
    int await_resume() const noexcept {
        ++rcc_coroutine_suspended_await_result_count;
        return rcc_coroutine_suspended_await_result_count;
    }
};

struct rcc_coroutine_suspended_promise {
    void get_return_object() noexcept {}
    rcc_coroutine_ready_awaiter initial_suspend() noexcept { return {}; }
    rcc_coroutine_ready_awaiter final_suspend() noexcept { return {}; }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
};

namespace std {
template<class Return, class... Args>
struct coroutine_traits {
    using promise_type = rcc_coroutine_suspended_promise;
};
}

int rcc_coroutine_await_identity(int value) noexcept {
    return value;
}

void rcc_coroutine_two_suspensions() {
    int resumed_result =
        rcc_coroutine_await_identity(
            ((short)(co_await rcc_coroutine_manual_awaiter{}) + 7) * 2);
    resumed_result = rcc_coroutine_await_identity(
        resumed_result + (int)(co_await rcc_coroutine_manual_awaiter{}));
    int retained_result = resumed_result;
    if ((co_await rcc_coroutine_manual_awaiter{}) - 1) {
        int resumed_condition_true = 1;
    } else {
        int resumed_condition_false = 2;
    }
    int nested_multi_await_result =
        (co_await rcc_coroutine_manual_awaiter{}) +
        ((co_await rcc_coroutine_manual_awaiter{}) -
         (co_await rcc_coroutine_manual_awaiter{}));
    rcc_coroutine_multi_await_sum = nested_multi_await_result;
    co_return;
}
