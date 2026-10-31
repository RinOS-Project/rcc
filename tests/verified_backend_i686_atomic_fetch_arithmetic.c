unsigned long long verified_i686_atomic_fetch_add(
    volatile unsigned long long* object,
    unsigned long long operand,
    int order)
{
    return __atomic_fetch_add(object, operand, order);
}

unsigned long long verified_i686_atomic_fetch_sub(
    volatile unsigned long long* object,
    unsigned long long operand,
    int order)
{
    return __atomic_fetch_sub(object, operand, order);
}

unsigned long long verified_i686_atomic_add_fetch(
    volatile unsigned long long* object,
    unsigned long long operand,
    int order)
{
    return __atomic_add_fetch(object, operand, order);
}

unsigned long long verified_i686_atomic_sub_fetch(
    volatile unsigned long long* object,
    unsigned long long operand,
    int order)
{
    return __atomic_sub_fetch(object, operand, order);
}

unsigned long long verified_i686_sync_fetch_add(
    volatile unsigned long long* object,
    unsigned long long operand)
{
    return __sync_fetch_and_add(object, operand);
}

unsigned long long verified_i686_sync_add_fetch(
    volatile unsigned long long* object,
    unsigned long long operand)
{
    return __sync_add_and_fetch(object, operand);
}

unsigned long long verified_i686_sync_fetch_sub(
    volatile unsigned long long* object,
    unsigned long long operand)
{
    return __sync_fetch_and_sub(object, operand);
}

unsigned long long verified_i686_sync_sub_fetch(
    volatile unsigned long long* object,
    unsigned long long operand)
{
    return __sync_sub_and_fetch(object, operand);
}
