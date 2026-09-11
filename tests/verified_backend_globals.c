int verified_global_data = 7;
int verified_global_zero;
static int verified_static_data = 5;
extern int verified_external_data;
int verified_global_array[3] = {4, 5, 6};

struct VerifiedGlobalPair {
    int first;
    int values[2];
};

struct VerifiedGlobalHalves {
    unsigned short low;
    unsigned short high;
};

union VerifiedGlobalWord {
    unsigned int bits;
    struct VerifiedGlobalHalves halves;
};

struct VerifiedGlobalPair verified_global_pair = {8, {9, 10}};
union VerifiedGlobalWord verified_global_word = {.bits = 0x00030002u};

int verified_global_read(void)
{
    return verified_global_data + verified_global_zero +
        verified_static_data;
}

int verified_global_write(int value)
{
    verified_global_data = value;
    verified_global_zero = value + 1;
    verified_static_data += 2;
    return verified_global_read();
}

int verified_external_read(void)
{
    return verified_external_data;
}

int verified_global_array_read(int index)
{
    return *(verified_global_array + index);
}

int verified_global_array_write(int index, int value)
{
    verified_global_array[index] = value;
    return verified_global_array[index];
}

int verified_string_read(int index)
{
    const char* first = "RinOS";
    const char* second = "RinOS";
    return first[index] + second[index];
}

int verified_global_aggregate_read(int index)
{
    struct VerifiedGlobalPair copy = verified_global_pair;
    return copy.first * 100 + copy.values[index] +
        verified_global_word.halves.high;
}

int verified_global_aggregate_write(int value)
{
    verified_global_pair.values[0] = value;
    verified_global_word.bits = 0x00050004u;
    return verified_global_pair.first * 100 +
        verified_global_pair.values[0] * 10 +
        verified_global_word.halves.low +
        verified_global_word.halves.high;
}
