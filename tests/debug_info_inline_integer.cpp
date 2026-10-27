enum DebugInlineUnsignedComparisonEnum : unsigned int {
    debug_inline_unsigned_comparison_enum_zero = 0u,
};

DebugInlineUnsignedComparisonEnum*
    debug_inline_unsigned_comparison_enum_escape;

enum DebugInlineUnsignedCharComparisonEnum : unsigned char {
    debug_inline_unsigned_char_comparison_enum_zero = 0,
    debug_inline_unsigned_char_comparison_enum_maximum = 255,
};

DebugInlineUnsignedCharComparisonEnum*
    debug_inline_unsigned_char_comparison_enum_escape;

enum DebugInlineSignedShortComparisonEnum : short {
    debug_inline_signed_short_comparison_enum_minimum = -32768,
    debug_inline_signed_short_comparison_enum_negative = -1,
    debug_inline_signed_short_comparison_enum_zero = 0,
    debug_inline_signed_short_comparison_enum_maximum = 32767,
};

DebugInlineSignedShortComparisonEnum*
    debug_inline_signed_short_comparison_enum_escape;

enum DebugInlineSignedComparisonEnum : int {
    debug_inline_signed_comparison_enum_minimum = (-2147483647 - 1),
    debug_inline_signed_comparison_enum_negative = -1,
    debug_inline_signed_comparison_enum_zero = 0,
    debug_inline_signed_comparison_enum_maximum = 2147483647,
};

DebugInlineSignedComparisonEnum* debug_inline_signed_comparison_enum_escape;

enum class DebugInlineScopedUnsignedCharComparisonEnum : unsigned char {
    zero = 0,
    maximum = 255,
};

DebugInlineScopedUnsignedCharComparisonEnum*
    debug_inline_scoped_unsigned_char_comparison_enum_escape;

enum class DebugInlineScopedSignedShortComparisonEnum : short {
    minimum = -32768,
    negative = -1,
    zero = 0,
    maximum = 32767,
};

DebugInlineScopedSignedShortComparisonEnum*
    debug_inline_scoped_signed_short_comparison_enum_escape;

enum class DebugInlineScopedUnsignedShortComparisonEnum : unsigned short {
    zero = 0,
    maximum = 65535,
};

DebugInlineScopedUnsignedShortComparisonEnum*
    debug_inline_scoped_unsigned_short_comparison_enum_escape;

enum class DebugInlineScopedSignedComparisonEnum : int {
    minimum = (-2147483647 - 1),
    negative = -1,
    zero = 0,
    maximum = 2147483647,
};

DebugInlineScopedSignedComparisonEnum*
    debug_inline_scoped_signed_comparison_enum_escape;

enum class DebugInlineScopedUnsignedWideComparisonEnum : unsigned long long {
    zero = 0,
    high_bit = 0x8000000000000000ULL,
    maximum = 0xffffffffffffffffULL,
};

DebugInlineScopedUnsignedWideComparisonEnum*
    debug_inline_scoped_unsigned_wide_comparison_enum_escape;

enum DebugInlineUnsignedShortComparisonEnum : unsigned short {
    debug_inline_unsigned_short_comparison_enum_zero = 0,
    debug_inline_unsigned_short_comparison_enum_maximum = 65535,
};

DebugInlineUnsignedShortComparisonEnum*
    debug_inline_unsigned_short_comparison_enum_escape;

enum DebugInlineSignedCharComparisonEnum : signed char {
    debug_inline_signed_char_comparison_enum_minimum = -128,
    debug_inline_signed_char_comparison_enum_negative = -1,
    debug_inline_signed_char_comparison_enum_zero = 0,
    debug_inline_signed_char_comparison_enum_maximum = 127,
};

DebugInlineSignedCharComparisonEnum*
    debug_inline_signed_char_comparison_enum_escape;

enum class DebugInlineGlobalUnsignedChar : unsigned char {
    widen_value = 0xe7u
};
enum class DebugInlineGlobalSignedShort : short {
    widen_value = -0x1234
};
enum class DebugInlineGlobalUnsignedInt : unsigned int {
    widen_value = 0x89abcdefu
};
enum class DebugInlineGlobalSignedChar : signed char {
    widen_value = -37
};
enum class DebugInlineGlobalUnsignedShort : unsigned short {
    widen_value = 0xd123u
};
enum class DebugInlineGlobalSignedInt : signed int {
    widen_value = -0x1234567
};

extern "C" {
unsigned long long debug_inline_nonconstant_unsigned_long_long_global_source =
    0x8000000100000003ULL;
unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_add_source =
    0x7ffffffffffffff0ULL;
unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_mod_source =
    0xfedcba9876543217ULL;
long long debug_inline_nonconstant_signed_long_long_global_add_lhs =
    0x1ffffffffLL;
long long debug_inline_nonconstant_signed_long_long_global_add_rhs = 1LL;
long long debug_inline_nonconstant_signed_long_long_global_sub_lhs =
    0x100000000LL;
long long debug_inline_nonconstant_signed_long_long_global_sub_rhs =
    0x100000001LL;
thread_local long long
debug_inline_nonconstant_signed_long_long_tls_add_lhs = 0x1ffffffffLL;
thread_local long long debug_inline_nonconstant_signed_long_long_tls_add_rhs =
    1LL;
thread_local long long
debug_inline_nonconstant_signed_long_long_tls_sub_lhs =
    (-0x7fffffffffffffffLL - 1LL);
thread_local long long debug_inline_nonconstant_signed_long_long_tls_sub_rhs =
    -1LL;
thread_local long long
debug_inline_nonconstant_signed_long_long_tls_bitwise_lhs =
    -0x0123456789abcdefLL;
thread_local long long
debug_inline_nonconstant_signed_long_long_tls_bitwise_rhs =
    0x70000000fedcba98LL;
thread_local long long debug_inline_nonconstant_signed_long_long_tls_mul_lhs =
    0x200000001LL;
thread_local long long debug_inline_nonconstant_signed_long_long_tls_mul_rhs =
    3LL;
thread_local unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_compare_lhs =
    0x7ffffffffffffff0ULL;
thread_local unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_compare_rhs =
    0xfedcba9876543217ULL;
thread_local long long
debug_inline_nonconstant_signed_long_long_tls_compare_lhs =
    0x7ffffffffffffff0LL;
thread_local long long
debug_inline_nonconstant_signed_long_long_tls_compare_rhs =
    (-0x0123456789abcde9LL);
long long debug_inline_nonconstant_signed_long_long_global_bitwise_lhs =
    -0x0123456789abcdefLL;
long long debug_inline_nonconstant_signed_long_long_global_bitwise_rhs =
    0x70000000fedcba98LL;
long long debug_inline_nonconstant_signed_long_long_global_mul_lhs =
    0x200000001LL;
long long debug_inline_nonconstant_signed_long_long_global_mul_rhs = -3LL;
long long debug_inline_nonconstant_signed_long_long_global_dividend =
    -0x7123456789abcdefLL;
long long debug_inline_nonconstant_signed_long_long_global_divisor =
    -0x100000001LL;
thread_local long long debug_inline_nonconstant_signed_long_long_tls_dividend =
    -0x7123456789abcdefLL;
thread_local long long debug_inline_nonconstant_signed_long_long_tls_divisor =
    -0x100000001LL;
thread_local unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_dividend =
    0xfedcba9876543217ULL;
thread_local unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_divisor =
    0x8000000000000001ULL;
unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_shift_count_source = 4u;
unsigned int
debug_inline_nonconstant_unsigned_long_long_global_shift_count_u32_source = 4u;
unsigned int debug_inline_unsigned_int_widen_source = 0x89abcdefu;
int debug_inline_signed_int_widen_source = -0x1234567;
unsigned char debug_inline_unsigned_char_widen_source = 0xe7u;
signed char debug_inline_signed_char_widen_source = -37;
unsigned short debug_inline_unsigned_short_widen_source = 0xd123u;
short debug_inline_signed_short_widen_source = -0x1234;
bool debug_inline_bool_widen_source = true;
DebugInlineGlobalUnsignedChar debug_inline_global_unsigned_char_widen_source =
    DebugInlineGlobalUnsignedChar::widen_value;
DebugInlineGlobalSignedShort debug_inline_global_signed_short_widen_source =
    DebugInlineGlobalSignedShort::widen_value;
DebugInlineGlobalUnsignedInt debug_inline_global_unsigned_int_widen_source =
    DebugInlineGlobalUnsignedInt::widen_value;
DebugInlineGlobalSignedChar debug_inline_global_signed_char_widen_source =
    DebugInlineGlobalSignedChar::widen_value;
DebugInlineGlobalUnsignedShort
debug_inline_global_unsigned_short_widen_source =
    DebugInlineGlobalUnsignedShort::widen_value;
DebugInlineGlobalSignedInt debug_inline_global_signed_int_widen_source =
    DebugInlineGlobalSignedInt::widen_value;
long long debug_inline_nonconstant_signed_long_long_global_shift_source =
    -0x123456789abcdefLL;
unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_zero_source = 0u;
}

