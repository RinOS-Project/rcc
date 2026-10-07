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
