struct KnownInheritedBase {
    explicit KnownInheritedBase(int value) : value_(value) {}
    int value_;
};

struct NotABase {
    explicit NotABase(int value) : value_(value) {}
    int value_;
};

struct UnknownInheritedDerived : public KnownInheritedBase {
    using NotABase::NotABase;
};