static inline int debug_nonconstant_integer_parameter_inline(int value)
{
    return value;
}

static inline unsigned long long
debug_nonconstant_unsigned_long_long_parameter_inline(
    unsigned long long value)
{
    return value;
}

static inline long long
debug_nonconstant_signed_long_long_parameter_inline(long long value)
{
    return value;
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_pair_divide_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_tls_dividend /
        debug_inline_nonconstant_signed_long_long_tls_divisor);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_pair_remainder_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_tls_dividend %
        debug_inline_nonconstant_signed_long_long_tls_divisor);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_constant_divide_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_tls_dividend / -7LL);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_constant_remainder_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_tls_dividend % -7LL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_pair_divide_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_tls_dividend /
        debug_inline_nonconstant_unsigned_long_long_tls_divisor);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_pair_remainder_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_tls_dividend %
        debug_inline_nonconstant_unsigned_long_long_tls_divisor);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_constant_divide_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_tls_dividend / 7ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_constant_remainder_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_tls_dividend % 7ULL);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_pair_divide_entry(void)
{
    static thread_local long long dividend = -0x7123456789abcdefLL;
    static thread_local long long divisor = -0x100000001LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        dividend / divisor);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_pair_remainder_entry(void)
{
    static thread_local long long dividend = -0x7123456789abcdefLL;
    static thread_local long long divisor = -0x100000001LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        dividend % divisor);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_constant_divide_entry(
    void)
{
    static thread_local long long dividend = -0x7123456789abcdefLL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        dividend / -7LL);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_constant_remainder_entry(
    void)
{
    static thread_local long long dividend = -0x7123456789abcdefLL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        dividend % -7LL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_pair_divide_entry(
    void)
{
    static thread_local unsigned long long dividend =
        0xfedcba9876543217ULL;
    static thread_local unsigned long long divisor =
        0x8000000000000001ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        dividend / divisor);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_pair_remainder_entry(
    void)
{
    static thread_local unsigned long long dividend =
        0xfedcba9876543217ULL;
    static thread_local unsigned long long divisor =
        0x8000000000000001ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        dividend % divisor);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_constant_divide_entry(
    void)
{
    static thread_local unsigned long long dividend =
        0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        dividend / 0x8000000000000001ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_constant_remainder_entry(
    void)
{
    static thread_local unsigned long long dividend =
        0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        dividend % 0x8000000000000001ULL);
}

extern "C" unsigned long long
debug_inline_unsigned_long_long_global_u32_widen_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(debug_inline_unsigned_int_widen_source));
}

extern "C" unsigned long long
debug_inline_unsigned_long_long_global_i32_widen_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(debug_inline_signed_int_widen_source));
}

extern "C" long long
debug_inline_signed_long_long_global_i32_widen_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(debug_inline_signed_int_widen_source));
}

extern "C" long long
debug_inline_signed_long_long_global_u32_widen_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(debug_inline_unsigned_int_widen_source));
}

extern "C" unsigned long long
debug_inline_unsigned_long_long_global_u8_widen_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(debug_inline_unsigned_char_widen_source));
}

extern "C" unsigned long long
debug_inline_unsigned_long_long_global_i8_widen_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(debug_inline_signed_char_widen_source));
}

extern "C" long long
debug_inline_signed_long_long_global_u8_widen_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(debug_inline_unsigned_char_widen_source));
}

extern "C" long long
debug_inline_signed_long_long_global_i8_widen_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(debug_inline_signed_char_widen_source));
}

extern "C" unsigned long long
debug_inline_unsigned_long_long_global_u16_widen_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(debug_inline_unsigned_short_widen_source));
}

extern "C" unsigned long long
debug_inline_unsigned_long_long_global_i16_widen_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(debug_inline_signed_short_widen_source));
}

extern "C" long long
debug_inline_signed_long_long_global_u16_widen_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(debug_inline_unsigned_short_widen_source));
}

extern "C" long long
debug_inline_signed_long_long_global_i16_widen_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(debug_inline_signed_short_widen_source));
}

extern "C" unsigned long long
debug_inline_unsigned_long_long_global_bool_widen_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(debug_inline_bool_widen_source));
}

extern "C" long long
debug_inline_signed_long_long_global_bool_widen_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(debug_inline_bool_widen_source));
}

extern "C" unsigned long long
debug_inline_unsigned_long_long_global_enum_u8_widen_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_global_unsigned_char_widen_source));
}

extern "C" long long
debug_inline_signed_long_long_global_enum_u8_widen_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(
            debug_inline_global_unsigned_char_widen_source));
}

extern "C" unsigned long long
debug_inline_unsigned_long_long_global_enum_i16_widen_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_global_signed_short_widen_source));
}

extern "C" long long
debug_inline_signed_long_long_global_enum_i16_widen_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(debug_inline_global_signed_short_widen_source));
}

extern "C" unsigned long long
debug_inline_unsigned_long_long_global_enum_u32_widen_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_global_unsigned_int_widen_source));
}

extern "C" long long
debug_inline_signed_long_long_global_enum_u32_widen_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(debug_inline_global_unsigned_int_widen_source));
}

extern "C" unsigned long long
debug_inline_unsigned_long_long_global_enum_i8_widen_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_global_signed_char_widen_source));
}

extern "C" long long
debug_inline_signed_long_long_global_enum_i8_widen_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(debug_inline_global_signed_char_widen_source));
}

extern "C" unsigned long long
debug_inline_unsigned_long_long_global_enum_u16_widen_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_global_unsigned_short_widen_source));
}

extern "C" long long
debug_inline_signed_long_long_global_enum_u16_widen_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(
            debug_inline_global_unsigned_short_widen_source));
}

extern "C" unsigned long long
debug_inline_unsigned_long_long_global_enum_i32_widen_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_global_signed_int_widen_source));
}

