class BaseReferenceCast {
public:
    virtual int value() { return 11; }
};

class DerivedReferenceCast : public BaseReferenceCast {
public:
    int value() override { return 42; }
};

extern "C" int main() {
    DerivedReferenceCast derived{};
    BaseReferenceCast& derived_base = derived;
    try {
        DerivedReferenceCast& recovered =
            dynamic_cast<DerivedReferenceCast&>(derived_base);
        if (recovered.value() != 42) return 1;
    } catch (...) {
        return 2;
    }

    BaseReferenceCast plain{};
    try {
        (void)dynamic_cast<DerivedReferenceCast&>(plain);
        return 3;
    } catch (...) {
        return 0;
    }
}
