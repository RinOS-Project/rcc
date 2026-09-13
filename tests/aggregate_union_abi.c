union AggregateUnionAbi {
    double floating;
    int integer;
};

int aggregate_union_integer(union AggregateUnionAbi value)
{
    return value.integer;
}
