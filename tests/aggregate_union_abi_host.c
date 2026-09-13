union AggregateUnionAbi {
    double floating;
    int integer;
};

extern int aggregate_union_integer(union AggregateUnionAbi value);

int main(void)
{
    union AggregateUnionAbi value;
    value.integer = 73;
    return aggregate_union_integer(value) == 73 ? 0 : 1;
}
