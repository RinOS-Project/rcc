struct VirtualValueRoot {
    int value;
};

struct VirtualValueArm : virtual VirtualValueRoot {};
struct FixedValueArm : VirtualValueRoot {};
struct AmbiguousVirtualValue : VirtualValueArm, FixedValueArm {};

int consume_virtual_value_root(VirtualValueRoot value) {
    return value.value;
}

int main() {
    AmbiguousVirtualValue value;
    return consume_virtual_value_root(value);
}
