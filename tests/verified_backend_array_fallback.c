struct VerifiedAggregateFallback {
    int first;
    int second;
};

struct VerifiedAggregateFallback verified_aggregate_return_fallback(void)
{
    struct VerifiedAggregateFallback value = {1, 2};
    return value;
}
