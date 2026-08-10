struct OverridePair {
    int first;
    int second;
};

union OverrideChoice {
    int number;
    char byte;
};

struct OverrideContainer {
    int values[2];
    struct OverridePair pair;
};

int override_array[4] = {1, 2, [0] = 7, [1] = 8, [0] = 9};

struct OverridePair override_pair = {
    .first = 1,
    .second = 2,
    .first = 7,
    .second = 8,
};

union OverrideChoice override_choice = {
    .number = 0x11223344,
    .byte = 'R',
};

struct OverrideContainer override_nested = {
    .values[0] = 2,
    .values[0] = 4,
    .pair.first = 5,
    .pair.first = 6,
};

int local_initializer_override(void)
{
    int values[3] = {1, 2, [0] = 5, [1] = 6};
    struct OverridePair pair = {
        .first = 1,
        .second = 2,
        .first = 7,
        .second = 8,
    };
    union OverrideChoice choice = {
        .number = 0x11223344,
        .byte = 3,
    };
    struct OverrideContainer nested = {
        .values[0] = 2,
        .values[0] = 4,
        .pair.first = 5,
        .pair.first = 6,
    };
    return values[0] + values[1] + values[2] +
           pair.first + pair.second + choice.byte +
           nested.values[0] + nested.values[1] +
           nested.pair.first + nested.pair.second;
}
