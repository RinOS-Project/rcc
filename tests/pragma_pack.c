#include <stddef.h>
#include <stdint.h>

struct NaturalRecord {
    uint8_t tag;
    uint64_t value;
};

#pragma pack(push, 1)
struct PackedRecord {
    uint8_t tag;
    uint64_t value;
    uint16_t tail;
};

#pragma pack(push, 2)
struct NestedPackedRecord {
    uint8_t tag;
    uint32_t value;
    uint16_t tail;
};
#pragma pack(pop)

struct PackedOuterRecord {
    uint8_t prefix;
    struct {
        uint16_t first;
        uint32_t second;
    } inner;
    uint32_t values[3];
};
#pragma pack(pop)

#pragma pack(4)
struct FourBytePackedRecord {
    uint8_t tag;
    uint64_t value;
    uint8_t tail;
};
#pragma pack()

struct NaturalRecordAfterReset {
    uint8_t tag;
    uint64_t value;
};

_Static_assert(sizeof(struct NaturalRecord) == 16u,
               "natural aggregate layout");
_Static_assert(offsetof(struct NaturalRecord, value) == 8u,
               "natural member offset");
_Static_assert(sizeof(struct PackedRecord) == 11u,
               "one-byte packed aggregate layout");
_Static_assert(offsetof(struct PackedRecord, value) == 1u,
               "one-byte packed member offset");
_Static_assert(sizeof(struct NestedPackedRecord) == 8u,
               "nested pack push layout");
_Static_assert(offsetof(struct NestedPackedRecord, value) == 2u,
               "nested pack push member offset");
_Static_assert(offsetof(struct PackedOuterRecord, inner.second) == 3u,
               "nested offsetof member designator");
_Static_assert(offsetof(struct PackedOuterRecord, values[2]) == 15u,
               "offsetof array designator");
_Static_assert(sizeof(struct FourBytePackedRecord) == 16u,
               "four-byte packed aggregate layout");
_Static_assert(offsetof(struct FourBytePackedRecord, value) == 4u,
               "four-byte packed member offset");
_Static_assert(sizeof(struct NaturalRecordAfterReset) == 16u,
               "pack reset aggregate layout");
_Static_assert(offsetof(struct NaturalRecordAfterReset, value) == 8u,
               "pack reset member offset");
