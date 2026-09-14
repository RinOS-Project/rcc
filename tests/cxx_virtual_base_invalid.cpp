class VirtualBase {
public:
    int value;
};

class InvalidVirtualDerived : virtual public VirtualBase {
public:
    int extra;
};

int main(void) {
    InvalidVirtualDerived value;
    return value.value + value.extra;
}
