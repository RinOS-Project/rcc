struct MixedAggregate {
    int values[3];
    int tail;
};

char string_rows[2][4] = {"a", "bc"};
char mixed_string_rows[2][4] = {"a", 'b', 'c'};
char inferred_string_rows[][4] = {"a", "bc"};
struct MixedAggregate mixed_designators = {
    .values[1] = 5,
    7,
};
int mixed_array[4] = {[2] = 3, 4, [0] = 1, 2};

int mixed_initializer_local(void)
{
    char local_rows[2][4] = {"xy", "z"};
    struct MixedAggregate local = {
        .values = {[2] = 9},
        11,
    };
    return local_rows[0][0] + local_rows[0][1] + local_rows[1][0] +
           local.values[0] + local.values[1] + local.values[2] + local.tail;
}
