enum DebugInlineUnsignedComparisonEnum : unsigned int {
    debug_inline_unsigned_comparison_enum_zero = 0u,
};

DebugInlineUnsignedComparisonEnum*
    debug_inline_unsigned_comparison_enum_escape;

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
