int target(void);

int first_value = 11;
extern int second_value;
int second_value = 22;
int second_value;
extern int zero_value;
int zero_value;
int zero_value;
int static_values[2];
char* static_literal = "StaticRinOS";
char* static_suffix = "StaticRinOS" + 6;
int* static_zero = &zero_value;
int* static_second = static_values + 1;
int (*static_target)(void) = &target;
int* static_null = (int*)0;
char static_array[] = "ArrayRin";
char static_fixed[12] = "Fixed";
int static_constant = (3 + 5) * 4 - 2;
int static_logic = (1 < 2) && (0 || 7);
int static_bits = (((3 << 4) | 5) ^ 1) + ((~0) & 3);
int static_choice = 0 ? 9 : sizeof(int);
_Bool static_bool = 7;
int static_unary = -5 + !0;

int aggregate_target = 41;
int aggregate_scalar = {13};
char aggregate_braced_string[] = {"Brace"};
int aggregate_numbers[] = {1, [3] = 7, 9};
int* aggregate_pointers[3] = {&aggregate_target,
                              [2] = &aggregate_target};

struct AggregateInitializer {
    int first;
    char marker;
    int* pointer;
};

struct AggregateInitializer aggregate_record = {
    .marker = 'R',
    .pointer = &aggregate_target,
    .first = 12,
};

struct AggregateContainer {
    int values[4];
    struct AggregateInitializer record;
};

struct AggregateContainer aggregate_nested = {
    .values = {1, [3] = 4},
    .record = {
        .first = 5,
        .marker = 'N',
        .pointer = &aggregate_target,
    },
};

struct AggregateContainer aggregate_path = {
    .record.pointer = &aggregate_target,
    .values[2] = 17,
};

union AggregateUnion {
    int number;
    char bytes[4];
};

union AggregateUnion aggregate_union = {.number = 6};

struct LocalAggregate {
    int first;
    int second;
    char third;
};

int target(void)
{
    return second_value;
}

unsigned long target_address(void)
{
    return (unsigned long)target;
}

int* zero_address(void)
{
    return &zero_value;
}

char* literal_address(void)
{
    return "RinOS";
}

int* pointer_add(int* base, int count)
{
    return base + count;
}

int* integer_add(int count, int* base)
{
    return count + base;
}

long pointer_distance(int* first, int* last)
{
    return last - first;
}

int* pointer_update(int* value)
{
    value++;
    value -= 1;
    ++value;
    return value;
}

int local_array_value(void)
{
    char inferred[] = "LocalRin";
    char padded[12] = "Pad";
    return inferred[5] + padded[3] + padded[11];
}

int large_local_array_value(void)
{
    char large[320] = "Large";
    return large[0] + large[319];
}

int aggregate_parameter_value(struct LocalAggregate value)
{
    char scratch[32] = "slot";
    return value.first + value.second + value.third + scratch[31];
}

int local_aggregate_initializer_value(void)
{
    int target_value = 11;
    int scalar = {7};
    int values[] = {2, [3] = 8, 10};
    int* pointers[2] = {&target_value, [1] = &target_value};
    struct AggregateInitializer record = {
        .pointer = &target_value,
        .marker = 'L',
        .first = 3,
    };
    struct AggregateContainer nested = {
        .values = {1, [3] = 4},
        .record = {
            .first = 5,
            .marker = 'N',
            .pointer = &target_value,
        },
    };
    struct AggregateContainer path = {
        .record.pointer = &target_value,
        .values[2] = 9,
    };
    union AggregateUnion choice = {.number = 6};
    return values[0] + values[1] + values[3] + values[4] +
           *pointers[0] + *pointers[1] + record.first + record.marker +
           *record.pointer + nested.values[0] + nested.values[1] +
           nested.values[3] + nested.record.first + nested.record.marker +
           *nested.record.pointer + choice.number + scalar +
           *path.record.pointer + path.values[2];
}

int main(void)
{
    return *(&second_value) + *zero_address() + (target_address() != 0) +
           (literal_address() != 0) + (static_literal[0] == 'S') +
           (static_suffix[0] == 'R') + *static_zero + *static_second +
           (static_target() != 0) + (static_null == 0) +
           (static_array[0] == 'A') + (static_fixed[5] == 0) +
           static_constant + static_logic + static_bits + static_choice +
           static_bool + static_unary + local_array_value() +
           large_local_array_value() + local_aggregate_initializer_value() +
           aggregate_numbers[1] + aggregate_numbers[4] +
           *aggregate_pointers[2] + aggregate_record.first +
           aggregate_nested.values[3] + aggregate_union.number;
}
