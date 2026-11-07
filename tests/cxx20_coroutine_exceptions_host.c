/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <string.h>

extern int rcc_coroutine_exception_unhandled_count;
extern int rcc_coroutine_exception_caught_value;
extern int rcc_coroutine_exception_local_destroy_count;
extern int rcc_coroutine_exception_payload_destroy_count;
extern int rcc_coroutine_exception_protocol_stage;
extern int rcc_coroutine_exception_awaiter_destroy_count;
extern void* rcc_coroutine_exception_last_frame;

extern void rcc_coroutine_body_throw(void)
    __asm__("_Z24rcc_coroutine_body_throwv");
extern void rcc_coroutine_initializer_throw(void)
    __asm__("_Z31rcc_coroutine_initializer_throwv");
extern void rcc_coroutine_caught_throw(void)
    __asm__("_Z26rcc_coroutine_caught_throwv");
extern void rcc_coroutine_throw_after_suspend(void)
    __asm__("_Z33rcc_coroutine_throw_after_suspendv");
extern void rcc_coroutine_throw_class_after_suspend(void)
    __asm__("_Z39rcc_coroutine_throw_class_after_suspendv");
extern void rcc_coroutine_protocol_throw(void)
    __asm__("_Z28rcc_coroutine_protocol_throwv");
extern void rcc_coroutine_throwing_awaiter_initializer(void)
    __asm__("_Z42rcc_coroutine_throwing_awaiter_initializerv");
extern void rcc_coroutine_throwing_awaiter_constructor(void)
    __asm__("_Z42rcc_coroutine_throwing_awaiter_constructorv");

extern void rin_coro_resume(void* frame);

#define CHECK(condition, failure_code) \
    do { if (!(condition)) return (failure_code); } while (0)

int main(int argc, char** argv)
{
    int expected_unhandled = 0;
    (void)argc;
    (void)argv;

    rcc_coroutine_exception_unhandled_count = 0;
    rcc_coroutine_exception_caught_value = 0;
    rcc_coroutine_exception_local_destroy_count = 0;
    rcc_coroutine_exception_payload_destroy_count = 0;
    rcc_coroutine_exception_protocol_stage = 0;
    rcc_coroutine_exception_awaiter_destroy_count = 0;
    rcc_coroutine_exception_last_frame = NULL;

    rcc_coroutine_body_throw();
    CHECK(++expected_unhandled == rcc_coroutine_exception_unhandled_count, 11);

    rcc_coroutine_initializer_throw();
    CHECK(++expected_unhandled == rcc_coroutine_exception_unhandled_count, 12);

    rcc_coroutine_caught_throw();
    CHECK(rcc_coroutine_exception_caught_value == 8, 13);
    CHECK(expected_unhandled == rcc_coroutine_exception_unhandled_count, 14);

    rcc_coroutine_throw_after_suspend();
    CHECK(rcc_coroutine_exception_last_frame != NULL, 15);
    CHECK(rcc_coroutine_exception_local_destroy_count == 0, 16);
    rin_coro_resume(rcc_coroutine_exception_last_frame);
    rcc_coroutine_exception_last_frame = NULL;
    CHECK(++expected_unhandled == rcc_coroutine_exception_unhandled_count, 17);
    CHECK(rcc_coroutine_exception_local_destroy_count == 1, 18);

    rcc_coroutine_throw_class_after_suspend();
    CHECK(rcc_coroutine_exception_last_frame != NULL, 19);
    CHECK(rcc_coroutine_exception_payload_destroy_count == 0, 20);
    rin_coro_resume(rcc_coroutine_exception_last_frame);
    rcc_coroutine_exception_last_frame = NULL;
    CHECK(++expected_unhandled == rcc_coroutine_exception_unhandled_count, 21);
    CHECK(rcc_coroutine_exception_payload_destroy_count == 1, 22);

    for (int stage = 1; stage <= 3; ++stage) {
        int previous_destroy_count =
            rcc_coroutine_exception_awaiter_destroy_count;
        rcc_coroutine_exception_protocol_stage = stage;
        rcc_coroutine_protocol_throw();
        CHECK(++expected_unhandled == rcc_coroutine_exception_unhandled_count,
              23 + stage);
        CHECK(rcc_coroutine_exception_awaiter_destroy_count ==
                  previous_destroy_count + 1,
              26 + stage);
    }

    rcc_coroutine_exception_protocol_stage = 0;
    rcc_coroutine_throwing_awaiter_initializer();
    CHECK(++expected_unhandled == rcc_coroutine_exception_unhandled_count, 30);

    rcc_coroutine_throwing_awaiter_constructor();
    CHECK(++expected_unhandled == rcc_coroutine_exception_unhandled_count, 31);
    return 0;
}
