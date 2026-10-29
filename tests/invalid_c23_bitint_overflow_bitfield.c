typedef _BitInt(7) s7;

struct overflowed_bitfield {
    unsigned int width : (s7)-64 - (s7)63;
};
