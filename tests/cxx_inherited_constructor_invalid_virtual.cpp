struct VirtualInheritedBase {
    explicit VirtualInheritedBase(int value) : value_(value) {}
    int value_;
};

struct VirtualInheritedDerived : public virtual VirtualInheritedBase {
    using VirtualInheritedBase::VirtualInheritedBase;
};
