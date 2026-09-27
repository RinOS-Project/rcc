struct PrivateInheritedBase {
    explicit PrivateInheritedBase(int value) : value_(value) {}
    int value_;
};

struct PrivateInheritedDerived : private PrivateInheritedBase {
    using PrivateInheritedBase::PrivateInheritedBase;
};
