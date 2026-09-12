struct FlexiblePacket {
    int length;
    char data[];
};

struct FlexiblePacket invalid_initializer = {7, {1}};
