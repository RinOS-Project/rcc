struct FlexiblePrefix {
    int value;
    int tail[];
};

int flexible_prefix_value(struct FlexiblePrefix value)
{
    return value.value;
}

struct FlexiblePrefix flexible_prefix_round_trip(struct FlexiblePrefix value)
{
    return value;
}
