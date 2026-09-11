struct VerifiedStraddledArgument {
    int first;
    int second;
    int third;
};

int verified_aggregate_register_straddle_fallback(
    int first, int second, int third, int fourth, int fifth,
    struct VerifiedStraddledArgument value)
{
    return first + second + third + fourth + fifth +
        value.first + value.second + value.third;
}
