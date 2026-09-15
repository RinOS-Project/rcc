class LeftDynamicVerified {
public:
    virtual int left_value() { return 11; }
};

class RightDynamicVerified {
public:
    virtual int right_value() { return 22; }
};

class BothDynamicVerified : public LeftDynamicVerified,
                            public RightDynamicVerified {};

extern "C" RightDynamicVerified* cast_right_dynamic(
    BothDynamicVerified* value) {
    return dynamic_cast<RightDynamicVerified*>(value);
}
