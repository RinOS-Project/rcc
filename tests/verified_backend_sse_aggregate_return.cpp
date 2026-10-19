struct VerifiedSseAggregateReturn {
    double value;
};

struct VerifiedSseAggregateFloatReturn {
    float value;
};

struct VerifiedMixedIntegerSseReturn {
    unsigned long long integer;
    double value;
};

struct VerifiedMixedSseIntegerReturn {
    double value;
    unsigned long long integer;
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

extern "C" VerifiedMixedIntegerSseReturn
verified_mixed_integer_sse_round_trip(VerifiedMixedIntegerSseReturn value)
{
    return value;
}

extern "C" unsigned long long verified_mixed_integer_sse_call_integer(
    VerifiedMixedIntegerSseReturn value)
{
    return verified_mixed_integer_sse_round_trip(value).integer;
}

extern "C" double verified_mixed_integer_sse_call_value(
    VerifiedMixedIntegerSseReturn value)
{
    return verified_mixed_integer_sse_round_trip(value).value;
}

extern "C" VerifiedMixedSseIntegerReturn
verified_mixed_sse_integer_round_trip(VerifiedMixedSseIntegerReturn value)
{
    return value;
}

extern "C" double verified_mixed_sse_integer_call_value(
    VerifiedMixedSseIntegerReturn value)
{
    return verified_mixed_sse_integer_round_trip(value).value;
}

extern "C" unsigned long long verified_mixed_sse_integer_call_integer(
    VerifiedMixedSseIntegerReturn value)
{
    return verified_mixed_sse_integer_round_trip(value).integer;
}
