class VirtualBase {
public:
    virtual int value() { return 11; }
};

class VirtualDerived : virtual public VirtualBase {
public:
    int value() override { return 33; }
};

int main() {
    VirtualDerived object{};
    VirtualBase* base = &object;
    return base->value() == 33 && object.value() == 33 ? 0 : 1;
}