extern "C" long long
debug_inline_signed_long_long_global_enum_i32_widen_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(debug_inline_global_signed_int_widen_source));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_add_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source +
        0x100000005ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_sub_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source -
        0x100000005ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_commuted_add_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        0x100000005ULL +
        debug_inline_nonconstant_unsigned_long_long_global_add_source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_and_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source &
        0x100000005ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_or_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source |
        0x100000005ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_xor_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source ^
        0x100000005ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_mul_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source *
        0x100000005ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_rshift_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source >> 3u);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_lshift_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source << 33u);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_rshift32_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source >> 32u);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_lshift32_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source << 32u);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_rshift0_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source >> 0u);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_lshift0_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source << 0u);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_div_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source /
        8ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_mod_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source %
        8ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_div7_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source / 7ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_mod7_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source % 7ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_div_large32_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source /
        1073741825ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_mod_large32_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source %
        1073741825ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_div_high32_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source /
        2147483649ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_mod_high32_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source %
        2147483649ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_div_high64_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source /
        0x8000000000000001ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_mod_high64_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source %
        0x8000000000000001ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_div_mid64_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source /
        0x100000001ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_mod_mid64_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source %
        0x100000001ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_div_large_mid64_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source /
        0x4000000000000001ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_mod_large_mid64_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source %
        0x4000000000000001ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_div_int64_max_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source /
        0x7fffffffffffffffULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_mod_int64_max_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source %
        0x7fffffffffffffffULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_div_max64_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source /
        0xffffffffffffffffULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_mod_max64_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source %
        0xffffffffffffffffULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_rshift63_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source >>
        63u);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_lshift1_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source <<
        1u);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_not_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        ~debug_inline_nonconstant_unsigned_long_long_global_mod_source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_neg_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        -debug_inline_nonconstant_unsigned_long_long_global_mod_source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_lnot_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            !debug_inline_nonconstant_unsigned_long_long_global_mod_source));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_lnot_zero_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            !debug_inline_nonconstant_unsigned_long_long_global_zero_source));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_eq_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_global_mod_source ==
            0xfedcba9876543217ULL));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_ne_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_global_mod_source !=
            0xfedcba9876543217ULL));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_lt_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_global_mod_source <
            0x8000000000000000ULL));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_le_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_global_mod_source <=
            0xffffffffffffffffULL));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_gt_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_global_mod_source >
            0x8000000000000000ULL));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_ge_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_global_mod_source >=
            0xffffffffffffffffULL));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_mul7_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source * 7ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_mul9_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source * 9ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_dynamic_lshift_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source <<
        debug_inline_nonconstant_unsigned_long_long_global_shift_count_source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_dynamic_rshift_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source >>
        debug_inline_nonconstant_unsigned_long_long_global_shift_count_source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_dynamic_lshift_u32_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source <<
        debug_inline_nonconstant_unsigned_long_long_global_shift_count_u32_source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_dynamic_rshift_u32_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_mod_source >>
        debug_inline_nonconstant_unsigned_long_long_global_shift_count_u32_source);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_global_dynamic_rshift_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_global_shift_source >>
        debug_inline_nonconstant_unsigned_long_long_global_shift_count_source);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_global_dynamic_rshift_u32_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_global_shift_source >>
        debug_inline_nonconstant_unsigned_long_long_global_shift_count_u32_source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_pair_add_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source +
        debug_inline_nonconstant_unsigned_long_long_global_mod_source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_pair_mul_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source *
        debug_inline_nonconstant_unsigned_long_long_global_mod_source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_pair_sub_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source -
        debug_inline_nonconstant_unsigned_long_long_global_mod_source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_pair_divide_entry(void)
{
    static unsigned long long dividend = 0xfedcba9876543217ULL;
    static unsigned long long divisor = 0x8000000000000001ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        dividend / divisor);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_pair_remainder_entry(void)
{
    static unsigned long long dividend = 0xfedcba9876543217ULL;
    static unsigned long long divisor = 0x8000000000000001ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        dividend % divisor);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_constant_divide_entry(void)
{
    static unsigned long long dividend = 0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        dividend / 0x8000000000000001ULL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_constant_remainder_entry(void)
{
    static unsigned long long dividend = 0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        dividend % 0x8000000000000001ULL);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_global_pair_add_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_global_add_lhs +
        debug_inline_nonconstant_signed_long_long_global_add_rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_global_pair_sub_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_global_sub_lhs -
        debug_inline_nonconstant_signed_long_long_global_sub_rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_pair_add_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_tls_add_lhs +
        debug_inline_nonconstant_signed_long_long_tls_add_rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_pair_sub_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_tls_sub_lhs -
        debug_inline_nonconstant_signed_long_long_tls_sub_rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_pair_and_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_tls_bitwise_lhs &
        debug_inline_nonconstant_signed_long_long_tls_bitwise_rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_pair_or_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_tls_bitwise_lhs |
        debug_inline_nonconstant_signed_long_long_tls_bitwise_rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_pair_xor_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_tls_bitwise_lhs ^
        debug_inline_nonconstant_signed_long_long_tls_bitwise_rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_pair_mul_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_tls_mul_lhs *
        debug_inline_nonconstant_signed_long_long_tls_mul_rhs);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_pair_lt_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_tls_compare_lhs <
            debug_inline_nonconstant_unsigned_long_long_tls_compare_rhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_pair_le_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_tls_compare_lhs <=
            debug_inline_nonconstant_unsigned_long_long_tls_compare_rhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_pair_gt_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_tls_compare_rhs >
            debug_inline_nonconstant_unsigned_long_long_tls_compare_lhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_pair_ge_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_tls_compare_rhs >=
            debug_inline_nonconstant_unsigned_long_long_tls_compare_lhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_pair_eq_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_tls_compare_lhs ==
            debug_inline_nonconstant_unsigned_long_long_tls_compare_rhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_pair_ne_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_tls_compare_lhs !=
            debug_inline_nonconstant_unsigned_long_long_tls_compare_rhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_pair_lt_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(
            debug_inline_nonconstant_signed_long_long_tls_compare_lhs <
            debug_inline_nonconstant_signed_long_long_tls_compare_rhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_pair_le_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(
            debug_inline_nonconstant_signed_long_long_tls_compare_lhs <=
            debug_inline_nonconstant_signed_long_long_tls_compare_rhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_pair_gt_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(
            debug_inline_nonconstant_signed_long_long_tls_compare_rhs >
            debug_inline_nonconstant_signed_long_long_tls_compare_lhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_pair_ge_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(
            debug_inline_nonconstant_signed_long_long_tls_compare_rhs >=
            debug_inline_nonconstant_signed_long_long_tls_compare_lhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_pair_eq_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(
            debug_inline_nonconstant_signed_long_long_tls_compare_lhs ==
            debug_inline_nonconstant_signed_long_long_tls_compare_rhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_pair_ne_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(
            debug_inline_nonconstant_signed_long_long_tls_compare_lhs !=
            debug_inline_nonconstant_signed_long_long_tls_compare_rhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_pair_lt_entry(void)
{
    static thread_local unsigned long long
        lhs =
            0x7ffffffffffffff0ULL;
    static thread_local unsigned long long
        rhs =
            0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            lhs <
            rhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_pair_le_entry(void)
{
    static thread_local unsigned long long
        lhs =
            0x7ffffffffffffff0ULL;
    static thread_local unsigned long long
        rhs =
            0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            lhs <=
            rhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_pair_gt_entry(void)
{
    static thread_local unsigned long long
        lhs =
            0x7ffffffffffffff0ULL;
    static thread_local unsigned long long
        rhs =
            0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            rhs >
            lhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_pair_ge_entry(void)
{
    static thread_local unsigned long long
        lhs =
            0x7ffffffffffffff0ULL;
    static thread_local unsigned long long
        rhs =
            0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            rhs >=
            lhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_pair_eq_entry(void)
{
    static thread_local unsigned long long
        lhs =
            0x7ffffffffffffff0ULL;
    static thread_local unsigned long long
        rhs =
            0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            lhs ==
            rhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_pair_ne_entry(void)
{
    static thread_local unsigned long long
        lhs =
            0x7ffffffffffffff0ULL;
    static thread_local unsigned long long
        rhs =
            0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            lhs !=
            rhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_pair_lt_entry(void)
{
    static thread_local long long
        lhs =
            0x7ffffffffffffff0LL;
    static thread_local long long
        rhs =
            -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(
            lhs <
            rhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_pair_le_entry(void)
{
    static thread_local long long
        lhs =
            0x7ffffffffffffff0LL;
    static thread_local long long
        rhs =
            -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(
            lhs <=
            rhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_pair_gt_entry(void)
{
    static thread_local long long
        lhs =
            0x7ffffffffffffff0LL;
    static thread_local long long
        rhs =
            -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(
            rhs >
            lhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_pair_ge_entry(void)
{
    static thread_local long long
        lhs =
            0x7ffffffffffffff0LL;
    static thread_local long long
        rhs =
            -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(
            rhs >=
            lhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_pair_eq_entry(void)
{
    static thread_local long long
        lhs =
            0x7ffffffffffffff0LL;
    static thread_local long long
        rhs =
            -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(
            lhs ==
            rhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_pair_ne_entry(void)
{
    static thread_local long long
        lhs =
            0x7ffffffffffffff0LL;
    static thread_local long long
        rhs =
            -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(
            lhs !=
            rhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_pair_add_entry(void)
{
    static thread_local long long lhs = 0x1ffffffffLL;
    static thread_local long long rhs = 1LL;
    return debug_nonconstant_signed_long_long_parameter_inline(lhs + rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_pair_sub_entry(void)
{
    static thread_local long long lhs = 0x100000000LL;
    static thread_local long long rhs = 0x100000001LL;
    return debug_nonconstant_signed_long_long_parameter_inline(lhs - rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_pair_and_entry(void)
{
    static thread_local long long lhs = -0x0123456789abcdefLL;
    static thread_local long long rhs = 0x70000000fedcba98LL;
    return debug_nonconstant_signed_long_long_parameter_inline(lhs & rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_pair_or_entry(void)
{
    static thread_local long long lhs = -0x0123456789abcdefLL;
    static thread_local long long rhs = 0x70000000fedcba98LL;
    return debug_nonconstant_signed_long_long_parameter_inline(lhs | rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_pair_xor_entry(void)
{
    static thread_local long long lhs = -0x0123456789abcdefLL;
    static thread_local long long rhs = 0x70000000fedcba98LL;
    return debug_nonconstant_signed_long_long_parameter_inline(lhs ^ rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_pair_mul_entry(void)
{
    static thread_local long long lhs = 0x200000001LL;
    static thread_local long long rhs = 3LL;
    return debug_nonconstant_signed_long_long_parameter_inline(lhs * rhs);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_pair_add_entry(void)
{
    static thread_local unsigned long long lhs = 0x1ffffffffULL;
    static thread_local unsigned long long rhs = 1ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(lhs + rhs);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_pair_sub_entry(void)
{
    static thread_local unsigned long long lhs = 0x100000000ULL;
    static thread_local unsigned long long rhs = 0x100000001ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(lhs - rhs);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_pair_and_entry(void)
{
    static thread_local unsigned long long lhs = 0xfedcba9876543211ULL;
    static thread_local unsigned long long rhs = 0x70000000fedcba98ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(lhs & rhs);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_pair_or_entry(void)
{
    static thread_local unsigned long long lhs = 0xfedcba9876543211ULL;
    static thread_local unsigned long long rhs = 0x70000000fedcba98ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(lhs | rhs);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_pair_xor_entry(void)
{
    static thread_local unsigned long long lhs = 0xfedcba9876543211ULL;
    static thread_local unsigned long long rhs = 0x70000000fedcba98ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(lhs ^ rhs);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_pair_mul_entry(void)
{
    static thread_local unsigned long long lhs = 0x200000001ULL;
    static thread_local unsigned long long rhs = 3ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(lhs * rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_pair_add_entry(void)
{
    static long long lhs = 0x1ffffffffLL;
    static long long rhs = 1LL;
    return debug_nonconstant_signed_long_long_parameter_inline(lhs + rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_pair_sub_entry(void)
{
    static long long lhs = 0x100000000LL;
    static long long rhs = 0x100000001LL;
    return debug_nonconstant_signed_long_long_parameter_inline(lhs - rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_pair_and_entry(void)
{
    static long long lhs = -0x0123456789abcdefLL;
    static long long rhs = 0x70000000fedcba98LL;
    return debug_nonconstant_signed_long_long_parameter_inline(lhs & rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_pair_or_entry(void)
{
    static long long lhs = -0x0123456789abcdefLL;
    static long long rhs = 0x70000000fedcba98LL;
    return debug_nonconstant_signed_long_long_parameter_inline(lhs | rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_pair_xor_entry(void)
{
    static long long lhs = -0x0123456789abcdefLL;
    static long long rhs = 0x70000000fedcba98LL;
    return debug_nonconstant_signed_long_long_parameter_inline(lhs ^ rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_pair_mul_entry(void)
{
    static long long lhs = 0x200000001LL;
    static long long rhs = 3LL;
    return debug_nonconstant_signed_long_long_parameter_inline(lhs * rhs);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_pair_add_entry(void)
{
    static unsigned long long lhs = 0x1ffffffffULL;
    static unsigned long long rhs = 1ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(lhs + rhs);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_pair_sub_entry(void)
{
    static unsigned long long lhs = 0x100000000ULL;
    static unsigned long long rhs = 0x100000001ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(lhs - rhs);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_pair_and_entry(void)
{
    static unsigned long long lhs = 0xfedcba9876543211ULL;
    static unsigned long long rhs = 0x70000000fedcba98ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(lhs & rhs);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_pair_or_entry(void)
{
    static unsigned long long lhs = 0xfedcba9876543211ULL;
    static unsigned long long rhs = 0x70000000fedcba98ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(lhs | rhs);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_pair_xor_entry(void)
{
    static unsigned long long lhs = 0xfedcba9876543211ULL;
    static unsigned long long rhs = 0x70000000fedcba98ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(lhs ^ rhs);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_pair_mul_entry(void)
{
    static unsigned long long lhs = 0x200000001ULL;
    static unsigned long long rhs = 3ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(lhs * rhs);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_pair_lt_entry(void)
{
    static unsigned long long lhs = 0x7ffffffffffffff0ULL;
    static unsigned long long rhs = 0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(lhs < rhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_pair_le_entry(void)
{
    static unsigned long long lhs = 0x7ffffffffffffff0ULL;
    static unsigned long long rhs = 0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(lhs <= rhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_pair_gt_entry(void)
{
    static unsigned long long lhs = 0x7ffffffffffffff0ULL;
    static unsigned long long rhs = 0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(rhs > lhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_pair_ge_entry(void)
{
    static unsigned long long lhs = 0x7ffffffffffffff0ULL;
    static unsigned long long rhs = 0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(rhs >= lhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_pair_eq_entry(void)
{
    static unsigned long long lhs = 0x7ffffffffffffff0ULL;
    static unsigned long long rhs = 0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(lhs == rhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_pair_ne_entry(void)
{
    static unsigned long long lhs = 0x7ffffffffffffff0ULL;
    static unsigned long long rhs = 0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(lhs != rhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_pair_lt_entry(void)
{
    static long long lhs = 0x7ffffffffffffff0LL;
    static long long rhs = -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(lhs < rhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_pair_le_entry(void)
{
    static long long lhs = 0x7ffffffffffffff0LL;
    static long long rhs = -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(lhs <= rhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_pair_gt_entry(void)
{
    static long long lhs = 0x7ffffffffffffff0LL;
    static long long rhs = -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(rhs > lhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_pair_ge_entry(void)
{
    static long long lhs = 0x7ffffffffffffff0LL;
    static long long rhs = -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(rhs >= lhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_pair_eq_entry(void)
{
    static long long lhs = 0x7ffffffffffffff0LL;
    static long long rhs = -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(lhs == rhs));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_pair_ne_entry(void)
{
    static long long lhs = 0x7ffffffffffffff0LL;
    static long long rhs = -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(lhs != rhs));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_neg_entry(void)
{
    static unsigned long long source = 0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(-source);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_neg_entry(void)
{
    static long long source = -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(-source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_neg_entry(void)
{
    static thread_local unsigned long long source =
        0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(-source);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_neg_entry(void)
{
    static thread_local long long source = -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(-source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_bitnot_entry(void)
{
    static unsigned long long source = 0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(~source);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_bitnot_entry(void)
{
    static long long source = -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(~source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_bitnot_entry(void)
{
    static thread_local unsigned long long source =
        0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(~source);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_bitnot_entry(void)
{
    static thread_local long long source = -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(~source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_static_lnot_entry(void)
{
    static unsigned long long source = 0xfedcba9876543217ULL;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(!source));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_static_lnot_zero_entry(void)
{
    static thread_local unsigned long long source = 0u;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(!source));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_lnot_entry(void)
{
    static long long source = -0x0123456789abcde9LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(!source));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_tls_static_lnot_zero_entry(void)
{
    static thread_local long long source = 0;
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(!source));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_global_pair_and_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_global_bitwise_lhs &
        debug_inline_nonconstant_signed_long_long_global_bitwise_rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_global_pair_or_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_global_bitwise_lhs |
        debug_inline_nonconstant_signed_long_long_global_bitwise_rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_global_pair_xor_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_global_bitwise_lhs ^
        debug_inline_nonconstant_signed_long_long_global_bitwise_rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_global_pair_mul_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_global_mul_lhs *
        debug_inline_nonconstant_signed_long_long_global_mul_rhs);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_global_pair_divide_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_global_dividend /
        debug_inline_nonconstant_signed_long_long_global_divisor);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_global_pair_remainder_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_global_dividend %
        debug_inline_nonconstant_signed_long_long_global_divisor);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_pair_divide_entry(void)
{
    static long long dividend = -0x7123456789abcdefLL;
    static long long divisor = -0x100000001LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        dividend / divisor);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_pair_remainder_entry(void)
{
    static long long dividend = -0x7123456789abcdefLL;
    static long long divisor = -0x100000001LL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        dividend % divisor);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_constant_divide_entry(void)
{
    static long long dividend = -0x7123456789abcdefLL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        dividend / -7LL);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_static_constant_remainder_entry(void)
{
    static long long dividend = -0x7123456789abcdefLL;
    return debug_nonconstant_signed_long_long_parameter_inline(
        dividend % -7LL);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_negated_global_dividend_divide_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        -debug_inline_nonconstant_signed_long_long_global_dividend /
        debug_inline_nonconstant_signed_long_long_global_divisor);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_negated_global_dividend_remainder_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        -debug_inline_nonconstant_signed_long_long_global_dividend %
        debug_inline_nonconstant_signed_long_long_global_divisor);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_negated_global_divisor_divide_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_global_dividend /
        -debug_inline_nonconstant_signed_long_long_global_divisor);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_negated_global_divisor_remainder_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_global_dividend %
        -debug_inline_nonconstant_signed_long_long_global_divisor);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_cast_global_dividend_divide_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        static_cast<long long>(
            debug_inline_nonconstant_signed_long_long_global_dividend) /
        debug_inline_nonconstant_signed_long_long_global_divisor);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_cast_global_divisor_remainder_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_global_dividend %
        static_cast<long long>(
            debug_inline_nonconstant_signed_long_long_global_divisor));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_positive_cast_global_dividend_divide_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        +static_cast<long long>(
            debug_inline_nonconstant_signed_long_long_global_dividend) /
        debug_inline_nonconstant_signed_long_long_global_divisor);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_double_negated_cast_global_dividend_divide_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        -(-static_cast<long long>(
            debug_inline_nonconstant_signed_long_long_global_dividend)) /
        debug_inline_nonconstant_signed_long_long_global_divisor);
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_triple_negated_cast_global_divisor_remainder_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_global_dividend %
        -(-(-static_cast<long long>(
            debug_inline_nonconstant_signed_long_long_global_divisor))));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_global_divide_by_negative_seven_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        +static_cast<long long>(
            debug_inline_nonconstant_signed_long_long_global_dividend) /
        static_cast<long long>(-7LL));
}

extern "C" long long
debug_inline_nonconstant_signed_long_long_global_remainder_by_negative_seven_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        +static_cast<long long>(
            debug_inline_nonconstant_signed_long_long_global_dividend) %
        -7LL);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_pair_xor_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source ^
        debug_inline_nonconstant_unsigned_long_long_global_mod_source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_pair_and_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source &
        debug_inline_nonconstant_unsigned_long_long_global_mod_source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_pair_or_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source |
        debug_inline_nonconstant_unsigned_long_long_global_mod_source);
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_pair_lt_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_global_add_source <
            debug_inline_nonconstant_unsigned_long_long_global_mod_source));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_pair_eq_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_global_add_source ==
            debug_inline_nonconstant_unsigned_long_long_global_mod_source));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_pair_gt_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_global_mod_source >
            debug_inline_nonconstant_unsigned_long_long_global_add_source));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_pair_ge_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_global_mod_source >=
            debug_inline_nonconstant_unsigned_long_long_global_add_source));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_pair_le_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_global_add_source <=
            debug_inline_nonconstant_unsigned_long_long_global_mod_source));
}

extern "C" unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_pair_ne_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        static_cast<unsigned long long>(
            debug_inline_nonconstant_unsigned_long_long_global_add_source !=
            debug_inline_nonconstant_unsigned_long_long_global_mod_source));
}

long long* debug_inline_signed_long_long_comparison_escape;
long long* debug_inline_signed_long_long_comparison_rhs_escape;
unsigned long long* debug_inline_unsigned_long_long_comparison_escape;
unsigned long long* debug_inline_unsigned_long_long_comparison_rhs_escape;
int* debug_inline_signed_int_comparison_escape;
unsigned int* debug_inline_unsigned_int_comparison_escape;
signed char* debug_inline_signed_char_comparison_escape;
unsigned char* debug_inline_unsigned_char_comparison_escape;
short* debug_inline_signed_short_comparison_escape;
unsigned short* debug_inline_unsigned_short_comparison_escape;
bool* debug_inline_bool_comparison_escape;

extern "C" int debug_inline_unsigned_enum_long_compare_entry(
    unsigned int value)
{
    DebugInlineUnsignedComparisonEnum
        debug_inline_unsigned_enum_long_compare_entry_source =
            static_cast<DebugInlineUnsignedComparisonEnum>(value);
    debug_inline_unsigned_comparison_enum_escape =
        &debug_inline_unsigned_enum_long_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_enum_long_compare_entry_source < -1L));
}

extern "C" int debug_inline_unsigned_char_enum_int_compare_entry(
    unsigned char value)
{
    DebugInlineUnsignedCharComparisonEnum
        debug_inline_unsigned_char_enum_int_compare_entry_source =
            static_cast<DebugInlineUnsignedCharComparisonEnum>(value);
    debug_inline_unsigned_char_comparison_enum_escape =
        &debug_inline_unsigned_char_enum_int_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_char_enum_int_compare_entry_source < -1));
}

extern "C" int debug_inline_unsigned_short_enum_long_compare_entry(
    unsigned short value)
{
    DebugInlineUnsignedShortComparisonEnum
        debug_inline_unsigned_short_enum_long_compare_entry_source =
            static_cast<DebugInlineUnsignedShortComparisonEnum>(value);
    debug_inline_unsigned_short_comparison_enum_escape =
        &debug_inline_unsigned_short_enum_long_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_short_enum_long_compare_entry_source < -1L));
}

extern "C" int debug_inline_signed_char_enum_unsigned_int_compare_entry(
    signed char value)
{
    DebugInlineSignedCharComparisonEnum
        debug_inline_signed_char_enum_unsigned_int_compare_entry_source =
            static_cast<DebugInlineSignedCharComparisonEnum>(value);
    debug_inline_signed_char_comparison_enum_escape =
        &debug_inline_signed_char_enum_unsigned_int_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_char_enum_unsigned_int_compare_entry_source <
          0x80000000u));
}

extern "C" int debug_inline_unsigned_char_enum_signed_char_enum_compare_entry(
    unsigned char value)
{
    DebugInlineUnsignedCharComparisonEnum
        debug_inline_unsigned_char_enum_signed_char_enum_compare_entry_source =
            static_cast<DebugInlineUnsignedCharComparisonEnum>(value);
    debug_inline_unsigned_char_comparison_enum_escape =
        &debug_inline_unsigned_char_enum_signed_char_enum_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_char_enum_signed_char_enum_compare_entry_source <
          debug_inline_signed_char_comparison_enum_negative));
}

extern "C" int debug_inline_signed_long_long_compare_entry(long long value)
{
    long long debug_inline_signed_long_long_compare_entry_source = value;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_compare_entry_source < 0LL));
}

extern "C" int debug_inline_signed_long_long_conditional_compare_entry(
    long long value)
{
    long long debug_inline_signed_long_long_conditional_compare_entry_source =
        value;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_conditional_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !((((debug_inline_signed_long_long_conditional_compare_entry_source &&
             1LL) ||
            (debug_inline_signed_long_long_conditional_compare_entry_source &&
             0LL))
               ? debug_inline_signed_long_long_conditional_compare_entry_source
               : -1LL) < 0LL));
}

extern "C" int debug_inline_unsigned_long_long_compare_entry(
    unsigned long long value)
{
    unsigned long long debug_inline_unsigned_long_long_compare_entry_source =
        value;
    debug_inline_unsigned_long_long_comparison_escape =
        &debug_inline_unsigned_long_long_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_long_long_compare_entry_source <
          0x8000000000000000ULL));
}

extern "C" int debug_inline_signed_long_long_less_equal_entry(
    long long value)
{
    long long debug_inline_signed_long_long_less_equal_entry_source = value;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_less_equal_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_less_equal_entry_source <= 0LL));
}

extern "C" int debug_inline_signed_long_long_greater_entry(long long value)
{
    long long debug_inline_signed_long_long_greater_entry_source = value;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_greater_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_greater_entry_source > 0LL));
}

extern "C" int debug_inline_unsigned_long_long_greater_equal_entry(
    unsigned long long value)
{
    unsigned long long debug_inline_unsigned_long_long_greater_equal_entry_source =
        value;
    debug_inline_unsigned_long_long_comparison_escape =
        &debug_inline_unsigned_long_long_greater_equal_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_long_long_greater_equal_entry_source >=
          0x8000000000000000ULL));
}

extern "C" int debug_inline_unsigned_long_long_equal_entry(
    unsigned long long value)
{
    unsigned long long debug_inline_unsigned_long_long_equal_entry_source =
        value;
    debug_inline_unsigned_long_long_comparison_escape =
        &debug_inline_unsigned_long_long_equal_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_long_long_equal_entry_source ==
          0x8000000000000000ULL));
}

extern "C" int debug_inline_unsigned_long_long_not_equal_entry(
    unsigned long long value)
{
    unsigned long long debug_inline_unsigned_long_long_not_equal_entry_source =
        value;
    debug_inline_unsigned_long_long_comparison_escape =
        &debug_inline_unsigned_long_long_not_equal_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_long_long_not_equal_entry_source !=
          0x8000000000000000ULL));
}

extern "C" int debug_inline_signed_long_long_unsigned_int_compare_entry(
    long long value)
{
    long long debug_inline_signed_long_long_unsigned_int_compare_entry_source =
        value;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_unsigned_int_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_unsigned_int_compare_entry_source <
          0x80000000u));
}

extern "C" int debug_inline_unsigned_long_long_signed_int_compare_entry(
    unsigned long long value)
{
    unsigned long long
        debug_inline_unsigned_long_long_signed_int_compare_entry_source = value;
    debug_inline_unsigned_long_long_comparison_escape =
        &debug_inline_unsigned_long_long_signed_int_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_long_long_signed_int_compare_entry_source <
          -1));
}

extern "C" int debug_inline_signed_long_long_int_compare_entry(
    long long value, int rhs)
{
    long long debug_inline_signed_long_long_int_compare_entry_source = value;
    int debug_inline_signed_long_long_int_compare_rhs = rhs;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_int_compare_entry_source;
    debug_inline_signed_int_comparison_escape =
        &debug_inline_signed_long_long_int_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_int_compare_entry_source <
          debug_inline_signed_long_long_int_compare_rhs));
}

extern "C" int debug_inline_unsigned_long_long_unsigned_int_compare_entry(
    unsigned long long value, unsigned int rhs)
{
    unsigned long long
        debug_inline_unsigned_long_long_unsigned_int_compare_entry_source =
            value;
    unsigned int debug_inline_unsigned_long_long_unsigned_int_compare_rhs =
        rhs;
    debug_inline_unsigned_long_long_comparison_escape =
        &debug_inline_unsigned_long_long_unsigned_int_compare_entry_source;
    debug_inline_unsigned_int_comparison_escape =
        &debug_inline_unsigned_long_long_unsigned_int_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_long_long_unsigned_int_compare_entry_source <
          debug_inline_unsigned_long_long_unsigned_int_compare_rhs));
}

extern "C" int debug_inline_signed_long_long_signed_char_compare_entry(
    long long value, signed char rhs)
{
    long long debug_inline_signed_long_long_signed_char_compare_entry_source =
        value;
    signed char debug_inline_signed_long_long_signed_char_compare_rhs = rhs;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_signed_char_compare_entry_source;
    debug_inline_signed_char_comparison_escape =
        &debug_inline_signed_long_long_signed_char_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_signed_char_compare_entry_source <
          debug_inline_signed_long_long_signed_char_compare_rhs));
}

extern "C" int debug_inline_signed_long_long_unsigned_char_compare_entry(
    long long value, unsigned char rhs)
{
    long long
        debug_inline_signed_long_long_unsigned_char_compare_entry_source =
            value;
    unsigned char debug_inline_signed_long_long_unsigned_char_compare_rhs =
        rhs;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_unsigned_char_compare_entry_source;
    debug_inline_unsigned_char_comparison_escape =
        &debug_inline_signed_long_long_unsigned_char_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_unsigned_char_compare_entry_source <
          debug_inline_signed_long_long_unsigned_char_compare_rhs));
}

extern "C" int debug_inline_unsigned_long_long_signed_short_compare_entry(
    unsigned long long value, short rhs)
{
    unsigned long long
        debug_inline_unsigned_long_long_signed_short_compare_entry_source =
            value;
    short debug_inline_unsigned_long_long_signed_short_compare_rhs = rhs;
    debug_inline_unsigned_long_long_comparison_escape =
        &debug_inline_unsigned_long_long_signed_short_compare_entry_source;
    debug_inline_signed_short_comparison_escape =
        &debug_inline_unsigned_long_long_signed_short_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_long_long_signed_short_compare_entry_source <
          debug_inline_unsigned_long_long_signed_short_compare_rhs));
}

extern "C" int debug_inline_unsigned_long_long_unsigned_short_compare_entry(
    unsigned long long value, unsigned short rhs)
{
    unsigned long long
        debug_inline_unsigned_long_long_unsigned_short_compare_entry_source =
            value;
    unsigned short debug_inline_unsigned_long_long_unsigned_short_compare_rhs =
        rhs;
    debug_inline_unsigned_long_long_comparison_escape =
        &debug_inline_unsigned_long_long_unsigned_short_compare_entry_source;
    debug_inline_unsigned_short_comparison_escape =
        &debug_inline_unsigned_long_long_unsigned_short_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_long_long_unsigned_short_compare_entry_source <
          debug_inline_unsigned_long_long_unsigned_short_compare_rhs));
}

extern "C" int debug_inline_signed_long_long_bool_compare_entry(
    long long value, bool rhs)
{
    long long debug_inline_signed_long_long_bool_compare_entry_source = value;
    bool debug_inline_signed_long_long_bool_compare_rhs = rhs;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_bool_compare_entry_source;
    debug_inline_bool_comparison_escape =
        &debug_inline_signed_long_long_bool_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_bool_compare_entry_source <
          debug_inline_signed_long_long_bool_compare_rhs));
}

extern "C" int debug_inline_unsigned_long_long_signed_char_enum_compare_entry(
    unsigned long long value, signed char rhs)
{
    unsigned long long
        debug_inline_unsigned_long_long_signed_char_enum_compare_entry_source =
            value;
    DebugInlineSignedCharComparisonEnum
        debug_inline_unsigned_long_long_signed_char_enum_compare_rhs =
            static_cast<DebugInlineSignedCharComparisonEnum>(rhs);
    debug_inline_unsigned_long_long_comparison_escape =
        &debug_inline_unsigned_long_long_signed_char_enum_compare_entry_source;
    debug_inline_signed_char_comparison_enum_escape =
        &debug_inline_unsigned_long_long_signed_char_enum_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_long_long_signed_char_enum_compare_entry_source <
          debug_inline_unsigned_long_long_signed_char_enum_compare_rhs));
}

extern "C" int debug_inline_signed_long_long_unsigned_short_enum_compare_entry(
    long long value, unsigned short rhs)
{
    long long
        debug_inline_signed_long_long_unsigned_short_enum_compare_entry_source =
            value;
    DebugInlineUnsignedShortComparisonEnum
        debug_inline_signed_long_long_unsigned_short_enum_compare_rhs =
            static_cast<DebugInlineUnsignedShortComparisonEnum>(rhs);
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_unsigned_short_enum_compare_entry_source;
    debug_inline_unsigned_short_comparison_enum_escape =
        &debug_inline_signed_long_long_unsigned_short_enum_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_unsigned_short_enum_compare_entry_source <
          debug_inline_signed_long_long_unsigned_short_enum_compare_rhs));
}

extern "C" int debug_inline_signed_long_long_unsigned_int_enum_compare_entry(
    long long value, unsigned int rhs)
{
    long long
        debug_inline_signed_long_long_unsigned_int_enum_compare_entry_source =
            value;
    DebugInlineUnsignedComparisonEnum
        debug_inline_signed_long_long_unsigned_int_enum_compare_rhs =
            static_cast<DebugInlineUnsignedComparisonEnum>(rhs);
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_unsigned_int_enum_compare_entry_source;
    debug_inline_unsigned_comparison_enum_escape =
        &debug_inline_signed_long_long_unsigned_int_enum_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_unsigned_int_enum_compare_entry_source <
          debug_inline_signed_long_long_unsigned_int_enum_compare_rhs));
}

extern "C" int debug_inline_signed_long_long_unsigned_char_enum_compare_entry(
    long long value, unsigned char rhs)
{
    long long
        debug_inline_signed_long_long_unsigned_char_enum_compare_entry_source =
            value;
    DebugInlineUnsignedCharComparisonEnum
        debug_inline_signed_long_long_unsigned_char_enum_compare_rhs =
            static_cast<DebugInlineUnsignedCharComparisonEnum>(rhs);
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_unsigned_char_enum_compare_entry_source;
    debug_inline_unsigned_char_comparison_enum_escape =
        &debug_inline_signed_long_long_unsigned_char_enum_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_unsigned_char_enum_compare_entry_source <
          debug_inline_signed_long_long_unsigned_char_enum_compare_rhs));
}

extern "C" int debug_inline_signed_long_long_signed_short_enum_compare_entry(
    long long value, short rhs)
{
    long long
        debug_inline_signed_long_long_signed_short_enum_compare_entry_source =
            value;
    DebugInlineSignedShortComparisonEnum
        debug_inline_signed_long_long_signed_short_enum_compare_rhs =
            static_cast<DebugInlineSignedShortComparisonEnum>(rhs);
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_signed_short_enum_compare_entry_source;
    debug_inline_signed_short_comparison_enum_escape =
        &debug_inline_signed_long_long_signed_short_enum_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_signed_short_enum_compare_entry_source <
          debug_inline_signed_long_long_signed_short_enum_compare_rhs));
}

extern "C" int debug_inline_signed_long_long_signed_int_enum_compare_entry(
    long long value, int rhs)
{
    long long
        debug_inline_signed_long_long_signed_int_enum_compare_entry_source =
            value;
    DebugInlineSignedComparisonEnum
        debug_inline_signed_long_long_signed_int_enum_compare_rhs =
            static_cast<DebugInlineSignedComparisonEnum>(rhs);
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_signed_int_enum_compare_entry_source;
    debug_inline_signed_comparison_enum_escape =
        &debug_inline_signed_long_long_signed_int_enum_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_signed_int_enum_compare_entry_source <
          debug_inline_signed_long_long_signed_int_enum_compare_rhs));
}

extern "C" int
debug_inline_signed_long_long_scoped_unsigned_char_enum_compare_entry(
    long long value, unsigned char rhs)
{
    long long debug_inline_signed_long_long_scoped_unsigned_char_enum_compare_entry_source =
        value;
    DebugInlineScopedUnsignedCharComparisonEnum
        debug_inline_signed_long_long_scoped_unsigned_char_enum_compare_rhs =
            static_cast<DebugInlineScopedUnsignedCharComparisonEnum>(rhs);
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_scoped_unsigned_char_enum_compare_entry_source;
    debug_inline_scoped_unsigned_char_comparison_enum_escape =
        &debug_inline_signed_long_long_scoped_unsigned_char_enum_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_scoped_unsigned_char_enum_compare_entry_source <
          static_cast<long long>(
              debug_inline_signed_long_long_scoped_unsigned_char_enum_compare_rhs)));
}

extern "C" int
debug_inline_signed_long_long_scoped_signed_short_enum_compare_entry(
    long long value, short rhs)
{
    long long debug_inline_signed_long_long_scoped_signed_short_enum_compare_entry_source =
        value;
    DebugInlineScopedSignedShortComparisonEnum
        debug_inline_signed_long_long_scoped_signed_short_enum_compare_rhs =
            static_cast<DebugInlineScopedSignedShortComparisonEnum>(rhs);
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_scoped_signed_short_enum_compare_entry_source;
    debug_inline_scoped_signed_short_comparison_enum_escape =
        &debug_inline_signed_long_long_scoped_signed_short_enum_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_scoped_signed_short_enum_compare_entry_source <
          static_cast<long long>(
              debug_inline_signed_long_long_scoped_signed_short_enum_compare_rhs)));
}

extern "C" int
debug_inline_signed_long_long_scoped_unsigned_short_enum_compare_entry(
    long long value, unsigned short rhs)
{
    long long debug_inline_signed_long_long_scoped_unsigned_short_enum_compare_entry_source =
        value;
    DebugInlineScopedUnsignedShortComparisonEnum
        debug_inline_signed_long_long_scoped_unsigned_short_enum_compare_rhs =
            static_cast<DebugInlineScopedUnsignedShortComparisonEnum>(rhs);
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_scoped_unsigned_short_enum_compare_entry_source;
    debug_inline_scoped_unsigned_short_comparison_enum_escape =
        &debug_inline_signed_long_long_scoped_unsigned_short_enum_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_scoped_unsigned_short_enum_compare_entry_source <
          static_cast<long long>(
              debug_inline_signed_long_long_scoped_unsigned_short_enum_compare_rhs)));
}

extern "C" int
debug_inline_signed_long_long_scoped_signed_int_enum_compare_entry(
    long long value, int rhs)
{
    long long debug_inline_signed_long_long_scoped_signed_int_enum_compare_entry_source =
        value;
    DebugInlineScopedSignedComparisonEnum
        debug_inline_signed_long_long_scoped_signed_int_enum_compare_rhs =
            static_cast<DebugInlineScopedSignedComparisonEnum>(rhs);
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_scoped_signed_int_enum_compare_entry_source;
    debug_inline_scoped_signed_comparison_enum_escape =
        &debug_inline_signed_long_long_scoped_signed_int_enum_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_scoped_signed_int_enum_compare_entry_source <
          static_cast<long long>(
              debug_inline_signed_long_long_scoped_signed_int_enum_compare_rhs)));
}

