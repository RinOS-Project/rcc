struct FlexiblePacket {
    int length;
    char data[];
};

struct FlexiblePacket global_packet = {7};

int flexible_local(void)
{
    struct FlexiblePacket packet = {9};
    return packet.length;
}

int flexible_size(void)
{
    return sizeof(struct FlexiblePacket);
}
