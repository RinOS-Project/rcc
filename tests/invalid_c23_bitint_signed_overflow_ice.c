typedef _BitInt(7) s7;

_Static_assert((s7)63 + (s7)1 == -64,
               "signed bit-precise overflow is not a constant expression");
_Static_assert((s7)-64 / (s7)-1 == -64,
               "signed bit-precise division overflow is not a constant expression");
