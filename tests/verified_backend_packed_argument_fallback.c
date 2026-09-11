#pragma pack(push, 1)
struct VerifiedPackedArgument {
    unsigned char tag;
    int value;
};
#pragma pack(pop)

int verified_packed_argument_fallback(struct VerifiedPackedArgument value)
{
    return (int)value.tag + value.value;
}
