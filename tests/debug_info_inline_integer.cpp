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

extern "C" {
unsigned long long debug_inline_nonconstant_unsigned_long_long_global_source =
    0x8000000100000003ULL;
unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_add_source =
    0x7ffffffffffffff0ULL;
unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_mod_source =
    0xfedcba9876543217ULL;
unsigned long long
debug_inline_nonconstant_unsigned_long_long_global_shift_count_source = 4u;
unsigned int
debug_inline_nonconstant_unsigned_long_long_global_shift_count_u32_source = 4u;
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
debug_inline_nonconstant_unsigned_long_long_global_pair_xor_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_global_add_source ^
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