extern "C" int
debug_inline_unsigned_long_long_scoped_unsigned_wide_enum_compare_entry(
    unsigned long long value, unsigned long long rhs)
{
    unsigned long long
        debug_inline_unsigned_long_long_scoped_unsigned_wide_enum_compare_entry_source =
            value;
    DebugInlineScopedUnsignedWideComparisonEnum
        debug_inline_unsigned_long_long_scoped_unsigned_wide_enum_compare_rhs =
            static_cast<DebugInlineScopedUnsignedWideComparisonEnum>(rhs);
    debug_inline_unsigned_long_long_comparison_escape =
        &debug_inline_unsigned_long_long_scoped_unsigned_wide_enum_compare_entry_source;
    debug_inline_scoped_unsigned_wide_comparison_enum_escape =
        &debug_inline_unsigned_long_long_scoped_unsigned_wide_enum_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_long_long_scoped_unsigned_wide_enum_compare_entry_source <
          static_cast<unsigned long long>(
              debug_inline_unsigned_long_long_scoped_unsigned_wide_enum_compare_rhs)));
}

extern "C" int
debug_inline_signed_long_long_scoped_unsigned_wide_enum_compare_entry(
    long long value, unsigned long long rhs)
{
    long long
        debug_inline_signed_long_long_scoped_unsigned_wide_enum_compare_entry_source =
            value;
    DebugInlineScopedUnsignedWideComparisonEnum
        debug_inline_signed_long_long_scoped_unsigned_wide_enum_compare_rhs =
            static_cast<DebugInlineScopedUnsignedWideComparisonEnum>(rhs);
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_scoped_unsigned_wide_enum_compare_entry_source;
    debug_inline_scoped_unsigned_wide_comparison_enum_escape =
        &debug_inline_signed_long_long_scoped_unsigned_wide_enum_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_scoped_unsigned_wide_enum_compare_entry_source <
          static_cast<unsigned long long>(
              debug_inline_signed_long_long_scoped_unsigned_wide_enum_compare_rhs)));
}

