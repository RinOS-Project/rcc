struct MemberInheritedBase {
    explicit MemberInheritedBase(int value) : value_(value) {}
    int value_;
};

struct MemberInheritedDerived : public MemberInheritedBase {
    using MemberInheritedBase::MemberInheritedBase;
    int extra_[2] = {1, 2};
};
