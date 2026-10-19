struct VerifiedSsePairDouble {
    double first;
    double second;
};

struct VerifiedSseTripleFloat {
    float first;
    float second;
    float third;
};

struct VerifiedSsePairDouble verified_sse_pair_double_round_trip(
    struct VerifiedSsePairDouble value)
{
    return value;
}

struct VerifiedSsePairDouble verified_sse_pair_double_conditional(
    int select_first, struct VerifiedSsePairDouble first,
    struct VerifiedSsePairDouble second)
{
    return select_first ? first : second;
}

double verified_sse_pair_double_first(
    struct VerifiedSsePairDouble value)
{
    return verified_sse_pair_double_round_trip(value).first;
}

double verified_sse_pair_double_second(
    struct VerifiedSsePairDouble value)
{
    return verified_sse_pair_double_round_trip(value).second;
}

struct VerifiedSseTripleFloat verified_sse_triple_float_round_trip(
    struct VerifiedSseTripleFloat value)
{
    return value;
}

struct VerifiedSseTripleFloat verified_sse_triple_float_conditional(
    int select_first, struct VerifiedSseTripleFloat first,
    struct VerifiedSseTripleFloat second)
{
    return select_first ? first : second;
}

float verified_sse_triple_float_first(
    struct VerifiedSseTripleFloat value)
{
    return verified_sse_triple_float_round_trip(value).first;
}

float verified_sse_triple_float_third(
    struct VerifiedSseTripleFloat value)
{
    return verified_sse_triple_float_round_trip(value).third;
}
