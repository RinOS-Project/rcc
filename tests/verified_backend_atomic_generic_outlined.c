/* SPDX-License-Identifier: MIT */
#include <rin/atomic_fallback_v1.h>

typedef struct {
    unsigned char bytes[3];
} three_byte_aggregate;

typedef struct __attribute__((packed)) {
    unsigned int low;
    unsigned int high;
} eight_byte_aggregate;

typedef struct {
    unsigned char bytes[13];
} thirteen_byte_aggregate;

typedef struct __attribute__((packed)) {
    unsigned char prefix;
    three_byte_aggregate value;
} three_byte_storage;

typedef struct __attribute__((packed)) {
    unsigned char prefix;
    eight_byte_aggregate value;
} eight_byte_storage;

typedef struct __attribute__((packed)) {
    unsigned char prefix;
    thirteen_byte_aggregate value;
} thirteen_byte_storage;

static volatile three_byte_storage three_object_storage = {
    0u, {{0x11u, 0x22u, 0x33u}}
};
static volatile eight_byte_storage eight_object_storage = {
    0u, {0x10293847u, 0x50617283u}
};
static volatile thirteen_byte_storage thirteen_object_storage = {
    0u, {{0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u,
          0x08u, 0x09u, 0x0au, 0x0bu, 0x0cu, 0x0du}}
};

static int same_three_byte(three_byte_aggregate left,
                           three_byte_aggregate right)
{
    return left.bytes[0] == right.bytes[0] &&
        left.bytes[1] == right.bytes[1] &&
        left.bytes[2] == right.bytes[2];
}

static int same_eight_byte(eight_byte_aggregate left,
                           eight_byte_aggregate right)
{
    return left.low == right.low && left.high == right.high;
}

static int same_thirteen_byte(thirteen_byte_aggregate left,
                              thirteen_byte_aggregate right)
{
    unsigned int index;
    for (index = 0u; index < 13u; ++index) {
        if (left.bytes[index] != right.bytes[index]) return 0;
    }
    return 1;
}

