class LeftVirtual {
public:
    virtual int left_value() { return 11; }
};

class RightVirtual {
public:
    virtual int right_value() { return 22; }
};

class BothVirtual : public LeftVirtual, public RightVirtual {
public:
    int right_value() override { return 33; }
};

int main() {
    BothVirtual local{};
    RightVirtual* right = &local;
    return right->right_value() == 33 && local.right_value() == 33 ? 0 : 1;
}
