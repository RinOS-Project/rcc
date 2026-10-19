struct VerifiedSsePairDouble {
    double first;
    double second;
};

struct VerifiedSseTripleFloat {
    float first;
    float second;
    float third;
};

extern "C" VerifiedSsePairDouble verified_sse_pair_double_round_trip(
    VerifiedSsePairDouble value)
{
    return value;
}

extern "C" VerifiedSsePairDouble verified_sse_pair_double_conditional(
    int select_first, VerifiedSsePairDouble first,
    VerifiedSsePairDouble second)
{
    return select_first ? first : second;
}

extern "C" double verified_sse_pair_double_first(
    VerifiedSsePairDouble value)
{
    return verified_sse_pair_double_round_trip(value).first;
}

extern "C" double verified_sse_pair_double_second(
    VerifiedSsePairDouble value)
{
    return verified_sse_pair_double_round_trip(value).second;
}

extern "C" VerifiedSseTripleFloat verified_sse_triple_float_round_trip(
    VerifiedSseTripleFloat value)
{
    return value;
}

extern "C" VerifiedSseTripleFloat verified_sse_triple_float_conditional(
    int select_first, VerifiedSseTripleFloat first,
    VerifiedSseTripleFloat second)
{
    return select_first ? first : second;
}

extern "C" float verified_sse_triple_float_first(
    VerifiedSseTripleFloat value)
{
    return verified_sse_triple_float_round_trip(value).first;
}

extern "C" float verified_sse_triple_float_third(
    VerifiedSseTripleFloat value)
{
    return verified_sse_triple_float_round_trip(value).third;
}
