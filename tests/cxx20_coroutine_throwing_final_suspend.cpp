/* SPDX-License-Identifier: Apache-2.0 */

namespace std {
template<class Promise = void> struct coroutine_handle;

template<> struct coroutine_handle<void> {
    void* frame;
};

template<class Promise> struct coroutine_handle : coroutine_handle<void> {};

template<class Return, class... Args> struct coroutine_traits;
}

struct rcc_final_suspend_initial_awaiter {
    bool await_ready() const noexcept { return true; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_final_suspend_potentially_throwing_awaiter {
    bool await_ready() const { return true; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_final_suspend_invalid_promise {
    void get_return_object() noexcept {}
    rcc_final_suspend_initial_awaiter initial_suspend() noexcept { return {}; }
    rcc_final_suspend_potentially_throwing_awaiter final_suspend() noexcept {
        return {};
    }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
};

namespace std {
template<class Return, class... Args>
struct coroutine_traits {
    using promise_type = rcc_final_suspend_invalid_promise;
};
}

void rcc_invalid_throwing_final_suspend_coroutine()
{
    co_return;
}
