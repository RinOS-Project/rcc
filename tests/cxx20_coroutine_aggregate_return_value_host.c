/* SPDX-License-Identifier: Apache-2.0 */

extern void* rcc_coroutine_aggregate_return_value_last_frame;
extern int rcc_coroutine_aggregate_return_value_payload_low;
extern int rcc_coroutine_aggregate_return_value_payload_high;
extern int rcc_coroutine_aggregate_return_value_operation_count;
extern int rcc_coroutine_aggregate_return_value_payload_destroy_count;
extern int rcc_coroutine_aggregate_return_value_promise_destroy_count;
extern int rcc_coroutine_aggregate_return_value_lifetime_order_error;
extern int rcc_coroutine_suspended_return_value_payload;
extern int rcc_coroutine_suspended_return_value_operation_count;
extern int rcc_coroutine_awaited_aggregate_return_value_low;
extern int rcc_coroutine_awaited_aggregate_return_value_high;
extern int rcc_coroutine_awaited_aggregate_return_value_operation_count;

extern int rcc_coroutine_aggregate_return_value(void);
extern int rcc_coroutine_suspended_return_value(void);
extern int rcc_coroutine_awaited_aggregate_return_value(void);
extern void rin_coro_resume(void* frame);
extern void rin_coro_destroy(void* frame);

int main(void)
{
    if (rcc_coroutine_aggregate_return_value() != 0 ||
        !rcc_coroutine_aggregate_return_value_last_frame ||
        rcc_coroutine_aggregate_return_value_operation_count != 0 ||
        rcc_coroutine_aggregate_return_value_payload_destroy_count != 0) {
        return 11;
    }

    rin_coro_resume(rcc_coroutine_aggregate_return_value_last_frame);
    if (rcc_coroutine_aggregate_return_value_operation_count != 1 ||
        rcc_coroutine_aggregate_return_value_payload_low != 37 ||
        rcc_coroutine_aggregate_return_value_payload_high != 91 ||
        rcc_coroutine_aggregate_return_value_payload_destroy_count != 1 ||
        rcc_coroutine_aggregate_return_value_lifetime_order_error != 0) {
        return 12;
    }

    rin_coro_destroy(rcc_coroutine_aggregate_return_value_last_frame);
    rcc_coroutine_aggregate_return_value_last_frame = 0;
    if (rcc_coroutine_aggregate_return_value_promise_destroy_count != 1) {
        return 13;
    }

    if (rcc_coroutine_suspended_return_value() != 0 ||
        !rcc_coroutine_aggregate_return_value_last_frame ||
        rcc_coroutine_suspended_return_value_operation_count != 0) {
        return 21;
    }
    rin_coro_resume(rcc_coroutine_aggregate_return_value_last_frame);
    if (rcc_coroutine_suspended_return_value_operation_count != 0) {
        return 22;
    }
    rin_coro_resume(rcc_coroutine_aggregate_return_value_last_frame);
    if (rcc_coroutine_suspended_return_value_operation_count != 1 ||
        rcc_coroutine_suspended_return_value_payload != 113) {
        return 24;
    }
    rin_coro_destroy(rcc_coroutine_aggregate_return_value_last_frame);
    rcc_coroutine_aggregate_return_value_last_frame = 0;
    if (rcc_coroutine_aggregate_return_value_promise_destroy_count != 2) {
        return 23;
    }

    if (rcc_coroutine_awaited_aggregate_return_value() != 0 ||
        !rcc_coroutine_aggregate_return_value_last_frame ||
        rcc_coroutine_awaited_aggregate_return_value_operation_count != 0) {
        return 31;
    }
    rin_coro_resume(rcc_coroutine_aggregate_return_value_last_frame);
    if (rcc_coroutine_awaited_aggregate_return_value_operation_count != 0) {
        return 32;
    }
    rin_coro_resume(rcc_coroutine_aggregate_return_value_last_frame);
    if (rcc_coroutine_awaited_aggregate_return_value_operation_count != 1 ||
        rcc_coroutine_awaited_aggregate_return_value_low != 47 ||
        rcc_coroutine_awaited_aggregate_return_value_high != 83) {
        return 33;
    }
    rin_coro_destroy(rcc_coroutine_aggregate_return_value_last_frame);
    rcc_coroutine_aggregate_return_value_last_frame = 0;
    if (rcc_coroutine_aggregate_return_value_promise_destroy_count != 3) {
        return 34;
    }
    return 0;
}
