typedef signed _BitInt(7) s7;
typedef unsigned _BitInt(7) u7;
typedef unsigned _BitInt(9) u9;
typedef unsigned _BitInt(33) u33;
typedef signed _BitInt(63) s63;
typedef unsigned _BitInt(63) u63;
typedef signed _BitInt(64) s64;
typedef unsigned _BitInt(64) u64;

struct bitint_promotion_fields {
    unsigned _BitInt(7) unsigned_narrow : 2;
    signed _BitInt(7) signed_narrow : 2;
    unsigned _BitInt(33) unsigned_wide : 33;
};

#define RCC_HAS_TYPE(expression, type) \
    _Generic((expression), type: 1, default: 0)

/* C23 6.3.1.1 keeps a bit-precise bit-field's declared type during integer
 * promotions, even when its declared field width is narrower. */
_Static_assert(RCC_HAS_TYPE(
                   ((struct bitint_promotion_fields){0}).unsigned_narrow,
                   u7),
               "bit-precise bit-field retains its declared type");
_Static_assert(RCC_HAS_TYPE(
                   +((struct bitint_promotion_fields){0}).unsigned_narrow,
                   u7),
               "unary plus does not promote a bit-precise bit-field");
_Static_assert(RCC_HAS_TYPE(
                   -((struct bitint_promotion_fields){0}).signed_narrow,
                   s7),
               "unary minus does not promote a bit-precise bit-field");
_Static_assert(RCC_HAS_TYPE(
                   ~((struct bitint_promotion_fields){0}).unsigned_narrow,
                   u7),
               "bitwise complement does not promote a bit-precise bit-field");
_Static_assert(RCC_HAS_TYPE(
                   ((struct bitint_promotion_fields){0}).unsigned_narrow << 1,
                   u7),
               "shift result keeps the promoted left operand type");
_Static_assert(RCC_HAS_TYPE(
                   ((struct bitint_promotion_fields){0}).signed_narrow +
                       (u7)1,
                   u7),
               "same-rank signed and unsigned bit-precise types use unsigned");
_Static_assert(RCC_HAS_TYPE(
                   ((struct bitint_promotion_fields){0}).unsigned_narrow + 0,
                   int),
               "standard int represents every unsigned bit-precise 7-bit value");
_Static_assert(RCC_HAS_TYPE((u7)1 + (u9)1, u9),
               "mixed bit-precise arithmetic uses the higher rank");
_Static_assert(RCC_HAS_TYPE(1 ? (u7)1 : (u9)1, u9),
               "conditional expression uses the higher bit-precise rank");
_Static_assert(RCC_HAS_TYPE(1 ? (s7)1 : (u7)1, u7),
               "conditional expression uses unsigned for equal ranks");
_Static_assert(RCC_HAS_TYPE(1 ? (u7)1 : 0, int),
               "conditional expression applies usual conversions with int");
_Static_assert(RCC_HAS_TYPE((u63)1 + 1LL, long long),
               "signed long long represents every unsigned 63-bit value");
_Static_assert(RCC_HAS_TYPE((u64)1 + 1LL, unsigned long long),
               "unsigned 64-bit values do not fit signed long long");
_Static_assert(RCC_HAS_TYPE((s64)1 + 1LL, long long),
               "standard long long outranks same-width signed _BitInt");
_Static_assert(RCC_HAS_TYPE((s64)1 + 1ULL, unsigned long long),
               "standard unsigned long long outranks signed _BitInt");
_Static_assert(RCC_HAS_TYPE((u64)1 + 1ULL, unsigned long long),
               "standard unsigned long long outranks same-width _BitInt");
_Static_assert(RCC_HAS_TYPE((u7)127 + (u7)1, u7) &&
                   (u7)127 + (u7)1 == 0,
               "unsigned bit-precise arithmetic wraps at its value width");
_Static_assert((s7)63 + (s7)-63 == 0,
               "representable signed bit-precise arithmetic remains valid");
_Static_assert((s7)31 << 1 == 62,
               "representable signed bit-precise left shift remains valid");

static int bitint_switch_value(u7 value) {
    switch (value) {
        case 127: return 1;
        case 0: return 2;
        default: return 3;
    }
}

int main(void) {
    struct bitint_promotion_fields fields = {0};
    u7 incremented = 127;
    u7 previous;
    u63 unsigned_63_high = (u63)1 << 62;
    u64 unsigned_64_high = (u64)1 << 63;
    fields.unsigned_narrow = 3;
    fields.signed_narrow = -1;
    fields.unsigned_wide = ((u33)1 << 32) | (u33)17;

    if (+fields.unsigned_narrow != 3 || -fields.signed_narrow != 1 ||
        ~fields.unsigned_narrow != 124 ||
        (fields.unsigned_narrow << 1) != 6) {
        return 1;
    }
    if ((u7)64 << 1 != 0 || ((u7)64 >> 6) != 1 ||
        fields.unsigned_narrow + 0 != 3 ||
        fields.unsigned_narrow + (u9)3 != 6) {
        return 2;
    }
    if (fields.unsigned_wide != (((u33)1 << 32) | (u33)17)) {
        return 3;
    }
    previous = incremented++;
    if (previous != 127 || incremented != 0) return 4;
    incremented = 127;
    incremented += (u7)1;
    if (incremented != 0) return 5;
    if (bitint_switch_value((u7)127) != 1 ||
        bitint_switch_value((u7)0) != 2) {
        return 6;
    }
    if ((long long)(unsigned_63_high + 1LL) != 4611686018427387905LL ||
        (unsigned long long)(unsigned_64_high + 1LL) !=
            9223372036854775809ULL) {
        return 7;
    }
    return 0;
}
