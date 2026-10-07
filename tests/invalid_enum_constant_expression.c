int enum_runtime_value;

enum InvalidNonconstantEnumerator {
    ENUM_RUNTIME_VALUE = enum_runtime_value
};

enum InvalidOutOfRangeEnumerator {
    ENUM_OUT_OF_RANGE_VALUE = 2147483648ULL
};

enum InvalidImplicitOutOfRangeEnumerator {
    ENUM_IMPLICIT_MAXIMUM = 2147483647,
    ENUM_IMPLICIT_OUT_OF_RANGE
};