extern "C" int
debug_inline_unsigned_long_long_scoped_signed_short_enum_to_unsigned_compare_entry(
    unsigned long long value, short rhs)
{
    unsigned long long
        debug_inline_unsigned_long_long_scoped_signed_short_enum_to_unsigned_compare_entry_source =
            value;
    DebugInlineScopedSignedShortComparisonEnum
        debug_inline_unsigned_long_long_scoped_signed_short_enum_to_unsigned_compare_rhs =
            static_cast<DebugInlineScopedSignedShortComparisonEnum>(rhs);
    debug_inline_unsigned_long_long_comparison_escape =
        &debug_inline_unsigned_long_long_scoped_signed_short_enum_to_unsigned_compare_entry_source;
    debug_inline_scoped_signed_short_comparison_enum_escape =
        &debug_inline_unsigned_long_long_scoped_signed_short_enum_to_unsigned_compare_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_long_long_scoped_signed_short_enum_to_unsigned_compare_entry_source <
          static_cast<unsigned long long>(
              debug_inline_unsigned_long_long_scoped_signed_short_enum_to_unsigned_compare_rhs)));
}

extern "C" int debug_inline_signed_long_long_add_one_compare_entry(
    long long value)
{
    long long debug_inline_signed_long_long_add_one_compare_entry_source =
        value;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_add_one_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_add_one_compare_entry_source + 1LL <
          0LL));
}

