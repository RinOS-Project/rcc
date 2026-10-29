typedef _BitInt(7) s7;

enum {
    overflowed_shift = (s7)63 << 1,
    negative_signed_shift = (s7)-1 << 1,
    oversized_shift_count = (s7)1 << 7
};
