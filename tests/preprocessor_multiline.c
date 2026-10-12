#define RCC_MULTILINE_ADD(left, right) ((left) + (right))
#define RCC_MULTILINE_ID(value) (value)

_Static_assert(
    RCC_MULTILINE_ADD(
        RCC_MULTILINE_ID(19),
        (23)
    ) == 42,
    "multiline C macro arguments must expand correctly");

int rcc_preprocessor_multiline_c(void) {
    return RCC_MULTILINE_ADD(
        20,
        22
    );
}