extern "C" int debug_inline_signed_long_long_sub_one_compare_entry(
    long long value)
{
    long long debug_inline_signed_long_long_sub_one_compare_entry_source =
        value;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_sub_one_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_sub_one_compare_entry_source - 1LL <
          0LL));
}

extern "C" int debug_inline_signed_long_long_add_compare_entry(
    long long value, long long rhs)
{
    long long debug_inline_signed_long_long_add_compare_entry_source = value;
    long long debug_inline_signed_long_long_add_compare_entry_rhs = rhs;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_add_compare_entry_source;
    debug_inline_signed_long_long_comparison_rhs_escape =
        &debug_inline_signed_long_long_add_compare_entry_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_add_compare_entry_source +
          debug_inline_signed_long_long_add_compare_entry_rhs < 0LL));
}

extern "C" int debug_inline_unsigned_long_long_add_compare_entry(
    unsigned long long value, unsigned long long rhs)
{
    unsigned long long debug_inline_unsigned_long_long_add_compare_entry_source =
        value;
    unsigned long long debug_inline_unsigned_long_long_add_compare_entry_rhs =
        rhs;
    debug_inline_unsigned_long_long_comparison_escape =
        &debug_inline_unsigned_long_long_add_compare_entry_source;
    debug_inline_unsigned_long_long_comparison_rhs_escape =
        &debug_inline_unsigned_long_long_add_compare_entry_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_long_long_add_compare_entry_source +
          debug_inline_unsigned_long_long_add_compare_entry_rhs <
          0x100000000ULL));
}

