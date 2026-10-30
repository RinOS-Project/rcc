#define DEFINE_AGGREGATE_ATOMIC_SUITE(tag, member_type, initial_value, \
                                      source_value, desired_value) \
    typedef struct { member_type value; } tag##_aggregate; \
    static volatile tag##_aggregate tag##_object = {initial_value}; \
    static tag##_aggregate tag##_source = {source_value}; \
    static tag##_aggregate tag##_result = {0}; \
    static tag##_aggregate tag##_previous = {0}; \
    static tag##_aggregate tag##_expected = {0}; \
    static tag##_aggregate tag##_desired = {desired_value}; \
    __attribute__((noinline)) int tag##_aggregate_atomic_suite(void) \
    { \
        int failed; \
        int succeeded; \
        tag##_object.value = (member_type)(initial_value); \
        __atomic_store(&tag##_object, &tag##_source, __ATOMIC_RELEASE); \
        __atomic_load(&tag##_object, &tag##_result, __ATOMIC_ACQUIRE); \
        if (tag##_object.value != tag##_source.value || \
            tag##_result.value != tag##_source.value) return 1; \
        tag##_object.value = (member_type)(initial_value); \
        __atomic_exchange(&tag##_object, &tag##_source, \
                          &tag##_previous, __ATOMIC_ACQ_REL); \
        if (tag##_object.value != tag##_source.value || \
            tag##_previous.value != (member_type)(initial_value)) return 1; \
        tag##_object.value = (member_type)(initial_value); \
        tag##_expected.value = (member_type)(source_value); \
        tag##_desired.value = (member_type)(desired_value); \
        failed = __atomic_compare_exchange( \
            &tag##_object, &tag##_expected, &tag##_desired, 0, \
            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE); \
        succeeded = __atomic_compare_exchange( \
            &tag##_object, &tag##_expected, &tag##_desired, 0, \
            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE); \
        return !failed && succeeded && \
            tag##_expected.value == (member_type)(initial_value) && \
            tag##_object.value == tag##_desired.value ? 0 : 1; \
    }

#define DEFINE_UNALIGNED_AGGREGATE_ATOMIC_SUITE( \
    tag, member_type, initial_value, source_value, desired_value) \
    typedef struct __attribute__((packed)) { \
        member_type value; \
    } tag##_unaligned_aggregate; \
    typedef struct __attribute__((packed)) { \
        unsigned char prefix; \
        tag##_unaligned_aggregate value; \
    } tag##_unaligned_storage; \
    static volatile tag##_unaligned_storage \
        tag##_unaligned_object_storage = {0u, {initial_value}}; \
    static tag##_unaligned_aggregate tag##_unaligned_source = {source_value}; \
    static tag##_unaligned_aggregate tag##_unaligned_result = {0}; \
    static tag##_unaligned_aggregate tag##_unaligned_previous = {0}; \
    static tag##_unaligned_aggregate tag##_unaligned_expected = {0}; \
    static tag##_unaligned_aggregate tag##_unaligned_desired = {desired_value}; \
    __attribute__((noinline)) int tag##_unaligned_atomic_suite(void) \
    { \
        volatile tag##_unaligned_aggregate* object = \
            (volatile tag##_unaligned_aggregate*)( \
                (volatile unsigned char*)&tag##_unaligned_object_storage + 1); \
        int failed; \
        int succeeded; \
        object->value = (member_type)(initial_value); \
        __atomic_store(object, &tag##_unaligned_source, __ATOMIC_RELEASE); \
        __atomic_load(object, &tag##_unaligned_result, __ATOMIC_ACQUIRE); \
        if (object->value != tag##_unaligned_source.value || \
            tag##_unaligned_result.value != tag##_unaligned_source.value) \
            return 1; \
        object->value = (member_type)(initial_value); \
        __atomic_exchange(object, &tag##_unaligned_source, \
                          &tag##_unaligned_previous, __ATOMIC_ACQ_REL); \
        if (object->value != tag##_unaligned_source.value || \
            tag##_unaligned_previous.value != (member_type)(initial_value)) \
            return 1; \
        object->value = (member_type)(initial_value); \
        tag##_unaligned_expected.value = (member_type)(source_value); \
        tag##_unaligned_desired.value = (member_type)(desired_value); \
        failed = __atomic_compare_exchange( \
            object, &tag##_unaligned_expected, &tag##_unaligned_desired, 0, \
            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE); \
        succeeded = __atomic_compare_exchange( \
            object, &tag##_unaligned_expected, &tag##_unaligned_desired, 0, \
            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE); \
        return !failed && succeeded && \
            tag##_unaligned_expected.value == (member_type)(initial_value) && \
            object->value == tag##_unaligned_desired.value ? 0 : 1; \
    }

DEFINE_AGGREGATE_ATOMIC_SUITE(byte, unsigned char, 0x19u, 0x53u, 0xa7u)
DEFINE_AGGREGATE_ATOMIC_SUITE(half, unsigned short, 0x1937u, 0x5261u,
                              0xa7c5u)
DEFINE_AGGREGATE_ATOMIC_SUITE(word, unsigned int, 0x10293847u, 0x50617283u,
                              0x94a5b6c7u)
DEFINE_UNALIGNED_AGGREGATE_ATOMIC_SUITE(
    packed_half, unsigned short, 0x1937u, 0x5261u, 0xa7c5u)
DEFINE_UNALIGNED_AGGREGATE_ATOMIC_SUITE(
    packed_word, unsigned int, 0x10293847u, 0x50617283u, 0x94a5b6c7u)
#if defined(__x86_64__)
DEFINE_AGGREGATE_ATOMIC_SUITE(wide, unsigned long long,
                              0x1029384750617283ULL,
                              0x94a5b6c713579bdfULL,
                              0x5061728394a5b6c7ULL)
DEFINE_UNALIGNED_AGGREGATE_ATOMIC_SUITE(
    packed_wide, unsigned long long, 0x1029384750617283ULL,
    0x94a5b6c713579bdfULL, 0x5061728394a5b6c7ULL)
#endif

typedef union { unsigned int value; } word_union_aggregate;
typedef struct {
    unsigned int low : 3;
    unsigned int high : 5;
} bitfield_aggregate;
typedef struct {
    volatile unsigned int status;
    unsigned int payload;
} nested_volatile_member_aggregate;
typedef struct {
    nested_volatile_member_aggregate nested;
    unsigned int sequence;
} nested_volatile_container;
typedef union {
    volatile unsigned int status;
    unsigned int raw;
} nested_volatile_member_union;
typedef struct {
    volatile unsigned int control : 3;
    unsigned int count : 5;
    unsigned int payload;
} nested_volatile_bitfield_aggregate;
typedef struct {
    volatile signed int value : 4;
    unsigned int sibling : 4;
} nested_volatile_signed_bitfield_aggregate;

__attribute__((noinline)) int local_volatile_aggregate_atomic(void)
{
    volatile word_aggregate object;
    word_aggregate source = {0x50617283u};
    word_aggregate result = {0u};
    word_aggregate previous = {0u};
    word_aggregate expected = {0xdeadbeefu};
    word_aggregate desired = {0x13579bdfu};
    int failed;
    int succeeded;

    object.value = 0x10293847u;
    __atomic_store(&object, &source, __ATOMIC_RELEASE);
    __atomic_load(&object, &result, __ATOMIC_ACQUIRE);
    if (object.value != source.value || result.value != source.value) {
        return 1;
    }
    object.value = 0x10293847u;
    __atomic_exchange(&object, &source, &previous, __ATOMIC_ACQ_REL);
    if (object.value != source.value || previous.value != 0x10293847u) {
        return 1;
    }
    object.value = 0x94a5b6c7u;
    failed = __atomic_compare_exchange(
        &object, &expected, &desired, 0, __ATOMIC_ACQ_REL,
        __ATOMIC_ACQUIRE);
    succeeded = __atomic_compare_exchange(
        &object, &expected, &desired, 0, __ATOMIC_ACQ_REL,
        __ATOMIC_ACQUIRE);
    return !failed && succeeded && expected.value == 0x94a5b6c7u &&
        object.value == desired.value ? 0 : 1;
}

__attribute__((noinline)) int local_volatile_union_atomic(void)
{
    volatile word_union_aggregate object;
    word_union_aggregate source = {0x50617283u};
    word_union_aggregate result = {0u};
    word_union_aggregate previous = {0u};
    word_union_aggregate expected = {0xdeadbeefu};
    word_union_aggregate desired = {0x13579bdfu};
    int failed;
    int succeeded;

    object.value = 0x10293847u;
    __atomic_store(&object, &source, __ATOMIC_RELEASE);
    __atomic_load(&object, &result, __ATOMIC_ACQUIRE);
    if (object.value != source.value || result.value != source.value) {
        return 1;
    }
    object.value = 0x10293847u;
    __atomic_exchange(&object, &source, &previous, __ATOMIC_ACQ_REL);
    if (object.value != source.value || previous.value != 0x10293847u) {
        return 1;
    }
    object.value = 0x94a5b6c7u;
    failed = __atomic_compare_exchange(
        &object, &expected, &desired, 0, __ATOMIC_ACQ_REL,
        __ATOMIC_ACQUIRE);
    succeeded = __atomic_compare_exchange(
        &object, &expected, &desired, 0, __ATOMIC_ACQ_REL,
        __ATOMIC_ACQUIRE);
    return !failed && succeeded && expected.value == 0x94a5b6c7u &&
        object.value == desired.value ? 0 : 1;
}

__attribute__((noinline)) int local_volatile_aggregate_initialized(void)
{
    unsigned int seed = 0x10293847u;
    volatile word_aggregate structure = {seed};
    volatile word_union_aggregate union_object = {
        .value = seed + 1u
    };

    return structure.value == seed && union_object.value == seed + 1u
        ? 0 : 1;
}

__attribute__((noinline)) int local_volatile_aggregate_copy(void)
{
    volatile word_aggregate volatile_structure = {0u};
    word_aggregate plain_structure = {0x10293847u};
    volatile word_union_aggregate volatile_union = {.value = 0u};
    word_union_aggregate plain_union = {.value = 0x50617283u};
    volatile word_aggregate* volatile_structure_pointer =
        &volatile_structure;
    word_aggregate* plain_structure_pointer = &plain_structure;
    volatile word_union_aggregate* volatile_union_pointer =
        &volatile_union;
    word_union_aggregate* plain_union_pointer = &plain_union;
    volatile bitfield_aggregate volatile_bits;
    bitfield_aggregate plain_bits = {5u, 19u};
    nested_volatile_container nested_source = {
        {0x10293847u, 0x50617283u}, 0x94a5b6c7u
    };
    nested_volatile_container nested_destination = {{0u, 0u}, 0u};
    nested_volatile_member_union nested_union_source = {
        .status = 0x13579bdfu
    };
    nested_volatile_member_union nested_union_destination = {.raw = 0u};
    nested_volatile_bitfield_aggregate nested_bits_source = {
        5u, 19u, 0x2468ace1u
    };
    nested_volatile_bitfield_aggregate nested_bits_destination = {
        0u, 0u, 0u
    };
    nested_volatile_signed_bitfield_aggregate signed_bits_source = {
        -3, 10u
    };
    nested_volatile_signed_bitfield_aggregate signed_bits_destination = {
        0, 0u
    };
    nested_volatile_container* nested_source_pointer = &nested_source;
    nested_volatile_container* nested_destination_pointer =
        &nested_destination;
    volatile unsigned char* volatile_bits_bytes =
        (volatile unsigned char*)&volatile_bits;
    unsigned char* plain_bits_bytes = (unsigned char*)&plain_bits;

    volatile_structure = plain_structure;
    plain_union = volatile_union;
    if (volatile_structure.value != plain_structure.value ||
        plain_union.value != 0u) return 1;
    plain_structure.value = 0x94a5b6c7u;
    plain_union.value = 0xa7c5e319u;
    volatile_structure = plain_structure;
    volatile_union = plain_union;
    plain_structure.value = 0x13579bdfu;
    plain_union.value = 0x2468ace1u;
    *volatile_structure_pointer = *plain_structure_pointer;
    *volatile_union_pointer = *plain_union_pointer;
    if (volatile_structure.value != plain_structure.value ||
        volatile_union.value != plain_union.value) return 1;
    volatile_structure.value = 0x50617283u;
    volatile_union.value = 0x94a5b6c7u;
    *plain_structure_pointer = *volatile_structure_pointer;
    *plain_union_pointer = *volatile_union_pointer;
    if (plain_structure.value != 0x50617283u ||
        plain_union.value != 0x94a5b6c7u) return 1;
    volatile_bits = plain_bits;
    for (unsigned int offset = 0u; offset < sizeof(plain_bits); ++offset) {
        if (volatile_bits_bytes[offset] != plain_bits_bytes[offset]) {
            return 1;
        }
    }
    plain_bits = volatile_bits;
    for (unsigned int offset = 0u; offset < sizeof(plain_bits); ++offset) {
        if (plain_bits_bytes[offset] != volatile_bits_bytes[offset]) {
            return 1;
        }
    }
    nested_destination = nested_source;
    if (nested_destination.nested.status != 0x10293847u ||
        nested_destination.nested.payload != 0x50617283u ||
        nested_destination.sequence != 0x94a5b6c7u) return 1;
    nested_source.nested.status = 0xa7c5e319u;
    nested_destination.nested = nested_source.nested;
    if (nested_destination.nested.status != 0xa7c5e319u ||
        nested_destination.nested.payload != 0x50617283u) return 1;
    nested_source.nested.status = 0x13579bdfu;
    *nested_destination_pointer = *nested_source_pointer;
    if (nested_destination.nested.status != 0x13579bdfu ||
        nested_destination.nested.payload != 0x50617283u ||
        nested_destination.sequence != 0x94a5b6c7u) return 1;
    nested_union_destination = nested_union_source;
    if (nested_union_destination.status != 0x13579bdfu) return 1;
    nested_bits_destination = nested_bits_source;
    if (nested_bits_destination.control != 5u ||
        nested_bits_destination.count != 19u ||
        nested_bits_destination.payload != 0x2468ace1u) return 1;
    nested_bits_destination.control = 3u;
    if (nested_bits_destination.control != 3u ||
        nested_bits_destination.count != 19u) return 1;
    nested_bits_destination.count = 7u;
    nested_bits_destination.count += 2u;
    if (nested_bits_destination.control != 3u ||
        nested_bits_destination.count != 9u) return 1;
    if (nested_bits_destination.control++ != 3u ||
        nested_bits_destination.control != 4u ||
        nested_bits_destination.count != 9u) return 1;
    signed_bits_destination = signed_bits_source;
    if (signed_bits_source.value != -3 ||
        signed_bits_source.sibling != 10u ||
        signed_bits_destination.value != -3 ||
        signed_bits_destination.sibling != 10u) return 1;
    signed_bits_destination.value = -1;
    signed_bits_destination.value += 3;
    if (signed_bits_destination.value != 2 ||
        signed_bits_destination.sibling != 10u) return 1;
    return volatile_structure.value == plain_structure.value &&
        volatile_union.value == plain_union.value ? 0 : 1;
}

__attribute__((noinline)) int static_volatile_aggregate_copy(void)
{
    static volatile word_aggregate initialized_structure = {0x10293847u};
    static volatile word_union_aggregate zero_initialized_union;
    static nested_volatile_container initialized_nested = {
        {0x13579bdfu, 0x2468ace1u}, 0x10293847u
    };
    static nested_volatile_container zero_nested;
    static nested_volatile_member_union initialized_nested_union = {
        .status = 0x50617283u
    };
    static nested_volatile_member_union zero_nested_union;
    word_aggregate structure_source = {0x50617283u};
    word_union_aggregate union_source = {0x94a5b6c7u};
    nested_volatile_container nested_source = {
        {0xa7c5e319u, 0x13579bdfu}, 0x2468ace1u
    };
    nested_volatile_member_union nested_union_source = {
        .status = 0x94a5b6c7u
    };

    if (initialized_structure.value != 0x10293847u ||
        zero_initialized_union.value != 0u ||
        initialized_nested.nested.status != 0x13579bdfu ||
        initialized_nested.nested.payload != 0x2468ace1u ||
        initialized_nested.sequence != 0x10293847u ||
        zero_nested.nested.status != 0u ||
        initialized_nested_union.status != 0x50617283u ||
        zero_nested_union.status != 0u) return 1;
    initialized_structure = structure_source;
    zero_initialized_union = union_source;
    initialized_nested = nested_source;
    zero_nested = nested_source;
    initialized_nested_union = nested_union_source;
    zero_nested_union = nested_union_source;
    if (initialized_structure.value != structure_source.value ||
        zero_initialized_union.value != union_source.value ||
        initialized_nested.nested.status != nested_source.nested.status ||
        initialized_nested.nested.payload != nested_source.nested.payload ||
        initialized_nested.sequence != nested_source.sequence ||
        zero_nested.nested.status != nested_source.nested.status ||
        zero_nested.nested.payload != nested_source.nested.payload ||
        zero_nested.sequence != nested_source.sequence ||
        initialized_nested_union.status != nested_union_source.status ||
        zero_nested_union.status != nested_union_source.status) return 1;
    structure_source.value = 0xa7c5e319u;
    union_source.value = 0x13579bdfu;
    initialized_structure = structure_source;
    zero_initialized_union = union_source;
    return initialized_structure.value == structure_source.value &&
        zero_initialized_union.value == union_source.value &&
        initialized_nested.nested.status == nested_source.nested.status &&
        zero_nested.sequence == nested_source.sequence &&
        initialized_nested_union.status == nested_union_source.status &&
        zero_nested_union.status == nested_union_source.status ? 0 : 1;
}

int main(void)
{
    int pointees[2] = {0, 0};
    volatile unsigned int object = 0x12345678u;
    unsigned int source = 0x87654321u;
    unsigned int result = 0u;
    int * volatile pointer_object = &pointees[0];
    int *pointer_source = &pointees[1];
    int *pointer_result = &pointees[0];
    volatile unsigned int exchange_object = 0x10203040u;
    unsigned int exchange_source = 0x50607080u;
    unsigned int exchange_previous = 0u;
    int * volatile pointer_exchange_object = &pointees[0];
    int *pointer_exchange_source = &pointees[1];
    int *pointer_exchange_previous = &pointees[0];
    unsigned int object_address_evaluations = 0u;
    unsigned int value_address_evaluations = 0u;
    unsigned int store_order_evaluations = 0u;
    unsigned int load_order_evaluations = 0u;
    unsigned int exchange_object_address_evaluations = 0u;
    unsigned int exchange_source_address_evaluations = 0u;
    unsigned int exchange_result_address_evaluations = 0u;
    unsigned int exchange_order_evaluations = 0u;

    __atomic_store(
        (object_address_evaluations++, &object),
        (value_address_evaluations++, &source),
        (store_order_evaluations++, __ATOMIC_RELEASE));
    __atomic_load(
        (object_address_evaluations++, &object),
        (value_address_evaluations++, &result),
        (load_order_evaluations++, __ATOMIC_ACQUIRE));
    __atomic_store(&pointer_object, &pointer_source, __ATOMIC_RELEASE);
    __atomic_load(&pointer_object, &pointer_result, __ATOMIC_ACQUIRE);
    __atomic_exchange(
        (exchange_object_address_evaluations++, &exchange_object),
        (exchange_source_address_evaluations++, &exchange_source),
        (exchange_result_address_evaluations++, &exchange_previous),
        (exchange_order_evaluations++, __ATOMIC_ACQ_REL));
    __atomic_exchange(&pointer_exchange_object,
                      &pointer_exchange_source,
                      &pointer_exchange_previous, __ATOMIC_SEQ_CST);

    return object == source && result == source &&
        pointer_object == pointer_source &&
        pointer_result == pointer_source &&
        exchange_object == exchange_source &&
        exchange_previous == 0x10203040u &&
        pointer_exchange_object == pointer_exchange_source &&
        pointer_exchange_previous == &pointees[0] &&
        byte_aggregate_atomic_suite() == 0 &&
        half_aggregate_atomic_suite() == 0 &&
        word_aggregate_atomic_suite() == 0 &&
        packed_half_unaligned_atomic_suite() == 0 &&
        packed_word_unaligned_atomic_suite() == 0 &&
        local_volatile_aggregate_atomic() == 0 &&
        local_volatile_union_atomic() == 0 &&
        local_volatile_aggregate_initialized() == 0 &&
        local_volatile_aggregate_copy() == 0 &&
        static_volatile_aggregate_copy() == 0 &&
#if defined(__x86_64__)
        wide_aggregate_atomic_suite() == 0 &&
        packed_wide_unaligned_atomic_suite() == 0 &&
#endif
        object_address_evaluations == 2u &&
        value_address_evaluations == 2u &&
        store_order_evaluations == 1u &&
        load_order_evaluations == 1u &&
        exchange_object_address_evaluations == 1u &&
        exchange_source_address_evaluations == 1u &&
        exchange_result_address_evaluations == 1u &&
        exchange_order_evaluations == 1u ? 0 : 1;
}
