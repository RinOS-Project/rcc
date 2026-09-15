/* C++20 variadic macro optional replacement list. */
#define OPTIONAL_SUM(first, ...) (first __VA_OPT__(+ __VA_ARGS__))

int optional_empty = OPTIONAL_SUM(7);
int optional_value = OPTIONAL_SUM(7,8);

#if OPTIONAL_SUM(7) != 7
#error "__VA_OPT__ emitted tokens for an empty argument list"
#endif

#if OPTIONAL_SUM(7,8) != 15
#error "__VA_OPT__ omitted a non-empty replacement list"
#endif

int preprocessor_va_opt(void) {
    return optional_empty == 7 && optional_value == 15 ? 0 : 1;
}
