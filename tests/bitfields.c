struct bitfield_value {
    unsigned low : 3;
    signed int signed_value : 5;
    unsigned high : 8;
    unsigned : 3;
    unsigned tail : 13;
    unsigned char byte_low : 4;
    unsigned char byte_high : 4;
};

union bitfield_union {
    unsigned value : 4;
    unsigned char byte : 4;
};

struct bitfield_wide_storage {
    unsigned long long low : 3;
    unsigned long long wide : 40;
    unsigned long long upper : 20;
    unsigned long long full : 64;
    long long signed_wide : 40;
    long long signed_high : 20;
};

struct bitfield_full_unsigned_int {
    unsigned int value : 32;
};

#define RCC_BITFIELD_HAS_TYPE(expression, type) \
    _Generic((expression), type: 1, default: 0)

_Static_assert(RCC_BITFIELD_HAS_TYPE(
                   +((struct bitfield_value){0}).low, int),
               "narrow unsigned-int bit-fields promote to int");
_Static_assert(RCC_BITFIELD_HAS_TYPE(
                   ~((struct bitfield_value){0}).low, int),
               "bitwise complement promotes a narrow unsigned-int bit-field");
_Static_assert(RCC_BITFIELD_HAS_TYPE(
                   ((struct bitfield_value){0}).low + 0, int),
               "usual arithmetic conversions use the bit-field width");
_Static_assert(RCC_BITFIELD_HAS_TYPE(
                   ((struct bitfield_value){0}).low << 1, int),
               "shift result uses the promoted bit-field type");
_Static_assert(RCC_BITFIELD_HAS_TYPE(
                   +((struct bitfield_full_unsigned_int){0}).value,
                   unsigned int),
               "a full-width unsigned-int bit-field promotes to unsigned int");

static struct bitfield_value initialized_value = {
    .low = 3,
    .signed_value = -2,
    .high = 0xa5,
    .tail = 0x1234,
    .byte_low = 0xf,
    .byte_high = 0xa
};

static struct bitfield_wide_storage initialized_wide = {
    3, 0x23456789abULL, 0xabcdeULL, 0x0123456789abcdefULL,
    -0x23456789aLL, -0x54321LL
};

int bitfield_static(void) {
    if (sizeof(struct bitfield_value) != 8) return 1;
    if (initialized_value.low != 3 || initialized_value.signed_value != -2) {
        return 2;
    }
    if (initialized_value.high != 0xa5 || initialized_value.tail != 0x1234) {
        return 3;
    }
    if (initialized_value.byte_low != 0xf || initialized_value.byte_high != 0xa) {
        return 4;
    }
    if (sizeof(struct bitfield_wide_storage) != 24 ||
        initialized_wide.low != 3 ||
        initialized_wide.wide != 0x23456789abULL ||
        initialized_wide.upper != 0xabcdeULL ||
        initialized_wide.full != 0x0123456789abcdefULL ||
        initialized_wide.signed_wide != -0x23456789aLL ||
        initialized_wide.signed_high != -0x54321LL) {
        return 5;
    }
    return 0;
}

int bitfield_runtime(void) {
    struct bitfield_value value = {0};
    struct bitfield_wide_storage wide = {
        2, 0x3456789abcULL, 0x54321ULL, 0x1020304050607080ULL,
        -0x3456789abLL, -0x12345LL
    };
    union bitfield_union alternate;

    value.low = 5;
    value.signed_value = -7;
    value.high = 0xa5;
    value.tail = 0x1234;
    value.byte_low = 0xf;
    value.byte_high = 0xa;
    if (value.low < -1) return 19;
    if (value.low != 5 || value.signed_value != -7 || value.high != 0xa5) {
        return 10;
    }
    if (value.tail != 0x1234 || value.byte_low != 0xf ||
        value.byte_high != 0xa) {
        return 11;
    }

    value.low += 1;
    if (value.low++ != 6 || value.low != 7) return 12;
    --value.signed_value;
    if (value.signed_value != -8) return 13;
    value.high ^= 1;
    value.tail >>= 1;
    value.byte_low <<= 1;
    value.byte_high |= 3;
    if (value.high != 0xa4 || value.tail != 0x091a ||
        value.byte_low != 0xe || value.byte_high != 0xb) {
        return 14;
    }

    alternate.value = 0xa;
    if (alternate.value != 0xa) return 15;
    alternate.byte = 5;
    if (alternate.byte != 5) return 16;
    if (wide.low != 2 || wide.wide != 0x3456789abcULL ||
        wide.upper != 0x54321ULL ||
        wide.full != 0x1020304050607080ULL ||
        wide.signed_wide != -0x3456789abLL ||
        wide.signed_high != -0x12345LL) return 17;
    wide.low = 5;
    wide.wide = 0x123456789aULL;
    wide.upper = 0xabcdeULL;
    wide.full = 0xfedcba9876543210ULL;
    wide.signed_wide = -0x123456789LL;
    wide.signed_high = -0x54321LL;
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
        wide.full != 0xfedcba9876543211ULL) return 18;
    return 0;
}

int bitfield_address_is_rejected(void) {
    return 0;
}
