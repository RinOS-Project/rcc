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
void* rcc_coroutine_compound_last_frame;
int rcc_coroutine_compound_results[10];
int rcc_coroutine_compound_lvalue_results[3];
float rcc_coroutine_compound_float_result;
int rcc_coroutine_compound_pointer_result;
long long rcc_coroutine_compound_wide_results[3];
}

struct rcc_coroutine_compound_promise;

struct rcc_coroutine_compound_ready_awaiter {
    bool await_ready() const noexcept { return true; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    void await_resume() const noexcept {}
};

struct rcc_coroutine_compound_int_awaiter {
    int value;
    bool await_ready() const noexcept { return false; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    int await_resume() const noexcept { return value; }
};

struct rcc_coroutine_compound_float_awaiter {
    float value;
    bool await_ready() const noexcept { return false; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    float await_resume() const noexcept { return value; }
};

struct rcc_coroutine_compound_wide_awaiter {
    long long value;
    bool await_ready() const noexcept { return false; }
    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}
    long long await_resume() const noexcept { return value; }
};

struct rcc_coroutine_compound_promise {
    void get_return_object() noexcept {
        rcc_coroutine_compound_last_frame =
            rin_coro_handle_from_promise(this);
    }
    rcc_coroutine_compound_ready_awaiter initial_suspend() noexcept {
        return {};
    }
    rcc_coroutine_compound_ready_awaiter final_suspend() noexcept {
        return {};
    }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
};

struct rcc_coroutine_compound_record {
    int value;
};

namespace std {
template<class Return, class... Args>
struct coroutine_traits {
    using promise_type = rcc_coroutine_compound_promise;
};
}

extern "C" void rcc_coroutine_compound_add()
{
    int value = 10;
    value += co_await rcc_coroutine_compound_int_awaiter{2};
    rcc_coroutine_compound_results[0] = value;
}

extern "C" void rcc_coroutine_compound_subtract()
{
    int value = 10;
    value -= co_await rcc_coroutine_compound_int_awaiter{2};
    rcc_coroutine_compound_results[1] = value;
}

extern "C" void rcc_coroutine_compound_multiply()
{
    int value = 10;
    value *= co_await rcc_coroutine_compound_int_awaiter{3};
    rcc_coroutine_compound_results[2] = value;
}

extern "C" void rcc_coroutine_compound_divide()
{
    int value = 20;
    value /= co_await rcc_coroutine_compound_int_awaiter{4};
    rcc_coroutine_compound_results[3] = value;
}

extern "C" void rcc_coroutine_compound_modulo()
{
    int value = 20;
    value %= co_await rcc_coroutine_compound_int_awaiter{6};
    rcc_coroutine_compound_results[4] = value;
}

extern "C" void rcc_coroutine_compound_and()
{
    int value = 15;
    value &= co_await rcc_coroutine_compound_int_awaiter{6};
    rcc_coroutine_compound_results[5] = value;
}

extern "C" void rcc_coroutine_compound_or()
{
    int value = 8;
    value |= co_await rcc_coroutine_compound_int_awaiter{3};
    rcc_coroutine_compound_results[6] = value;
}

extern "C" void rcc_coroutine_compound_xor()
{
    int value = 15;
    value ^= co_await rcc_coroutine_compound_int_awaiter{6};
    rcc_coroutine_compound_results[7] = value;
}

extern "C" void rcc_coroutine_compound_left_shift()
{
    int value = 3;
    value <<= co_await rcc_coroutine_compound_int_awaiter{2};
    rcc_coroutine_compound_results[8] = value;
}

extern "C" void rcc_coroutine_compound_right_shift()
{
    int value = 16;
    value >>= co_await rcc_coroutine_compound_int_awaiter{2};
    rcc_coroutine_compound_results[9] = value;
}

extern "C" void rcc_coroutine_compound_float_add()
{
    float value = 1.5f;
    value += co_await rcc_coroutine_compound_float_awaiter{2.25f};
    rcc_coroutine_compound_float_result = value;
}

extern "C" void rcc_coroutine_compound_pointer_add(int* base)
{
    int* value = base + 1;
    value += co_await rcc_coroutine_compound_int_awaiter{2};
    rcc_coroutine_compound_pointer_result = (int)(value - base);
}

extern "C" void rcc_coroutine_compound_indexed_add(int* base, int index)
{
    base[index] += co_await rcc_coroutine_compound_int_awaiter{3};
    rcc_coroutine_compound_lvalue_results[0] = base[index];
}

extern "C" void rcc_coroutine_compound_member_xor(
    rcc_coroutine_compound_record* record)
{
    record->value ^= co_await rcc_coroutine_compound_int_awaiter{3};
    rcc_coroutine_compound_lvalue_results[1] = record->value;
}

extern "C" void rcc_coroutine_compound_dereference_add(int* value)
{
    *value += co_await rcc_coroutine_compound_int_awaiter{4};
    rcc_coroutine_compound_lvalue_results[2] = *value;
}

extern "C" void rcc_coroutine_compound_wide_add()
{
    long long value = 0x1122334455667788ll;
    value += co_await rcc_coroutine_compound_wide_awaiter{
        0x0102030405060708ll};
    rcc_coroutine_compound_wide_results[0] = value;
}

extern "C" void rcc_coroutine_compound_wide_xor()
{
    long long value = 0x1122334455667788ll;
    value ^= co_await rcc_coroutine_compound_wide_awaiter{
        0x0102030405060708ll};
    rcc_coroutine_compound_wide_results[1] = value;
}

extern "C" void rcc_coroutine_compound_wide_left_shift()
{
    long long value = 7;
    value <<= co_await rcc_coroutine_compound_int_awaiter{3};
    rcc_coroutine_compound_wide_results[2] = value;
}
