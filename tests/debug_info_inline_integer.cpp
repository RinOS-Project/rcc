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
