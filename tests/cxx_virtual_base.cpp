class VirtualBase {
public:
    int value;

    int base_sum(int extra) { return value + extra; }
};

class VirtualDerived : virtual public VirtualBase {
public:
    int extra;
};

int main() {
    VirtualDerived value;
    value.value = 7;
    value.extra = 5;
    VirtualBase* base = &value;
    return base->value == 7 && value.base_sum(4) == 11 &&
                   base->base_sum(3) == 10 ? 0 : 1;
}
