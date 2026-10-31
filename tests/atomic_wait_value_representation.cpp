#include <__rcc_atomic_wait_value.h>

struct AtomicWaitPaddedValue {
    unsigned char tag;
    unsigned value;
};

static_assert(sizeof(AtomicWaitPaddedValue) == 8u,
              "atomic wait fixture must contain internal padding");

static bool equal_ignoring_padding(void) {
    AtomicWaitPaddedValue observed = {0x35u, 0x4678a9bcu};
    AtomicWaitPaddedValue expected = {0x35u, 0x4678a9bcu};
    volatile unsigned char* observed_bytes =
        reinterpret_cast<volatile unsigned char*>(&observed);
    volatile unsigned char* expected_bytes =
        reinterpret_cast<volatile unsigned char*>(&expected);

    observed_bytes[1] = 0xa1u;
    observed_bytes[2] = 0xb2u;
    observed_bytes[3] = 0xc3u;
    expected_bytes[1] = 0xd4u;
    expected_bytes[2] = 0xe5u;
    expected_bytes[3] = 0xf6u;
    return std::__rcc_atomic_wait_value_equal_v1(observed, expected);
}

static bool compare_value_bytes(void) {
    AtomicWaitPaddedValue observed = {0x35u, 0x4678a9bcu};
    AtomicWaitPaddedValue expected = {0x35u, 0x4678a9bdu};
    return !std::__rcc_atomic_wait_value_equal_v1(observed, expected);
}

int main(void) {
    return equal_ignoring_padding() && compare_value_bytes() ? 0 : 1;
}
