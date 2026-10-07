volatile int verified_volatile_global;

struct VerifiedVolatilePair {
    int first;
    int second;
};

int verified_volatile_local(int value)
{
    volatile int local = value;
    local = local + 1;
    return local + local;
}

int verified_volatile_pointer(volatile int* pointer)
{
    int first = *pointer;
    int second = *pointer;
    *pointer = first + second;
    return *pointer;
}

int verified_volatile_pointer_object(volatile int* pointer)
{
    volatile int* volatile current = pointer;
    *current += 1;
    return *current;
}

int verified_volatile_global_access(int value)
{
    verified_volatile_global = value;
    return verified_volatile_global;
}

int verified_volatile_aggregate_member(
    volatile struct VerifiedVolatilePair* pair, int value)
{
    pair->first = value;
    return pair->first + pair->second;
}
