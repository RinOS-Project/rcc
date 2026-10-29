typedef _BitInt(7) s7;
typedef _BitInt(64) s64;

static int overflowed_initializer = (s7)-64 - (s7)63;
static int overflowed_negation = -((s7)-64);
static int overflowed_wide_initializer =
    (s64)9223372036854775807LL + (s64)1;
