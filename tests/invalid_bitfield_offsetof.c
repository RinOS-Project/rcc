struct invalid_bitfield_offsetof {
    unsigned value : 3;
};

int invalid_bitfield_offsetof(void) {
    return __builtin_offsetof(struct invalid_bitfield_offsetof, value);
}
