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
