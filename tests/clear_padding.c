#include <stddef.h>

#if !__has_builtin(__builtin_clear_padding)
#error "RCC must report __builtin_clear_padding"
#endif

struct ClearPaddingPair {
    unsigned char tag;
    unsigned value;
};

struct ClearPaddingNested {
    unsigned char leading;
    struct ClearPaddingPair pairs[2];
    unsigned char trailing;
};

struct ClearPaddingBits {
    unsigned low : 3;
    unsigned high : 5;
};

union ClearPaddingSameLayout {
    struct ClearPaddingPair first;
    struct ClearPaddingPair second;
};

union ClearPaddingDifferentSizes {
    struct ClearPaddingPair wide;
    unsigned char small;
};

static int clear_padding_pair(void) {
    struct ClearPaddingPair value = {0x5au, 0x12345678u};
    volatile unsigned char* representation =
        (volatile unsigned char*)&value;
    representation[1] = 0xa1u;
    representation[2] = 0xb2u;
    representation[3] = 0xc3u;
    __builtin_clear_padding(&value);
    return value.tag == 0x5au && value.value == 0x12345678u &&
        representation[1] == 0u && representation[2] == 0u &&
        representation[3] == 0u;
}

static int clear_padding_array_and_nested(void) {
    struct ClearPaddingPair values[2] = {
        {0x11u, 0x22334455u}, {0x66u, 0x778899aau}
    };
    struct ClearPaddingNested nested = {
        0x31u, {{0x42u, 0x53647586u}, {0x97u, 0xa8b9cadbu}}, 0xecu
    };
    volatile unsigned char* value_bytes =
        (volatile unsigned char*)values;
    volatile unsigned char* nested_bytes =
        (volatile unsigned char*)&nested;
    const size_t pairs_offset =
        __builtin_offsetof(struct ClearPaddingNested, pairs);
    const size_t trailing_offset =
        __builtin_offsetof(struct ClearPaddingNested, trailing);

    value_bytes[1] = 0xa1u;
    value_bytes[2] = 0xb2u;
    value_bytes[3] = 0xc3u;
    value_bytes[sizeof(struct ClearPaddingPair) + 1u] = 0xd4u;
    value_bytes[sizeof(struct ClearPaddingPair) + 2u] = 0xe5u;
    value_bytes[sizeof(struct ClearPaddingPair) + 3u] = 0xf6u;
    __builtin_clear_padding(&values);

    nested_bytes[1] = 0xa1u;
    nested_bytes[2] = 0xb2u;
    nested_bytes[3] = 0xc3u;
    nested_bytes[pairs_offset + 1u] = 0xd4u;
    nested_bytes[pairs_offset + 2u] = 0xe5u;
    nested_bytes[pairs_offset + 3u] = 0xf6u;
    nested_bytes[pairs_offset + sizeof(struct ClearPaddingPair) + 1u] = 0x17u;
    nested_bytes[pairs_offset + sizeof(struct ClearPaddingPair) + 2u] = 0x28u;
    nested_bytes[pairs_offset + sizeof(struct ClearPaddingPair) + 3u] = 0x39u;
    nested_bytes[trailing_offset + 1u] = 0x4au;
    nested_bytes[trailing_offset + 2u] = 0x5bu;
    nested_bytes[trailing_offset + 3u] = 0x6cu;
    __builtin_clear_padding(&nested);

    return values[0].tag == 0x11u && values[0].value == 0x22334455u &&
        values[1].tag == 0x66u && values[1].value == 0x778899aau &&
        value_bytes[1] == 0u && value_bytes[2] == 0u && value_bytes[3] == 0u &&
        value_bytes[9] == 0u && value_bytes[10] == 0u && value_bytes[11] == 0u &&
        nested.leading == 0x31u && nested.pairs[0].tag == 0x42u &&
        nested.pairs[0].value == 0x53647586u &&
        nested.pairs[1].tag == 0x97u &&
        nested.pairs[1].value == 0xa8b9cadbu && nested.trailing == 0xecu &&
        nested_bytes[1] == 0u && nested_bytes[2] == 0u &&
        nested_bytes[3] == 0u && nested_bytes[pairs_offset + 1u] == 0u &&
        nested_bytes[pairs_offset + 2u] == 0u &&
        nested_bytes[pairs_offset + 3u] == 0u &&
        nested_bytes[pairs_offset + sizeof(struct ClearPaddingPair) + 1u] == 0u &&
        nested_bytes[pairs_offset + sizeof(struct ClearPaddingPair) + 2u] == 0u &&
        nested_bytes[pairs_offset + sizeof(struct ClearPaddingPair) + 3u] == 0u &&
        nested_bytes[trailing_offset + 1u] == 0u &&
        nested_bytes[trailing_offset + 2u] == 0u &&
        nested_bytes[trailing_offset + 3u] == 0u;
}

