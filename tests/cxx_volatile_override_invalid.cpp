struct VolatileOverrideBase {
    virtual int value() volatile {
        return 1;
    }
};

struct VolatileOverrideMismatch : VolatileOverrideBase {
    int value() const volatile override {
        return 2;
    }
};
