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
        object_address_evaluations == 2u &&
        value_address_evaluations == 2u &&
        store_order_evaluations == 1u &&
        load_order_evaluations == 1u &&
        exchange_object_address_evaluations == 1u &&
        exchange_source_address_evaluations == 1u &&
        exchange_result_address_evaluations == 1u &&
        exchange_order_evaluations == 1u ? 0 : 1;
}
