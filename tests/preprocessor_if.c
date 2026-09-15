/* C17 preprocessing integer constant expressions. */
#define FLAG 1
#define VALUE (3 * 4)
#define SHIFT 2
#define ADD(left, right) ((left) + (right))

#if !defined(MISSING) && defined(FLAG) && FLAG
#else
#error "defined or object-like macro evaluation failed"
#endif

#if VALUE == 12 && ADD(2, 3) == 5
#else
#error "function-like macro expansion in #if failed"
#endif

#if (1 << SHIFT) == 4 && (0x30 >> 3) == 6
#else
#error "shift operators in #if failed"
#endif

#if (17 % 5) == 2 && (7 & 3) == 3 && (4 ^ 1) == 5 && (4 | 2) == 6
#else
#error "arithmetic or bitwise operators in #if failed"
#endif

#if ('A' == 65) && ('\n' != 0) && (-3 < -2) && ((1 ? 9 : 0) == 9)
#else
#error "character, unary, comparison, or conditional operators failed"
#endif

#if 0 || (1 && 0)
#error "logical operator precedence failed"
#endif

#if UNDEFINED_IDENTIFIER
#error "undefined identifiers must evaluate to zero"
#endif

int preprocessor_if(void) {
    return 0;
}
