class LeftVirtual {
public:
    virtual int left_value() { return 11; }
};

class RightVirtual {
public:
    virtual int right_value() { return 22; }
};

class InvalidBoth : public LeftVirtual, public RightVirtual {
public:
    int right_value() override { return 33; }
};

int main(void) {
    InvalidBoth value;
    return value.right_value();
}
