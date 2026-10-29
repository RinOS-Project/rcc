typedef _BitInt(7) s7;
typedef _BitInt(64) s64;

_Static_assert((s7)63 + (s7)1 == -64,
               "signed bit-precise overflow is not a constant expression");
_Static_assert((s7)-64 - (s7)1 == 63,
               "signed bit-precise subtraction overflow is not a constant expression");
_Static_assert((s7)63 * (s7)2 == -2,
               "signed bit-precise multiplication overflow is not a constant expression");
_Static_assert(-((s7)-64) == -64,
               "signed bit-precise negation overflow is not a constant expression");
_Static_assert((s7)-64 / (s7)-1 == -64,
               "signed bit-precise division overflow is not a constant expression");
_Static_assert((s7)-64 % (s7)-1 == 0,
               "signed bit-precise remainder overflow is not a constant expression");
_Static_assert((s64)9223372036854775807LL + (s64)1 == 0,
               "signed 64-bit bit-precise addition overflow is not a constant expression");
_Static_assert(-((s64)(-9223372036854775807LL - 1)) ==
                   (s64)(-9223372036854775807LL - 1),
               "signed 64-bit bit-precise negation overflow is not a constant expression");
