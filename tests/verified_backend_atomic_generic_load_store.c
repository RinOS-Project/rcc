int main(void)
{
    int pointees[2] = {0, 0};
    volatile unsigned int object = 0x12345678u;
    unsigned int source = 0x87654321u;
    unsigned int result = 0u;
    int * volatile pointer_object = &pointees[0];
    int *pointer_source = &pointees[1];
    int *pointer_result = &pointees[0];
    unsigned int object_address_evaluations = 0u;
    unsigned int value_address_evaluations = 0u;
    unsigned int store_order_evaluations = 0u;
    unsigned int load_order_evaluations = 0u;

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

    return object == source && result == source &&
        pointer_object == pointer_source &&
        pointer_result == pointer_source &&
        object_address_evaluations == 2u &&
        value_address_evaluations == 2u &&
        store_order_evaluations == 1u &&
        load_order_evaluations == 1u ? 0 : 1;
}
