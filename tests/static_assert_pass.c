typedef struct StaticAssertPair {
    int number;
    char tag;
} StaticAssertPair;

enum { STATIC_ASSERT_VALUE = 7 };

_Static_assert(sizeof(int) == 4, "RinOS int ABI");
_Static_assert(sizeof(StaticAssertPair) >= 5, "aggregate layout");
_Static_assert((STATIC_ASSERT_VALUE << 1) == 14, "enum expression");
_Static_assert(1 || (1 / 0), "constant-expression short circuit");
static_assert(1);

int main(void) {
    _Static_assert(sizeof(char) == 1, "block-scope assertion");
    return 0;
}
