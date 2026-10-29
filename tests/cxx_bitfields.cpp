struct cxx_bitfield_value {
    unsigned low : 3;
    int signed_value : 5;
    unsigned high : 8;
    unsigned : 3;
    unsigned tail : 13;
    unsigned char byte_low : 4;
    unsigned char byte_high : 4;
};

struct cxx_bitfield_promotion_value {
    unsigned int narrow : 3;
    unsigned int full : 32;
};

int cxx_bitfield_promotion_kind(int) { return 1; }
int cxx_bitfield_promotion_kind(unsigned int) { return 2; }
int cxx_bitfield_promotion_kind(long long) { return 3; }
int cxx_bitfield_promotion_kind(unsigned long long) { return 4; }

enum cxx_bitfield_unscoped_unsigned : unsigned int {
    cxx_bitfield_enum_low = 1u,
    cxx_bitfield_enum_high = 7u
};

struct cxx_bitfield_unscoped_enum_value {
    cxx_bitfield_unscoped_unsigned value : 3;
};

enum class cxx_bitfield_scoped_unsigned : unsigned int {
    low = 1u,
    high = 0x80000000u
};

struct cxx_bitfield_scoped_enum_value {
    cxx_bitfield_scoped_unsigned value : 32;
};

constexpr int cxx_narrow_unsigned_bitfield_constant_comparison() {
    cxx_bitfield_promotion_value value = {};
    value.narrow = 5;
    return value.narrow < -1 ? 1 : 0;
}

static_assert(cxx_narrow_unsigned_bitfield_constant_comparison() == 0,
              "constexpr comparison uses width-sensitive bit-field promotion");

constexpr int cxx_narrow_unsigned_bitfield_constant_remainder_assignment() {
    cxx_bitfield_promotion_value value = {};
    value.narrow = 5;
    value.narrow %= -2;
    return value.narrow;
}

static_assert(
    cxx_narrow_unsigned_bitfield_constant_remainder_assignment() == 1,
    "constexpr compound assignment promotes the bit-field before remainder");

constexpr int cxx_unscoped_enum_bitfield_constant_comparison() {
    cxx_bitfield_unscoped_enum_value value = {};
    value.value = cxx_bitfield_enum_high;
    return value.value < -1 ? 1 : 0;
}

static_assert(cxx_unscoped_enum_bitfield_constant_comparison() == 1,
              "constexpr enum bit-field follows fixed underlying type");

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
    cxx_bitfield_promotion_value promotion_value;
    cxx_bitfield_unscoped_enum_value unscoped_enum_value;
    cxx_bitfield_scoped_enum_value scoped_enum_value;
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
    promotion_value.narrow = 5;
    promotion_value.full = 0x80000000u;
    unscoped_enum_value.value = cxx_bitfield_enum_high;
    scoped_enum_value.value = cxx_bitfield_scoped_unsigned::high;
    barrier.before = 5;
    barrier.after = 6;
    wide.low = 5;
    wide.wide = 0x123456789aULL;
    wide.upper = 0xabcdeULL;
    wide.full = 0xfedcba9876543210ULL;
    wide.signed_wide = -0x123456789LL;
    wide.signed_high = -0x54321LL;
    if (wide.low < -1 ||
        cxx_bitfield_promotion_kind(+wide.low) != 1 ||
        cxx_bitfield_promotion_kind(+wide.signed_high) != 1 ||
        wide.wide < 0x1000000000ULL) {
        return 10;
    }
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
    if (value.low < -1 || !(unscoped_enum_value.value < -1)) {
        return 8;
    }
    if (cxx_bitfield_promotion_kind(+promotion_value.narrow) != 1 ||
        cxx_bitfield_promotion_kind(+promotion_value.full) != 2 ||
        cxx_bitfield_promotion_kind(+unscoped_enum_value.value) != 2) {
        return 9;
    }
    if (!(scoped_enum_value.value > cxx_bitfield_scoped_unsigned::low) ||
        scoped_enum_value.value < cxx_bitfield_scoped_unsigned::low) {
        return 7;
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
