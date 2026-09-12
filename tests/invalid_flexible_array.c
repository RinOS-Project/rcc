struct OnlyFlexible {
    int data[];
};

struct FlexibleNotLast {
    int data[];
    int length;
};

union UnionFlexible {
    int data[];
};

struct FlexiblePacket {
    int length;
    char data[];
};

struct FlexiblePacket invalid_initializer = {7, {1}};