static int test_three_byte_atomic(void)
{
    volatile three_byte_aggregate* object =
        (volatile three_byte_aggregate*)((volatile unsigned char*)
            &three_object_storage + 1);
    three_byte_aggregate source = {{0x41u, 0x52u, 0x63u}};
    three_byte_aggregate loaded = {{0u, 0u, 0u}};
    three_byte_aggregate previous = {{0u, 0u, 0u}};
    three_byte_aggregate expected = {{0x91u, 0x92u, 0x93u}};
    three_byte_aggregate desired = {{0xa1u, 0xb2u, 0xc3u}};
    three_byte_aggregate initial = {{0x14u, 0x25u, 0x36u}};
    int failed;
    int succeeded;

    __atomic_store(object, &source, __ATOMIC_RELEASE);
    if (!same_three_byte(*object, source)) return 101;
    __atomic_load(object, &loaded, __ATOMIC_ACQUIRE);
    if (!same_three_byte(loaded, source)) return 102;

    __atomic_store(object, &initial, __ATOMIC_RELAXED);
    __atomic_exchange(object, &source, &previous, __ATOMIC_ACQ_REL);
    if (!same_three_byte(previous, initial) ||
        !same_three_byte(*object, source)) return 103;

    failed = __atomic_compare_exchange(
        object, &expected, &desired, 0,
        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    if (failed || !same_three_byte(expected, source) ||
        !same_three_byte(*object, source)) return 104;
    succeeded = __atomic_compare_exchange(
        object, &expected, &desired, 0,
        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    return succeeded && same_three_byte(*object, desired) ? 0 : 105;
}

static int test_eight_byte_atomic(void)
{
    volatile eight_byte_aggregate* object =
        (volatile eight_byte_aggregate*)((volatile unsigned char*)
            &eight_object_storage + 1);
    eight_byte_aggregate source = {0x13579bdfu, 0x2468ace0u};
    eight_byte_aggregate loaded = {0u, 0u};
    eight_byte_aggregate previous = {0u, 0u};
    eight_byte_aggregate expected = {0x01020304u, 0x05060708u};
    eight_byte_aggregate desired = {0x89abcdefu, 0x76543210u};
    eight_byte_aggregate initial = {0xa1b2c3d4u, 0xe5f60718u};
    int failed;
    int succeeded;

    __atomic_store(object, &source, __ATOMIC_RELEASE);
    __atomic_load(object, &loaded, __ATOMIC_ACQUIRE);
    if (!same_eight_byte(*object, source) ||
        !same_eight_byte(loaded, source)) return 1;

    __atomic_store(object, &initial, __ATOMIC_RELAXED);
    __atomic_exchange(object, &source, &previous, __ATOMIC_ACQ_REL);
    if (!same_eight_byte(previous, initial) ||
        !same_eight_byte(*object, source)) return 1;

    failed = __atomic_compare_exchange(
        object, &expected, &desired, 0,
        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    if (failed || !same_eight_byte(expected, source) ||
        !same_eight_byte(*object, source)) return 1;
    succeeded = __atomic_compare_exchange(
        object, &expected, &desired, 0,
        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    return succeeded && same_eight_byte(*object, desired) ? 0 : 1;
}

static int test_thirteen_byte_atomic(void)
{
    volatile thirteen_byte_aggregate* object =
        (volatile thirteen_byte_aggregate*)((volatile unsigned char*)
            &thirteen_object_storage + 1);
    thirteen_byte_aggregate source = {{
        0x41u, 0x42u, 0x43u, 0x44u, 0x45u, 0x46u, 0x47u,
        0x48u, 0x49u, 0x4au, 0x4bu, 0x4cu, 0x4du
    }};
    thirteen_byte_aggregate loaded = {{0u}};
    thirteen_byte_aggregate previous = {{0u}};
    thirteen_byte_aggregate expected = {{
        0x91u, 0x92u, 0x93u, 0x94u, 0x95u, 0x96u, 0x97u,
        0x98u, 0x99u, 0x9au, 0x9bu, 0x9cu, 0x9du
    }};
    thirteen_byte_aggregate desired = {{
        0xa1u, 0xa2u, 0xa3u, 0xa4u, 0xa5u, 0xa6u, 0xa7u,
        0xa8u, 0xa9u, 0xaau, 0xabu, 0xacu, 0xadu
    }};
    thirteen_byte_aggregate initial = {{
        0x14u, 0x15u, 0x16u, 0x17u, 0x18u, 0x19u, 0x1au,
        0x1bu, 0x1cu, 0x1du, 0x1eu, 0x1fu, 0x20u
    }};
    int failed;
    int succeeded;

    __atomic_store(object, &source, __ATOMIC_RELEASE);
    if (!same_thirteen_byte(*object, source)) return 201;
    __atomic_load(object, &loaded, __ATOMIC_ACQUIRE);
    if (!same_thirteen_byte(loaded, source)) return 202;

    __atomic_store(object, &initial, __ATOMIC_RELAXED);
    __atomic_exchange(object, &source, &previous, __ATOMIC_ACQ_REL);
    if (!same_thirteen_byte(previous, initial) ||
        !same_thirteen_byte(*object, source)) return 203;

    failed = __atomic_compare_exchange(
        object, &expected, &desired, 0,
        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    if (failed || !same_thirteen_byte(expected, source) ||
        !same_thirteen_byte(*object, source)) return 204;
    succeeded = __atomic_compare_exchange(
        object, &expected, &desired, 0,
        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    return succeeded && same_thirteen_byte(*object, desired) ? 0 : 205;
}

static int test_overlapping_exchange_buffers(void)
{
    unsigned char bytes[6] = {0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u};
    __rin_atomic_exchange_v1(&bytes[1], &bytes[2], &bytes[0], 4u,
                             __ATOMIC_SEQ_CST);
    return bytes[0] == 0x20u && bytes[1] == 0x30u &&
        bytes[2] == 0x40u && bytes[3] == 0x50u &&
        bytes[4] == 0x60u && bytes[5] == 0x60u ? 0 : 1;
}

int main(void)
{
    int result = test_three_byte_atomic();
    if (result != 0) return result;
    result = test_eight_byte_atomic();
    if (result != 0) return result + 100;
    result = test_thirteen_byte_atomic();
    if (result != 0) return result + 200;
    result = test_overlapping_exchange_buffers();
    if (result != 0) return result + 200;
    return 0;
}
