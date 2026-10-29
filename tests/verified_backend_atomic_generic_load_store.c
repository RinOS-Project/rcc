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
