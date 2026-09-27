struct InheritedBase {
    explicit InheritedBase(int value) : value_(value) {}
    InheritedBase(int left, int right) : value_(left + right) {}

    int value_;
};

struct InheritedDerived : InheritedBase {
    using InheritedBase::InheritedBase;
};

extern "C" int cxx_inherited_constructor_probe(void) {
    InheritedDerived one(7);
    InheritedDerived two(3, 4);
    return one.value_ + two.value_;
}
