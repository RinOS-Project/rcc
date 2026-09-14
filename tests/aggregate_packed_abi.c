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
