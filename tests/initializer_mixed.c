struct MixedAggregate {
    int values[3];
    int tail;
};

struct MixedInner {
    int values[2];
    int tail;
};

struct MixedOuter {
    struct MixedInner inner;
    int tail;
};

struct MixedLeadingOuter {
    int first;
    struct MixedInner inner;
    int tail;
};

struct MixedMatrixHolder {
    int values[2][2];
    int tail;
};

char string_rows[2][4] = {"a", "bc"};
char mixed_string_rows[2][4] = {"a", 'b', 'c'};
char inferred_string_rows[][4] = {"a", "bc"};
struct MixedAggregate mixed_designators = {
    /* The following positional clause continues at values[2], not tail. */
    .values[1] = 5,
    7,
};
struct MixedOuter mixed_nested_designators = {
    .inner.values[1] = 5,
    7,
    8,
};
struct MixedLeadingOuter mixed_designator_before_aggregate = {
    .first = 1,
    2,
    3,
    4,
    5,
};
struct MixedMatrixHolder mixed_nested_array_designator = {
    .values[0][1] = 6,
    7,
    8,
    9,
};
int mixed_array[4] = {[2] = 3, 4, [0] = 1, 2};

int mixed_initializer_local(void)
{
    char local_rows[2][4] = {"xy", "z"};
    char large_rows[2][1024];
    struct MixedAggregate local = {
        .values = {[2] = 9},
        11,
    };
    struct MixedAggregate local_nested_designator = {
        .values[1] = 12,
        13,
    };
    struct MixedOuter local_deep_nested_designator = {
        .inner.values[1] = 14,
        15,
        16,
    };
    struct MixedOuter local_deep_nested_elision = {
        .inner.values[0] = 17,
        18,
        19,
        20,
    };
    struct MixedLeadingOuter local_designator_before_aggregate = {
        .first = 21,
        22,
        23,
        24,
        25,
    };
    struct MixedMatrixHolder local_nested_array_designator = {
        .values[0][1] = 26,
        27,
        28,
        29,
    };
    large_rows[1][1023] = 1;
    return local_rows[0][0] + local_rows[0][1] + local_rows[1][0] +
           local.values[0] + local.values[1] + local.values[2] + local.tail +
           local_nested_designator.values[1] +
           local_nested_designator.values[2] +
           local_nested_designator.tail +
           local_deep_nested_designator.inner.values[1] +
           local_deep_nested_designator.inner.tail +
           local_deep_nested_designator.tail +
           local_deep_nested_elision.inner.values[0] +
           local_deep_nested_elision.inner.values[1] +
           local_deep_nested_elision.inner.tail +
           local_deep_nested_elision.tail +
           local_designator_before_aggregate.first +
           local_designator_before_aggregate.inner.values[0] +
           local_designator_before_aggregate.inner.values[1] +
           local_designator_before_aggregate.inner.tail +
           local_designator_before_aggregate.tail +
           local_nested_array_designator.values[0][1] +
           local_nested_array_designator.values[1][0] +
           local_nested_array_designator.values[1][1] +
           local_nested_array_designator.tail +
           large_rows[1][1023];
}
