/* SPDX-License-Identifier: Apache-2.0 */

namespace std {
template<class Promise = void> struct coroutine_handle;

template<> struct coroutine_handle<void> {
    void* frame;
};

template<class Promise> struct coroutine_handle : coroutine_handle<void> {};

template<class Return, class... Args> struct coroutine_traits;
}

extern "C" void* rin_coro_handle_from_promise(void* promise);

extern "C" {
void* rcc_coroutine_expression_last_frame;
int rcc_coroutine_pointer_add_result;
int rcc_coroutine_pointer_difference_result;
int rcc_coroutine_indexed_result;
int rcc_coroutine_bitwise_result;
int rcc_coroutine_comparison_result;
int rcc_coroutine_conditional_result;
int rcc_coroutine_short_circuit_result;
long long rcc_coroutine_wide_arithmetic_result;
long long rcc_coroutine_wide_bitwise_result;
int rcc_coroutine_wide_comparison_result;
}

struct rcc_coroutine_expression_ready_awaiter {
    bool await_ready() const noexcept { return true; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_coroutine_expression_int_awaiter {
    int value;
    bool await_ready() const noexcept { return false; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    int await_resume() const noexcept { return value; }
};

struct rcc_coroutine_expression_pointer_awaiter {
    int* value;
    bool await_ready() const noexcept { return false; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    int* await_resume() const noexcept { return value; }
};

struct rcc_coroutine_expression_wide_awaiter {
    long long value;
    bool await_ready() const noexcept { return false; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    long long await_resume() const noexcept { return value; }
};

struct rcc_coroutine_expression_promise {
    void get_return_object() noexcept {
        rcc_coroutine_expression_last_frame =
            rin_coro_handle_from_promise(this);
    }
    rcc_coroutine_expression_ready_awaiter initial_suspend() noexcept {
        return {};
    }
    rcc_coroutine_expression_ready_awaiter final_suspend() noexcept {
        return {};
    }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
};

namespace std {
template<class Return, class... Args>
struct coroutine_traits {
    using promise_type = rcc_coroutine_expression_promise;
};
}

extern "C" void rcc_coroutine_pointer_add(int* base)
{
    int* result =
        (co_await rcc_coroutine_expression_pointer_awaiter{base + 1}) +
        (co_await rcc_coroutine_expression_int_awaiter{2});
    rcc_coroutine_pointer_add_result = (int)(result - base);
}

extern "C" void rcc_coroutine_pointer_difference(int* base)
{
    int difference =
        (co_await rcc_coroutine_expression_pointer_awaiter{base + 6}) -
        (co_await rcc_coroutine_expression_pointer_awaiter{base + 2});
    rcc_coroutine_pointer_difference_result = difference;
}

extern "C" void rcc_coroutine_indexed_lvalue(int* base)
{
    int result =
        (co_await rcc_coroutine_expression_pointer_awaiter{base})[
            (co_await rcc_coroutine_expression_int_awaiter{1}) +
            (co_await rcc_coroutine_expression_int_awaiter{2})];
    rcc_coroutine_indexed_result = result;
}

extern "C" void rcc_coroutine_bitwise_shift()
{
	int result =
        (co_await rcc_coroutine_expression_int_awaiter{7}) <<
        (co_await rcc_coroutine_expression_int_awaiter{2});
    rcc_coroutine_bitwise_result = result;
}

extern "C" void rcc_coroutine_compare_awaited_values()
{
    int result =
        (co_await rcc_coroutine_expression_int_awaiter{8}) >
        (co_await rcc_coroutine_expression_int_awaiter{3});
    rcc_coroutine_comparison_result = result;
}

extern "C" void rcc_coroutine_conditional_await_values()
{
    int result =
        (co_await rcc_coroutine_expression_int_awaiter{1})
            ? (co_await rcc_coroutine_expression_int_awaiter{7})
            : (co_await rcc_coroutine_expression_int_awaiter{9});
    rcc_coroutine_conditional_result = result;
}

extern "C" void rcc_coroutine_short_circuit_await()
{
    int result =
        (co_await rcc_coroutine_expression_int_awaiter{0}) &&
        (co_await rcc_coroutine_expression_int_awaiter{1});
    rcc_coroutine_short_circuit_result = result;
}

extern "C" void rcc_coroutine_wide_addition()
{
    long long result =
        (co_await rcc_coroutine_expression_wide_awaiter{
            0x1122334455667788ll}) +
        (co_await rcc_coroutine_expression_wide_awaiter{
            0x0102030405060708ll});
    rcc_coroutine_wide_arithmetic_result = result;
}

extern "C" void rcc_coroutine_wide_bitwise_shift()
{
    long long result =
        ((co_await rcc_coroutine_expression_wide_awaiter{
              0x1122334455667788ll}) ^
         (co_await rcc_coroutine_expression_wide_awaiter{
              0x0102030405060708ll})) << 1;
    rcc_coroutine_wide_bitwise_result = result;
}

extern "C" void rcc_coroutine_wide_comparison()
{
    int result =
        (co_await rcc_coroutine_expression_wide_awaiter{
            0x1122334455667788ll}) >
        (co_await rcc_coroutine_expression_wide_awaiter{
            0x0102030405060708ll});
    rcc_coroutine_wide_comparison_result = result;
}
