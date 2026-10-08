struct VerifiedAggregateFallback {
    double value;
};

struct VerifiedAggregateFloatFallback {
    float value;
};

struct VerifiedAggregateFallback verified_aggregate_return_fallback(
    struct VerifiedAggregateFallback value)
{
    return value;
}

double verified_aggregate_return_call_value(
    struct VerifiedAggregateFallback value)
{
    return verified_aggregate_return_fallback(value).value;
}

struct VerifiedAggregateFloatFallback
verified_aggregate_float_return_fallback(
    struct VerifiedAggregateFloatFallback value)
{
    return value;
}

float verified_aggregate_float_call_value(
    struct VerifiedAggregateFloatFallback value)
{
    return verified_aggregate_float_return_fallback(value).value;
}
