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

static struct bitfield_value initialized_value = {
    .low = 3,
    .signed_value = -2,
    .high = 0xa5,
    .tail = 0x1234,
    .byte_low = 0xf,
    .byte_high = 0xa
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
    return 0;
}

int bitfield_runtime(void) {
    struct bitfield_value value = {0};
    union bitfield_union alternate;

    value.low = 5;
    value.signed_value = -7;
    value.high = 0xa5;
    value.tail = 0x1234;
    value.byte_low = 0xf;
    value.byte_high = 0xa;
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
    return 0;
}

int bitfield_address_is_rejected(void) {
    return 0;
}
