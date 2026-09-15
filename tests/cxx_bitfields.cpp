struct cxx_bitfield_value {
    unsigned low : 3;
    int signed_value : 5;
    unsigned high : 8;
    unsigned : 3;
    unsigned tail : 13;
    unsigned char byte_low : 4;
    unsigned char byte_high : 4;
};

extern "C" int main() {
    cxx_bitfield_value value;
    value.low = 5;
    value.signed_value = -7;
    value.high = 0xa5;
    value.tail = 0x1234;
    value.byte_low = 0xf;
    value.byte_high = 0xa;
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
    return 0;
}
