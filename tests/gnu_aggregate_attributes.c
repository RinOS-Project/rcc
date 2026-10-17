#include <stddef.h>

typedef struct NaturalNested {
    unsigned char byte;
    unsigned int word;
} NaturalNested;

typedef struct __attribute__((packed)) PackedRecord {
    unsigned char tag;
    unsigned int value;
} PackedRecord;

#define RIN_TEST_ALIGNED __attribute__((aligned(8)))
typedef struct RIN_TEST_ALIGNED AlignedRecord {
    unsigned char value;
} AlignedRecord;

typedef struct PackedOuter {
    unsigned char prefix;
    NaturalNested nested;
    unsigned char suffix;
} __attribute__((packed)) PackedOuter;

typedef struct {
    unsigned char tag;
    unsigned int value;
} __attribute__((packed)) AnonymousPackedRecord;

_Static_assert(sizeof(PackedRecord) == 5u, "packed aggregate size");
_Static_assert(_Alignof(PackedRecord) == 1u, "packed aggregate alignment");
_Static_assert(offsetof(PackedRecord, value) == 1u,
               "packed member offset");
_Static_assert(sizeof(AlignedRecord) == 8u, "aligned aggregate size");
_Static_assert(_Alignof(AlignedRecord) == 8u,
               "aligned aggregate alignment");
_Static_assert(offsetof(PackedOuter, nested) == 1u,
               "packed outer member offset");
_Static_assert(offsetof(PackedOuter, suffix) == 9u,
               "packed outer preserves nested layout");
_Static_assert(sizeof(PackedOuter) == 10u, "packed outer size");
_Static_assert(sizeof(AnonymousPackedRecord) == 5u,
               "anonymous packed aggregate size");
_Static_assert(offsetof(AnonymousPackedRecord, value) == 1u,
               "anonymous packed member offset");
