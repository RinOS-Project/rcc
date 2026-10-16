struct cxx_bitfield_value {
    unsigned low : 3;
    int signed_value : 5;
    unsigned high : 8;
    unsigned : 3;
    unsigned tail : 13;
    unsigned char byte_low : 4;
    unsigned char byte_high : 4;
};

struct cxx_bitfield_zero_width_barrier {
    unsigned before : 3;
    unsigned : 0;
    unsigned after : 3;
};

struct cxx_bitfield_wide_storage {
    unsigned long long low : 3;
    unsigned long long wide : 40;
    unsigned long long upper : 20;
    unsigned long long full : 64;
    long long signed_wide : 40;
    long long signed_high : 20;
};

static cxx_bitfield_wide_storage static_wide_storage = {
    3, 0x23456789abULL, 0xabcdeULL, 0x0123456789abcdefULL,
    -0x23456789aLL, -0x54321LL
};

extern "C" int main() {
    cxx_bitfield_value value;
    cxx_bitfield_zero_width_barrier barrier;
    cxx_bitfield_wide_storage wide;
    cxx_bitfield_wide_storage initialized = {
        2, 0x3456789abcULL, 0x54321ULL, 0x1020304050607080ULL,
        -0x3456789abLL, -0x12345LL
    };
    value.low = 5;
    value.signed_value = -7;
    value.high = 0xa5;
    value.tail = 0x1234;
    value.byte_low = 0xf;
    value.byte_high = 0xa;
    barrier.before = 5;
    barrier.after = 6;
    wide.low = 5;
    wide.wide = 0x123456789aULL;
    wide.upper = 0xabcdeULL;
    wide.full = 0xfedcba9876543210ULL;
    wide.signed_wide = -0x123456789LL;
    wide.signed_high = -0x54321LL;
    if (sizeof(cxx_bitfield_value) != 8 || value.low != 5 ||
        value.signed_value != -7 || value.high != 0xa5 ||
        value.tail != 0x1234 || value.byte_low != 0xf ||
        value.byte_high != 0xa) {
        return 1;
    }
    value.low += 1;
    value.signed_value--;
    value.high ^= 1;
    if (value.low != 6 || value.signed_value != -8 || value.high != 0xa4) {
        return 2;
    }
    if (sizeof(cxx_bitfield_zero_width_barrier) != 8 ||
        barrier.before != 5 || barrier.after != 6) {
        return 3;
    }
    if (sizeof(cxx_bitfield_wide_storage) != 24 || wide.low != 5 ||
        wide.wide != 0x123456789aULL ||
        wide.upper != 0xabcdeULL ||
        wide.full != 0xfedcba9876543210ULL ||
        wide.signed_wide != -0x123456789LL ||
        wide.signed_high != -0x54321LL) {
        return 4;
    }
    if (initialized.low != 2 || initialized.wide != 0x3456789abcULL ||
        initialized.upper != 0x54321ULL ||
        initialized.full != 0x1020304050607080ULL ||
        initialized.signed_wide != -0x3456789abLL ||
        initialized.signed_high != -0x12345LL ||
        static_wide_storage.low != 3 ||
        static_wide_storage.wide != 0x23456789abULL ||
        static_wide_storage.upper != 0xabcdeULL ||
        static_wide_storage.full != 0x0123456789abcdefULL ||
        static_wide_storage.signed_wide != -0x23456789aLL ||
        static_wide_storage.signed_high != -0x54321LL) {
        return 5;
    }
    wide.wide += 2;
    wide.wide *= 2;
    wide.wide /= 2;
    wide.wide <<= 1;
    wide.wide >>= 1;
    wide.signed_wide += 1;
    wide.signed_high += 1;
    wide.low++;
    unsigned long long previous_full = wide.full++;
    if (wide.low != 6 || wide.wide != 0x123456789cULL ||
        wide.upper != 0xabcdeULL ||
        wide.signed_wide != -0x123456788LL ||
        wide.signed_high != -0x54320LL ||
        previous_full != 0xfedcba9876543210ULL ||
        wide.full != 0xfedcba9876543211ULL) {
        return 6;
    }
    return 0;
}
