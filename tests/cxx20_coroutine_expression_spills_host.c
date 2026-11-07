/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>

extern void* rcc_coroutine_expression_last_frame;
extern int rcc_coroutine_pointer_add_result;
extern int rcc_coroutine_pointer_difference_result;
extern int rcc_coroutine_indexed_result;
extern int rcc_coroutine_bitwise_result;
extern int rcc_coroutine_comparison_result;
extern int rcc_coroutine_conditional_result;
extern int rcc_coroutine_short_circuit_result;
extern long long rcc_coroutine_wide_arithmetic_result;
extern long long rcc_coroutine_wide_bitwise_result;
extern int rcc_coroutine_wide_comparison_result;

extern void rcc_coroutine_pointer_add(int* base);
extern void rcc_coroutine_pointer_difference(int* base);
extern void rcc_coroutine_indexed_lvalue(int* base);
extern void rcc_coroutine_bitwise_shift(void);
extern void rcc_coroutine_compare_awaited_values(void);
extern void rcc_coroutine_conditional_await_values(void);
extern void rcc_coroutine_short_circuit_await(void);
extern void rcc_coroutine_wide_addition(void);
extern void rcc_coroutine_wide_bitwise_shift(void);
extern void rcc_coroutine_wide_comparison(void);
extern void rin_coro_resume(void* frame);
extern void rin_coro_destroy(void* frame);

#define CHECK(condition, failure_code) \
    do { if (!(condition)) return (failure_code); } while (0)

static int resume_and_destroy_coroutine(int resume_count)
{
    void* frame = rcc_coroutine_expression_last_frame;
    if (!frame) return 90;
    for (int i = 0; i < resume_count; ++i) {
        rin_coro_resume(frame);
    }
    rin_coro_destroy(frame);
    rcc_coroutine_expression_last_frame = NULL;
    return 0;
}

static int finish_coroutine(int resume_count, int* result, int expected)
{
    int status = resume_and_destroy_coroutine(resume_count);
    if (status != 0) return status;
    return *result == expected ? 0 : 91;
}

int main(void)
{
    int values[] = {11, 13, 17, 19, 23, 29, 31, 37};
    int status;

    rcc_coroutine_expression_last_frame = NULL;
    rcc_coroutine_pointer_add_result = -1;
    rcc_coroutine_pointer_add(values);
    status = finish_coroutine(2, &rcc_coroutine_pointer_add_result, 3);
    CHECK(status == 0, 11 + status);

    rcc_coroutine_pointer_difference_result = -1;
    rcc_coroutine_pointer_difference(values);
    status = finish_coroutine(2, &rcc_coroutine_pointer_difference_result, 4);
    CHECK(status == 0, 20 + status);

    rcc_coroutine_indexed_result = -1;
    rcc_coroutine_indexed_lvalue(values);
    status = finish_coroutine(3, &rcc_coroutine_indexed_result, 19);
    CHECK(status == 0, 30 + status);

    rcc_coroutine_bitwise_result = -1;
    rcc_coroutine_bitwise_shift();
    status = finish_coroutine(2, &rcc_coroutine_bitwise_result, 28);
    CHECK(status == 0, 40 + status);

    rcc_coroutine_comparison_result = -1;
    rcc_coroutine_compare_awaited_values();
    status = finish_coroutine(2, &rcc_coroutine_comparison_result, 1);
    CHECK(status == 0, 50 + status);

    rcc_coroutine_conditional_result = -1;
    rcc_coroutine_conditional_await_values();
    status = finish_coroutine(2, &rcc_coroutine_conditional_result, 7);
    CHECK(status == 0, 60 + status);

    rcc_coroutine_short_circuit_result = -1;
    rcc_coroutine_short_circuit_await();
    status = finish_coroutine(1, &rcc_coroutine_short_circuit_result, 0);
    CHECK(status == 0, 70 + status);

    rcc_coroutine_wide_arithmetic_result = -1;
    rcc_coroutine_wide_addition();
    status = resume_and_destroy_coroutine(2);
    CHECK(status == 0, 80 + status);
    CHECK(rcc_coroutine_wide_arithmetic_result ==
              0x122436485a6c7e90ll, 81);

    rcc_coroutine_wide_bitwise_result = -1;
    rcc_coroutine_wide_bitwise_shift();
    status = resume_and_destroy_coroutine(2);
    CHECK(status == 0, 82 + status);
    CHECK(rcc_coroutine_wide_bitwise_result ==
              0x20406080a0c0e100ll, 83);

    rcc_coroutine_wide_comparison_result = -1;
    rcc_coroutine_wide_comparison();
    status = finish_coroutine(2, &rcc_coroutine_wide_comparison_result, 1);
    CHECK(status == 0, 84 + status);
    return 0;
}
