/* SPDX-License-Identifier: Apache-2.0 */

namespace std {
template<class Promise = void> struct coroutine_handle;

template<> struct coroutine_handle<void> {
    void* frame;
};

template<class Promise> struct coroutine_handle : coroutine_handle<void> {};

template<class Return, class... Args> struct coroutine_traits;
}

extern "C" {
void* rcc_coroutine_aggregate_return_value_last_frame;
int rcc_coroutine_aggregate_return_value_payload_low;
int rcc_coroutine_aggregate_return_value_payload_high;
int rcc_coroutine_aggregate_return_value_operation_count;
int rcc_coroutine_aggregate_return_value_payload_destroy_count;
int rcc_coroutine_aggregate_return_value_promise_destroy_count;
int rcc_coroutine_aggregate_return_value_lifetime_order_error;
void* rin_coro_handle_from_promise(void* promise);
}

struct rcc_coroutine_aggregate_return_value_payload {
    int low;
    int high;

    ~rcc_coroutine_aggregate_return_value_payload() noexcept {
        if (rcc_coroutine_aggregate_return_value_operation_count != 1) {
            ++rcc_coroutine_aggregate_return_value_lifetime_order_error;
        }
        ++rcc_coroutine_aggregate_return_value_payload_destroy_count;
    }
};

struct rcc_coroutine_aggregate_return_value_awaiter {
    bool await_ready() const noexcept { return false; }

    template<class Promise>
    void await_suspend(std::coroutine_handle<Promise>) const noexcept {}

    void await_resume() const noexcept {}
};

struct rcc_coroutine_aggregate_return_value_promise {
    int get_return_object() noexcept {
        rcc_coroutine_aggregate_return_value_last_frame =
            rin_coro_handle_from_promise(this);
        return 0;
    }

    rcc_coroutine_aggregate_return_value_awaiter initial_suspend() noexcept {
        return {};
    }

    rcc_coroutine_aggregate_return_value_awaiter final_suspend() noexcept {
        return {};
    }

    void return_value(
        const rcc_coroutine_aggregate_return_value_payload& payload) noexcept {
        rcc_coroutine_aggregate_return_value_payload_low = payload.low;
        rcc_coroutine_aggregate_return_value_payload_high = payload.high;
        ++rcc_coroutine_aggregate_return_value_operation_count;
    }

    void unhandled_exception() noexcept {}

    ~rcc_coroutine_aggregate_return_value_promise() noexcept {
        ++rcc_coroutine_aggregate_return_value_promise_destroy_count;
    }
};

namespace std {
template<class Return, class... Args>
struct coroutine_traits {
    using promise_type = rcc_coroutine_aggregate_return_value_promise;
};
}

extern "C" int rcc_coroutine_aggregate_return_value() {
    co_return rcc_coroutine_aggregate_return_value_payload{37, 91};
}
