/* C17 preprocessing replacement-list operators. */
#define WORD 42
#define RAW_STRING(x) #x
#define STRING(x) RAW_STRING(x)
#define RAW_ARGS(...) #__VA_ARGS__
#define RAW_JOIN(a, b) a##b
#define JOIN(a, b) RAW_JOIN(a, b)
#define prefix_42 77
#define number_123 123

static const char raw_string[] = RAW_STRING(WORD +   1);
static const char expanded_string[] = STRING(WORD +   1);
static const char variadic_string[] = RAW_ARGS(one,   two);
int pasted_identifier = JOIN(prefix_, WORD);
int pasted_number = JOIN(number_, 123);

#if JOIN(prefix_, WORD) != 77
#error "token pasting did not rescan the pasted token"
#endif

#if JOIN(number_, 123) != 123
#error "token pasting did not form a numeric token"
#endif

int preprocessor_operators(void) {
    return pasted_identifier + pasted_number +
           (raw_string[0] == 'W' ? 1 : 0) +
           (expanded_string[0] == '4' ? 1 : 0) +
           (variadic_string[4] == ',' ? 1 : 0);
}
