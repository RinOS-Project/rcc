enum class DebugUnsignedEnum : unsigned long long {
    maximum = 18446744073709551615ULL,
};

DebugUnsignedEnum debug_unsigned_enum;

enum DebugInferredUnsignedEnum {
    inferred_maximum = 18446744073709551615ULL,
};

DebugInferredUnsignedEnum debug_inferred_unsigned_enum;

enum DebugInferredUnsignedInt {
    inferred_uint_max = 0xFFFFFFFFU,
};

DebugInferredUnsignedInt debug_inferred_unsigned_int;

enum DebugSignedEnum : signed char {
    signed_negative = -2,
    signed_positive = 6,
};

DebugSignedEnum debug_signed_enum;

enum class DebugScopedEnum : unsigned short {
    scoped_value = 7,
};

DebugScopedEnum debug_scoped_enum;

enum struct DebugScopedStructEnum : signed char {
    struct_scoped_value = -1,
};

DebugScopedStructEnum debug_scoped_struct_enum;
