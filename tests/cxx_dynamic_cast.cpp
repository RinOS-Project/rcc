class LeftDynamic {
public:
    virtual int left_value() { return 11; }
};

class RightDynamic {
public:
    virtual int right_value() { return 22; }
};

class BothDynamic : public LeftDynamic, public RightDynamic {
public:
    int right_value() override { return 33; }
};

extern "C" int main() {
    BothDynamic local{};
    BothDynamic* derived = &local;
    BothDynamic* null_derived = 0;
    RightDynamic* right = dynamic_cast<RightDynamic*>(derived);
    RightDynamic* null_right = dynamic_cast<RightDynamic*>(null_derived);
    return right && right->right_value() == 33 && null_right == 0 ? 0 : 1;
}
