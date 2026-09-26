class OverrideBase {
public:
    virtual int value() { return 1; }
    virtual int final_value() final { return 2; }
};

class InvalidName : public OverrideBase {
public:
    int other() override { return 3; }
};

class InvalidSignature : public OverrideBase {
public:
    int value(int unused) override { return unused; }
};

class InvalidFinal : public OverrideBase {
public:
    int final_value() override { return 4; }
};
