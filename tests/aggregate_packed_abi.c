#pragma pack(push, 1)
struct PackedArgument {
    unsigned char tag;
    long long value;
};

struct PackedReturn {
    unsigned char tag;
    long long value;
};
#pragma pack(pop)

int packed_argument(struct PackedArgument value)
{
    return (int)value.tag + (int)value.value;
}

struct PackedReturn packed_return(int tag, long long value)
{
    struct PackedReturn result;
    result.tag = (unsigned char)tag;
    result.value = value;
    return result;
}

int main(void)
{
    struct PackedArgument argument = { 7u, 35ll };
    struct PackedReturn result = packed_return(9, 123456789ll);
    if (packed_argument(argument) != 42 ||
        result.tag != 9u || result.value != 123456789ll) {
        return 1;
    }
    return 0;
}
