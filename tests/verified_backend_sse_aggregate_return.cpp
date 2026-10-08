struct VerifiedSseAggregateReturn {
    double value;
};

struct VerifiedSseAggregateFloatReturn {
    float value;
};

extern "C" VerifiedSseAggregateReturn verified_sse_aggregate_round_trip(
    VerifiedSseAggregateReturn value)
{
    return value;
}

extern "C" double verified_sse_aggregate_call_value(
    VerifiedSseAggregateReturn value)
{
    return verified_sse_aggregate_round_trip(value).value;
}

extern "C" VerifiedSseAggregateFloatReturn
verified_sse_aggregate_float_round_trip(
    VerifiedSseAggregateFloatReturn value)
{
    return value;
}

extern "C" float verified_sse_aggregate_float_call_value(
    VerifiedSseAggregateFloatReturn value)
{
    return verified_sse_aggregate_float_round_trip(value).value;
}

extern "C" VerifiedSseAggregateReturn
verified_sse_aggregate_conditional(
    int select_first, VerifiedSseAggregateReturn first,
    VerifiedSseAggregateReturn second)
{
    return select_first ? first : second;
}

extern "C" VerifiedSseAggregateFloatReturn
verified_sse_aggregate_float_conditional(
    int select_first, VerifiedSseAggregateFloatReturn first,
    VerifiedSseAggregateFloatReturn second)
{
    return select_first ? first : second;
}
