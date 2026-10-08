struct InheritedBase {
    explicit InheritedBase(int value) : value_(value) {}
    InheritedBase(int left, int right) : value_(left + right) {}

    int value_;
};

struct InheritedDerived : InheritedBase {
    using InheritedBase::InheritedBase;
};

struct InheritedClassDefaultMember {
    InheritedClassDefaultMember() : value_(9) {}

    int value_;
};

struct InheritedDerivedWithClassDefault : InheritedBase {
    using InheritedBase::InheritedBase;
    InheritedClassDefaultMember member_{};
};

extern "C" int cxx_inherited_constructor_probe(void) {
    InheritedDerived one(7);
    InheritedDerived two(3, 4);
    InheritedDerivedWithClassDefault three(1);
    return one.value_ + two.value_ + three.value_ + three.member_.value_;
}
