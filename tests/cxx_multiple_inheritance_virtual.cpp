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
    int payload;
};

BothVirtual global_both;

int main() {
    BothVirtual local{};
    LeftVirtual* left = &local;
    RightVirtual* right = &local;
    RightVirtual* global_right = &global_both;
    return left->left_value() == 11 && right->right_value() == 22 &&
                   global_right->right_value() == 22 ? 0 : 1;
}
