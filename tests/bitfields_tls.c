struct bitfield_tls_value {
    unsigned low : 3;
    unsigned high : 5;
};

_Thread_local struct bitfield_tls_value tls_value = {
    .low = 3,
    .high = 17,
};

int bitfield_tls_read(void) {
    return tls_value.low + tls_value.high;
}