extern "C" int debug_inline_signed_long_long_sub_compare_entry(
    long long value, long long rhs)
{
    long long debug_inline_signed_long_long_sub_compare_entry_source = value;
    long long debug_inline_signed_long_long_sub_compare_entry_rhs = rhs;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_sub_compare_entry_source;
    debug_inline_signed_long_long_comparison_rhs_escape =
        &debug_inline_signed_long_long_sub_compare_entry_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_sub_compare_entry_source -
          debug_inline_signed_long_long_sub_compare_entry_rhs < 0LL));
}

extern "C" int debug_inline_unsigned_long_long_sub_compare_entry(
    unsigned long long value, unsigned long long rhs)
{
    unsigned long long debug_inline_unsigned_long_long_sub_compare_entry_source =
        value;
    unsigned long long debug_inline_unsigned_long_long_sub_compare_entry_rhs =
        rhs;
    debug_inline_unsigned_long_long_comparison_escape =
        &debug_inline_unsigned_long_long_sub_compare_entry_source;
    debug_inline_unsigned_long_long_comparison_rhs_escape =
        &debug_inline_unsigned_long_long_sub_compare_entry_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_unsigned_long_long_sub_compare_entry_source -
          debug_inline_unsigned_long_long_sub_compare_entry_rhs <
          0x100000000ULL));
}

