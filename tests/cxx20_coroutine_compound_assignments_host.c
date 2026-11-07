/* SPDX-License-Identifier: Apache-2.0 */

extern void* rcc_coroutine_compound_last_frame;
extern int rcc_coroutine_compound_results[10];
extern int rcc_coroutine_compound_lvalue_results[3];
extern float rcc_coroutine_compound_float_result;
extern int rcc_coroutine_compound_pointer_result;
extern long long rcc_coroutine_compound_wide_results[3];

extern void rcc_coroutine_compound_add(void);
extern void rcc_coroutine_compound_subtract(void);
extern void rcc_coroutine_compound_multiply(void);
extern void rcc_coroutine_compound_divide(void);
extern void rcc_coroutine_compound_modulo(void);
extern void rcc_coroutine_compound_and(void);
extern void rcc_coroutine_compound_or(void);
extern void rcc_coroutine_compound_xor(void);
extern void rcc_coroutine_compound_left_shift(void);
extern void rcc_coroutine_compound_right_shift(void);
extern void rcc_coroutine_compound_float_add(void);
extern void rcc_coroutine_compound_pointer_add(int* base);
extern void rcc_coroutine_compound_indexed_add(int* base, int index);
struct rcc_coroutine_compound_record {
    int value;
};
extern void rcc_coroutine_compound_member_xor(
    struct rcc_coroutine_compound_record* record);
extern void rcc_coroutine_compound_dereference_add(int* value);
extern void rcc_coroutine_compound_wide_add(void);
extern void rcc_coroutine_compound_wide_xor(void);
extern void rcc_coroutine_compound_wide_left_shift(void);
extern void rin_coro_resume(void* frame);
extern void rin_coro_destroy(void* frame);

#define CHECK(condition, failure_code) \
    do { if (!(condition)) return (failure_code); } while (0)

static int finish_one_coroutine(void)
{
    void* frame = rcc_coroutine_compound_last_frame;
    if (!frame) return 90;
    rin_coro_resume(frame);
    rin_coro_resume(frame);
    rin_coro_destroy(frame);
    rcc_coroutine_compound_last_frame = 0;
    return 0;
}

int main(void)
{
    int base[] = {5, 7, 11, 13, 17};
    int status;

#define RUN_INT(index, function, expected, failure_code) \
    do { \
        function(); \
        status = finish_one_coroutine(); \
        CHECK(status == 0, failure_code); \
        CHECK(rcc_coroutine_compound_results[index] == expected, \
              (failure_code) + 1); \
    } while (0)

    rcc_coroutine_compound_last_frame = 0;
    RUN_INT(0, rcc_coroutine_compound_add, 12, 11);
    RUN_INT(1, rcc_coroutine_compound_subtract, 8, 13);
    RUN_INT(2, rcc_coroutine_compound_multiply, 30, 15);
    RUN_INT(3, rcc_coroutine_compound_divide, 5, 17);
    RUN_INT(4, rcc_coroutine_compound_modulo, 2, 19);
    RUN_INT(5, rcc_coroutine_compound_and, 6, 21);
    RUN_INT(6, rcc_coroutine_compound_or, 11, 23);
    RUN_INT(7, rcc_coroutine_compound_xor, 9, 25);
    RUN_INT(8, rcc_coroutine_compound_left_shift, 12, 27);
    RUN_INT(9, rcc_coroutine_compound_right_shift, 4, 29);

    rcc_coroutine_compound_float_result = -1.0f;
    rcc_coroutine_compound_float_add();
    status = finish_one_coroutine();
    CHECK(status == 0, 31);
    CHECK(rcc_coroutine_compound_float_result == 3.75f, 32);

    rcc_coroutine_compound_pointer_result = -1;
    rcc_coroutine_compound_pointer_add(base);
    status = finish_one_coroutine();
    CHECK(status == 0, 33);
    CHECK(rcc_coroutine_compound_pointer_result == 3, 34);

    {
        int indexed[] = {5, 7, 11};
        struct rcc_coroutine_compound_record record = {12};
        int pointed_value = 5;
        rcc_coroutine_compound_indexed_add(indexed, 2);
        status = finish_one_coroutine();
        CHECK(status == 0, 41);
        CHECK(indexed[2] == 14 &&
                  rcc_coroutine_compound_lvalue_results[0] == 14, 42);

        rcc_coroutine_compound_member_xor(&record);
        status = finish_one_coroutine();
        CHECK(status == 0, 43);
        CHECK(record.value == 15 &&
                  rcc_coroutine_compound_lvalue_results[1] == 15, 44);

        rcc_coroutine_compound_dereference_add(&pointed_value);
        status = finish_one_coroutine();
        CHECK(status == 0, 45);
        CHECK(pointed_value == 9 &&
                  rcc_coroutine_compound_lvalue_results[2] == 9, 46);
    }

    rcc_coroutine_compound_wide_results[0] = -1;
    rcc_coroutine_compound_wide_add();
    status = finish_one_coroutine();
    CHECK(status == 0, 35);
    CHECK(rcc_coroutine_compound_wide_results[0] ==
              0x122436485a6c7e90ll, 36);

    rcc_coroutine_compound_wide_results[1] = -1;
    rcc_coroutine_compound_wide_xor();
    status = finish_one_coroutine();
    CHECK(status == 0, 37);
    CHECK(rcc_coroutine_compound_wide_results[1] ==
              0x1020304050607080ll, 38);

    rcc_coroutine_compound_wide_results[2] = -1;
    rcc_coroutine_compound_wide_left_shift();
    status = finish_one_coroutine();
    CHECK(status == 0, 39);
    CHECK(rcc_coroutine_compound_wide_results[2] == 56, 40);
    return 0;
}
