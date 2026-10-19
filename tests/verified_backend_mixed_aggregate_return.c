struct VerifiedMixedIntegerSseReturn {
    unsigned long long integer;
    double value;
};

struct VerifiedMixedSseIntegerReturn {
    double value;
    unsigned long long integer;
};

struct VerifiedMixedIntegerSseReturn verified_mixed_integer_sse_round_trip(
    struct VerifiedMixedIntegerSseReturn value)
{
    return value;
}

unsigned long long verified_mixed_integer_sse_call_integer(
    struct VerifiedMixedIntegerSseReturn value)
{
    return verified_mixed_integer_sse_round_trip(value).integer;
}

double verified_mixed_integer_sse_call_value(
    struct VerifiedMixedIntegerSseReturn value)
{
    return verified_mixed_integer_sse_round_trip(value).value;
}

struct VerifiedMixedSseIntegerReturn verified_mixed_sse_integer_round_trip(
    struct VerifiedMixedSseIntegerReturn value)
{
    return value;
}

double verified_mixed_sse_integer_call_value(
    struct VerifiedMixedSseIntegerReturn value)
{
    return verified_mixed_sse_integer_round_trip(value).value;
}

unsigned long long verified_mixed_sse_integer_call_integer(
    struct VerifiedMixedSseIntegerReturn value)
{
    return verified_mixed_sse_integer_round_trip(value).integer;
}
