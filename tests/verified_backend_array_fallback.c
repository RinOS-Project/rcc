struct VerifiedAggregateFallback {
    int first;
    int second;
};

int verified_struct_initializer_fallback(void)
{
    struct VerifiedAggregateFallback value = {1, 2};
    return value.first;
}
