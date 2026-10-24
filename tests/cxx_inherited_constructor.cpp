extern "C" {
int cxx_inherited_template_order_state = 0;
int cxx_inherited_template_order_error = 0;
int cxx_inherited_template_order_complete = 0;
}

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
        if (cxx_inherited_template_order_state != 0) {
            cxx_inherited_template_order_error = 1;
        }
        cxx_inherited_template_order_state = 1;
    }

    int value_;
};

struct InheritedTemplateEmptyBase {
    template <typename T>
    explicit InheritedTemplateEmptyBase(T value)
        : value_(value + sizeof(T)) {}

    int value_;
};

struct InheritedTemplateMarker {
    InheritedTemplateMarker() {
        if (cxx_inherited_template_order_state < 1 ||
            cxx_inherited_template_order_state > 3) {
            cxx_inherited_template_order_error = 1;
        } else {
            cxx_inherited_template_order_state += 1;
            if (cxx_inherited_template_order_state == 4) {
                cxx_inherited_template_order_state = 0;
                cxx_inherited_template_order_complete += 1;
            }
        }
    }
};

struct InheritedTemplateDerived : InheritedTemplateBase {
    using InheritedTemplateBase::InheritedTemplateBase;
    int extra_ = 5;
    InheritedTemplateMarker marker_{};
    InheritedTemplateMarker markers_[2] = {};
};

struct InheritedTemplateEmptyDerived : InheritedTemplateEmptyBase {
    using InheritedTemplateEmptyBase::InheritedTemplateEmptyBase;
    int extra_ = 3;
};

extern "C" int cxx_inherited_constructor_probe(void) {
    InheritedDerived one(7);
    InheritedDerived two(3, 4);
    InheritedDerivedWithClassDefault three(1);
    InheritedTemplateDerived four{10};
    InheritedTemplateDerived five{'A'};
    InheritedTemplateDerived six(5);
    InheritedTemplateEmptyDerived seven{2};
    return one.value_ + two.value_ + three.value_ + three.member_.value_ +
           four.value_ + five.value_ + six.value_ + four.extra_ +
           five.extra_ + six.extra_ + seven.value_ + seven.extra_ +
           cxx_inherited_template_order_complete;
}