extern "C" int debug_inline_signed_long_long_bitand_compare_entry(
    long long value, long long rhs)
{
    long long debug_inline_signed_long_long_bitand_compare_entry_source = value;
    long long debug_inline_signed_long_long_bitand_compare_entry_rhs = rhs;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_bitand_compare_entry_source;
    debug_inline_signed_long_long_comparison_rhs_escape =
        &debug_inline_signed_long_long_bitand_compare_entry_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !((debug_inline_signed_long_long_bitand_compare_entry_source &
           debug_inline_signed_long_long_bitand_compare_entry_rhs) < 0LL));
}

extern "C" int debug_inline_signed_long_long_bitor_compare_entry(
    long long value, long long rhs)
{
    long long debug_inline_signed_long_long_bitor_compare_entry_source = value;
    long long debug_inline_signed_long_long_bitor_compare_entry_rhs = rhs;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_bitor_compare_entry_source;
    debug_inline_signed_long_long_comparison_rhs_escape =
        &debug_inline_signed_long_long_bitor_compare_entry_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !((debug_inline_signed_long_long_bitor_compare_entry_source |
           debug_inline_signed_long_long_bitor_compare_entry_rhs) < 0LL));
}

extern "C" int debug_inline_signed_long_long_bitxor_compare_entry(
    long long value, long long rhs)
{
    long long debug_inline_signed_long_long_bitxor_compare_entry_source = value;
    long long debug_inline_signed_long_long_bitxor_compare_entry_rhs = rhs;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_bitxor_compare_entry_source;
    debug_inline_signed_long_long_comparison_rhs_escape =
        &debug_inline_signed_long_long_bitxor_compare_entry_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !((debug_inline_signed_long_long_bitxor_compare_entry_source ^
           debug_inline_signed_long_long_bitxor_compare_entry_rhs) < 0LL));
}

extern "C" int debug_inline_signed_long_long_bitnot_compare_entry(
    long long value)
{
    long long debug_inline_signed_long_long_bitnot_compare_entry_source = value;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_bitnot_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !((~debug_inline_signed_long_long_bitnot_compare_entry_source) < 0LL));
}

extern "C" int debug_inline_signed_long_long_nested_bitwise_compare_entry(
    long long value, long long rhs)
{
    long long debug_inline_signed_long_long_nested_bitwise_compare_entry_source =
        value;
    long long debug_inline_signed_long_long_nested_bitwise_compare_entry_rhs =
        rhs;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_nested_bitwise_compare_entry_source;
    debug_inline_signed_long_long_comparison_rhs_escape =
        &debug_inline_signed_long_long_nested_bitwise_compare_entry_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !((((debug_inline_signed_long_long_nested_bitwise_compare_entry_source &
             debug_inline_signed_long_long_nested_bitwise_compare_entry_rhs) |
            debug_inline_signed_long_long_nested_bitwise_compare_entry_source) ^
           debug_inline_signed_long_long_nested_bitwise_compare_entry_rhs) <
          0LL));
}

extern "C" int
debug_inline_signed_long_long_nested_add_bitxor_compare_entry(long long value)
{
    long long
        debug_inline_signed_long_long_nested_add_bitxor_compare_entry_source =
            value;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_nested_add_bitxor_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !((((debug_inline_signed_long_long_nested_add_bitxor_compare_entry_source +
             1LL) ^ 0x123456789abcdef0LL) + 2LL) < 0LL));
}

extern "C" int
debug_inline_signed_long_long_nested_sub_bitxor_compare_entry(long long value)
{
    long long
        debug_inline_signed_long_long_nested_sub_bitxor_compare_entry_source =
            value;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_nested_sub_bitxor_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !((((debug_inline_signed_long_long_nested_sub_bitxor_compare_entry_source -
             1LL) ^ 0x123456789abcdef0LL) - 2LL) < 0LL));
}

extern "C" int
debug_inline_signed_long_long_mul_small_compare_entry(long long value)
{
    long long debug_inline_signed_long_long_mul_small_compare_entry_source =
        value;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_mul_small_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_mul_small_compare_entry_source * 3LL <
          0LL));
}

extern "C" int
debug_inline_signed_long_long_mul_wide_constant_compare_entry(long long value)
{
    long long
        debug_inline_signed_long_long_mul_wide_constant_compare_entry_source =
            value;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_mul_wide_constant_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_signed_long_long_mul_wide_constant_compare_entry_source *
              0x123456789abcdef0LL <
          0LL));
}

extern "C" int
debug_inline_signed_long_long_nested_add_mul_wide_constant_compare_entry(
    long long value)
{
    long long
        debug_inline_signed_long_long_nested_add_mul_wide_constant_compare_entry_source =
            value;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_nested_add_mul_wide_constant_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !((debug_inline_signed_long_long_nested_add_mul_wide_constant_compare_entry_source +
           1LL) * 0x123456789abcdef0LL < 0LL));
}

extern "C" int debug_inline_signed_long_long_negated_bitwise_compare_entry(
    long long value, long long rhs)
{
    long long debug_inline_signed_long_long_negated_bitwise_compare_entry_source =
        value;
    long long debug_inline_signed_long_long_negated_bitwise_compare_entry_rhs =
        rhs;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_negated_bitwise_compare_entry_source;
    debug_inline_signed_long_long_comparison_rhs_escape =
        &debug_inline_signed_long_long_negated_bitwise_compare_entry_rhs;
    return debug_nonconstant_integer_parameter_inline(
        !(-(
            debug_inline_signed_long_long_negated_bitwise_compare_entry_source &
            debug_inline_signed_long_long_negated_bitwise_compare_entry_rhs) <
          0LL));
}

extern "C" int debug_inline_unsigned_long_long_shift_compare_entry(
    unsigned long long value)
{
    unsigned long long
        debug_inline_unsigned_long_long_shift_compare_entry_source = value;
    debug_inline_unsigned_long_long_comparison_escape =
        &debug_inline_unsigned_long_long_shift_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(((debug_inline_unsigned_long_long_shift_compare_entry_source << 0u) ^
           (debug_inline_unsigned_long_long_shift_compare_entry_source << 31u) ^
           (debug_inline_unsigned_long_long_shift_compare_entry_source << 32u) ^
           (debug_inline_unsigned_long_long_shift_compare_entry_source << 33u) ^
           (debug_inline_unsigned_long_long_shift_compare_entry_source >> 0u) ^
           (debug_inline_unsigned_long_long_shift_compare_entry_source >> 1u) ^
           (debug_inline_unsigned_long_long_shift_compare_entry_source >> 31u) ^
           (debug_inline_unsigned_long_long_shift_compare_entry_source >> 32u) ^
           (debug_inline_unsigned_long_long_shift_compare_entry_source >> 33u) ^
           (debug_inline_unsigned_long_long_shift_compare_entry_source >> 63u)) <
          0x8000000000000000ULL));
}

extern "C" int debug_inline_signed_long_long_shift_compare_entry(
    long long value)
{
    long long debug_inline_signed_long_long_shift_compare_entry_source = value;
    debug_inline_signed_long_long_comparison_escape =
        &debug_inline_signed_long_long_shift_compare_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(((debug_inline_signed_long_long_shift_compare_entry_source >> 0u) ^
           (debug_inline_signed_long_long_shift_compare_entry_source >> 1u) ^
           (debug_inline_signed_long_long_shift_compare_entry_source >> 31u) ^
           (debug_inline_signed_long_long_shift_compare_entry_source >> 32u) ^
           (debug_inline_signed_long_long_shift_compare_entry_source >> 33u) ^
           (debug_inline_signed_long_long_shift_compare_entry_source >> 63u)) <
          -1LL));
}
