struct DynamicBase {
    virtual int value() { return 1; }
};

struct DynamicDerived : DynamicBase {
    int value() override { return 2; }
};

int main(void) {
    DynamicBase* base = 0;
    DynamicDerived* derived = dynamic_cast<DynamicDerived*>(base);
    return derived != 0;
}
