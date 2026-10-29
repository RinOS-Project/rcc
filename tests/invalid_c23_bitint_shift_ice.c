typedef unsigned _BitInt(7) u7;
typedef _BitInt(7) s7;

_Static_assert(((u7)1 << 7) == 0,
               "shift count equal to the bit-precise width is invalid");
_Static_assert(((s7)-1 << 1) == -2,
               "negative signed left operand is invalid");
_Static_assert(((s7)63 << 1) == -2,
               "signed bit-precise left-shift overflow is invalid");
