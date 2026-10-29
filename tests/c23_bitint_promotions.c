typedef signed _BitInt(7) s7;
typedef unsigned _BitInt(7) u7;
typedef unsigned _BitInt(9) u9;
typedef unsigned _BitInt(33) u33;

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
_Static_assert(RCC_HAS_TYPE((u7)127 + (u7)1, u7) &&
                   (u7)127 + (u7)1 == 0,
               "unsigned bit-precise arithmetic wraps at its value width");
_Static_assert((s7)63 + (s7)-63 == 0,
               "representable signed bit-precise arithmetic remains valid");

int main(void) {
    struct bitint_promotion_fields fields = {0};
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
    return 0;
}
