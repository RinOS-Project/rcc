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

static inline int debug_nonconstant_integer_parameter_inline(int value)
{
    return value;
}

long long* debug_inline_signed_long_long_comparison_escape;
unsigned long long* debug_inline_unsigned_long_long_comparison_escape;

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
