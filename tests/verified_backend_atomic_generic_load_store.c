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

DEFINE_AGGREGATE_ATOMIC_SUITE(byte, unsigned char, 0x19u, 0x53u, 0xa7u)
DEFINE_AGGREGATE_ATOMIC_SUITE(half, unsigned short, 0x1937u, 0x5261u,
                              0xa7c5u)
DEFINE_AGGREGATE_ATOMIC_SUITE(word, unsigned int, 0x10293847u, 0x50617283u,
                              0x94a5b6c7u)
#if defined(__x86_64__)
DEFINE_AGGREGATE_ATOMIC_SUITE(wide, unsigned long long,
                              0x1029384750617283ULL,
                              0x94a5b6c713579bdfULL,
                              0x5061728394a5b6c7ULL)
#endif

typedef union { unsigned int value; } word_union_aggregate;
typedef struct {
    unsigned int low : 3;
    unsigned int high : 5;
} bitfield_aggregate;

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
    return volatile_structure.value == plain_structure.value &&
        volatile_union.value == plain_union.value ? 0 : 1;
}

__attribute__((noinline)) int static_volatile_aggregate_copy(void)
{
    static volatile word_aggregate initialized_structure = {0x10293847u};
    static volatile word_union_aggregate zero_initialized_union;
    word_aggregate structure_source = {0x50617283u};
    word_union_aggregate union_source = {0x94a5b6c7u};

    if (initialized_structure.value != 0x10293847u ||
        zero_initialized_union.value != 0u) return 1;
    initialized_structure = structure_source;
    zero_initialized_union = union_source;
    if (initialized_structure.value != structure_source.value ||
        zero_initialized_union.value != union_source.value) return 1;
    structure_source.value = 0xa7c5e319u;
    union_source.value = 0x13579bdfu;
    initialized_structure = structure_source;
    zero_initialized_union = union_source;
    return initialized_structure.value == structure_source.value &&
        zero_initialized_union.value == union_source.value ? 0 : 1;
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
        local_volatile_aggregate_atomic() == 0 &&
        local_volatile_union_atomic() == 0 &&
        local_volatile_aggregate_initialized() == 0 &&
        local_volatile_aggregate_copy() == 0 &&
        static_volatile_aggregate_copy() == 0 &&
#if defined(__x86_64__)
        wide_aggregate_atomic_suite() == 0 &&
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
