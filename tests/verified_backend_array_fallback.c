union VerifiedAggregateFallback {
    int first;
    int second;
};

int verified_union_initializer_fallback(void)
{
    union VerifiedAggregateFallback value = {.first = 7};
    return value.first;
}