static int clear_padding_bitfield(void) {
    struct ClearPaddingBits value = {5u, 17u};
    volatile unsigned char* representation =
        (volatile unsigned char*)&value;
    representation[1] = 0xa1u;
    representation[2] = 0xb2u;
    representation[3] = 0xc3u;
    __builtin_clear_padding(&value);
    return value.low == 5u && value.high == 17u &&
        representation[1] == 0u && representation[2] == 0u &&
        representation[3] == 0u;
}

static int clear_padding_union(void) {
    union ClearPaddingSameLayout same = {
        .first = {0x27u, 0x38495a6bu}
    };
    union ClearPaddingDifferentSizes different = {
        .wide = {0x7cu, 0x8d9eaf01u}
    };
    volatile unsigned char* same_bytes =
        (volatile unsigned char*)&same;
    volatile unsigned char* different_bytes =
        (volatile unsigned char*)&different;
    same_bytes[1] = 0xa1u;
    same_bytes[2] = 0xb2u;
    same_bytes[3] = 0xc3u;
    different_bytes[1] = 0xd4u;
    different_bytes[2] = 0xe5u;
    different_bytes[3] = 0xf6u;
    __builtin_clear_padding(&same);
    __builtin_clear_padding(&different);
    return same.first.tag == 0x27u &&
        same.first.value == 0x38495a6bu && same_bytes[1] == 0u &&
        same_bytes[2] == 0u && same_bytes[3] == 0u &&
        different.wide.tag == 0x7cu &&
        different.wide.value == 0x8d9eaf01u && different_bytes[1] == 0u &&
        different_bytes[2] == 0u && different_bytes[3] == 0u;
}

#define CLEAR_PADDING_LARGE_COUNT 2048u

static int clear_padding_large_array(void) {
    struct ClearPaddingPair values[CLEAR_PADDING_LARGE_COUNT];
    volatile unsigned char* bytes = (volatile unsigned char*)values;
    for (size_t index = 0u; index < CLEAR_PADDING_LARGE_COUNT; ++index) {
        values[index].tag = (unsigned char)(index + 1u);
        values[index].value = (unsigned)(index * 17u + 9u);
        bytes[index * sizeof(values[0]) + 1u] = 0xa1u;
        bytes[index * sizeof(values[0]) + 2u] = 0xb2u;
        bytes[index * sizeof(values[0]) + 3u] = 0xc3u;
    }
    __builtin_clear_padding(&values);
    for (size_t index = 0u; index < CLEAR_PADDING_LARGE_COUNT; ++index) {
        if (values[index].tag != (unsigned char)(index + 1u)) return 0;
        if (values[index].value != (unsigned)(index * 17u + 9u)) return 0;
        if (bytes[index * sizeof(values[0]) + 1u] != 0u) return 0;
        if (bytes[index * sizeof(values[0]) + 2u] != 0u) return 0;
        if (bytes[index * sizeof(values[0]) + 3u] != 0u) return 0;
    }
    return 1;
}

static int clear_padding_vla(size_t rows, size_t columns) {
    struct ClearPaddingPair values[rows++][columns];
    const size_t row_count = rows - 1u;
    volatile unsigned char* bytes = (volatile unsigned char*)values;
    for (size_t row = 0u; row < row_count; ++row) {
        for (size_t column = 0u; column < columns; ++column) {
            const size_t index = row * columns + column;
            const size_t offset = index * sizeof(values[0][0]);
            values[row][column].tag = (unsigned char)(index + 3u);
            values[row][column].value = (unsigned)(index * 29u + 5u);
            bytes[offset + 1u] = 0x17u;
            bytes[offset + 2u] = 0x28u;
            bytes[offset + 3u] = 0x39u;
        }
    }
    __builtin_clear_padding(&values);
    if (rows != row_count + 1u) return 0;
    for (size_t row = 0u; row < row_count; ++row) {
        for (size_t column = 0u; column < columns; ++column) {
            const size_t index = row * columns + column;
            const size_t offset = index * sizeof(values[0][0]);
            if (values[row][column].tag != (unsigned char)(index + 3u)) {
                return 0;
            }
            if (values[row][column].value != (unsigned)(index * 29u + 5u)) {
                return 0;
            }
            if (bytes[offset + 1u] != 0u) return 0;
            if (bytes[offset + 2u] != 0u) return 0;
            if (bytes[offset + 3u] != 0u) return 0;
        }
    }
    return 1;
}

int main(void) {
    if (!clear_padding_pair()) return 1;
    if (!clear_padding_array_and_nested()) return 2;
    if (!clear_padding_bitfield()) return 3;
    if (!clear_padding_union()) return 4;
    if (!clear_padding_large_array()) return 5;
    if (!clear_padding_vla(4u, 5u)) return 6;
    return 0;
}
