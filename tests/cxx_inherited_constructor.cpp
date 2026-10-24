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

struct InheritedTemplateBase {
    template <typename T>
    explicit InheritedTemplateBase(T value)
        : value_(value + sizeof(T)) {
        value_ += 1;
    }

    int value_;
};

struct InheritedTemplateDerived : InheritedTemplateBase {
    using InheritedTemplateBase::InheritedTemplateBase;
};

extern "C" int cxx_inherited_constructor_probe(void) {
    InheritedDerived one(7);
    InheritedDerived two(3, 4);
    InheritedDerivedWithClassDefault three(1);
    InheritedTemplateDerived four{10};
    InheritedTemplateDerived five{'A'};
    InheritedTemplateDerived six(5);
    return one.value_ + two.value_ + three.value_ + three.member_.value_ +
           four.value_ + five.value_ + six.value_;
}
