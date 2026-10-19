struct VerifiedPartialArrayInput {
    int* order;
    int value;
};

class VerifiedPartialArrayElement final {
public:
    explicit VerifiedPartialArrayElement(
        const VerifiedPartialArrayInput* input) : input_(input) {}
    VerifiedPartialArrayElement() : input_(0) {}

    ~VerifiedPartialArrayElement() {
        if (input_) {
            *input_->order = *input_->order * 10 + input_->value;
        }
    }

    const VerifiedPartialArrayInput* input_;
};

class VerifiedThrowingPartialArrayElement final {
public:
    explicit VerifiedThrowingPartialArrayElement(
        const VerifiedPartialArrayInput* input) : input_(input) {
        if (input->value == 3) throw 43;
    }

    VerifiedThrowingPartialArrayElement() noexcept : input_(0) {}

    ~VerifiedThrowingPartialArrayElement() {
        if (input_) {
            *input_->order = *input_->order * 10 + input_->value;
        }
    }

    const VerifiedPartialArrayInput* input_;
};

extern "C" int verified_partial_array_next_ctor() noexcept;
extern "C" void verified_partial_array_record_destructor(int value);

static int verified_partial_array_throwing_default_argument() {
    int value = verified_partial_array_next_ctor();
    if (value == 3) throw 52;
    return value;
}

class VerifiedDynamicThrowingArrayElement final {
public:
    VerifiedDynamicThrowingArrayElement() : value_(0) {
        value_ = verified_partial_array_next_ctor();
        if (value_ == 3) throw 43;
    }

    ~VerifiedDynamicThrowingArrayElement() {
        verified_partial_array_record_destructor(value_);
    }

    int value_;
};

class VerifiedDynamicDefaultArgumentArrayElement final {
public:
    explicit VerifiedDynamicDefaultArgumentArrayElement(
        int value = verified_partial_array_next_ctor()) : value_(value) {}

    ~VerifiedDynamicDefaultArgumentArrayElement() noexcept {
        verified_partial_array_record_destructor(value_);
    }

    int value_;
};

class VerifiedImplicitDefaultArrayElement final {
public:
    ~VerifiedImplicitDefaultArrayElement() noexcept {
        verified_partial_array_record_destructor(1);
    }
};

class VerifiedImplicitDmiArrayElement final {
public:
    int value_ = 6;
    int default_initialized_only_by_lifetime_;

    ~VerifiedImplicitDmiArrayElement() noexcept {
        verified_partial_array_record_destructor(value_);
    }
};

struct VerifiedImplicitAggregateDmiPair {
    int first;
    int second;
};

class VerifiedImplicitAggregateDmiArrayElement final {
public:
    VerifiedImplicitAggregateDmiPair pair_ =
        VerifiedImplicitAggregateDmiPair{3, 4};
    int default_initialized_only_by_lifetime_;

    ~VerifiedImplicitAggregateDmiArrayElement() noexcept {
        verified_partial_array_record_destructor(pair_.first + pair_.second);
    }
};

class VerifiedNestedImplicitDmiMember final {
public:
    int first_ = 2;
    int second_ = 5;
    int default_initialized_only_by_lifetime_;
};

class VerifiedImplicitNestedDmiArrayElement final {
public:
    VerifiedNestedImplicitDmiMember nested_;
    int value_ = 1;
    int default_initialized_only_by_lifetime_;

    ~VerifiedImplicitNestedDmiArrayElement() noexcept {
        verified_partial_array_record_destructor(
            nested_.first_ + nested_.second_ + value_);
    }
};

class VerifiedImplicitDmiBase {
public:
    int base_value_ = 8;
};

class VerifiedImplicitBaseDmiArrayElement final
    : public VerifiedImplicitDmiBase {
public:
    int value_ = 1;

    ~VerifiedImplicitBaseDmiArrayElement() noexcept {
        verified_partial_array_record_destructor(base_value_ + value_);
    }
};

class VerifiedNestedVirtualDmiMember final {
public:
    int nested_value_ = 2;
};

class VerifiedImplicitVirtualDmiBase {
public:
    VerifiedNestedVirtualDmiMember nested_;
    int base_value_ = 3;
};

class VerifiedImplicitVirtualDmiArrayElement final
    : public virtual VerifiedImplicitVirtualDmiBase {
public:
    int value_ = 1;

    ~VerifiedImplicitVirtualDmiArrayElement() noexcept {
        verified_partial_array_record_destructor(
            nested_.nested_value_ + base_value_ + value_);
    }
};

class VerifiedNestedVirtualDmiRoot {
public:
    int root_value_ = verified_partial_array_next_ctor();
};

class VerifiedNestedVirtualDmiMiddle
    : public virtual VerifiedNestedVirtualDmiRoot {
public:
    int middle_value_ = 5;
};

class VerifiedNestedVirtualDmiArrayElement final
    : public virtual VerifiedNestedVirtualDmiMiddle {
public:
    int leaf_value_ = 3;

    ~VerifiedNestedVirtualDmiArrayElement() noexcept {
        verified_partial_array_record_destructor(
            root_value_ + middle_value_ + leaf_value_);
    }
};

class VerifiedSharedNestedVirtualDmiRoot {
public:
    int root_value_ = verified_partial_array_next_ctor();

    ~VerifiedSharedNestedVirtualDmiRoot() noexcept {
        verified_partial_array_record_destructor(root_value_);
    }
};

class VerifiedSharedNestedVirtualDmiLeft
    : public virtual VerifiedSharedNestedVirtualDmiRoot {
public:
    int left_value_ = 2;
};

class VerifiedSharedNestedVirtualDmiRight
    : public virtual VerifiedSharedNestedVirtualDmiRoot {
public:
    int right_value_ = 4;
};

class VerifiedSharedNestedVirtualDmiArrayElement final
    : public virtual VerifiedSharedNestedVirtualDmiLeft,
      public virtual VerifiedSharedNestedVirtualDmiRight {
public:
    int leaf_value_ = 8;

    ~VerifiedSharedNestedVirtualDmiArrayElement() noexcept {
        verified_partial_array_record_destructor(
            left_value_ + right_value_ + leaf_value_);
    }
};

class VerifiedNestedArrayDmiMember final {
public:
    int value_ = 5;
};

class VerifiedImplicitArrayDmiArrayElement final {
public:
    VerifiedNestedArrayDmiMember nested_[2];
    int value_ = 1;

    ~VerifiedImplicitArrayDmiArrayElement() noexcept {
        verified_partial_array_record_destructor(
            nested_[0].value_ + nested_[1].value_ + value_);
    }
};

class VerifiedNestedArrayDmiConstructorMember final {
public:
    int value_;

    VerifiedNestedArrayDmiConstructorMember() noexcept : value_(4) {}
};

class VerifiedImplicitArrayDmiConstructorElement final {
public:
    VerifiedNestedArrayDmiConstructorMember nested_;
    int value_ = 2;

    ~VerifiedImplicitArrayDmiConstructorElement() noexcept {
        verified_partial_array_record_destructor(nested_.value_ + value_);
    }
};

class VerifiedClassValueDmiMember final {
public:
    explicit VerifiedClassValueDmiMember(int value) noexcept : value_(value) {
        value_ += 2;
    }

    int value_;
};

class VerifiedImplicitClassValueDmiArrayElement final {
public:
    VerifiedClassValueDmiMember member_ = VerifiedClassValueDmiMember(3);

    ~VerifiedImplicitClassValueDmiArrayElement() noexcept {
        verified_partial_array_record_destructor(member_.value_);
    }
};

class VerifiedThrowingDefaultArgumentArrayElement final {
public:
    explicit VerifiedThrowingDefaultArgumentArrayElement(
        int value = verified_partial_array_throwing_default_argument())
        : value_(value) {}

    ~VerifiedThrowingDefaultArgumentArrayElement() noexcept {
        verified_partial_array_record_destructor(value_);
    }

    int value_;
};

static const VerifiedPartialArrayInput* verified_partial_array_argument(
    const VerifiedPartialArrayInput* input) {
    if (input->value == 3) throw 43;
    return input;
}

extern "C" int verified_cxx_partial_array_init() {
    int order = 0;
    VerifiedPartialArrayInput first{&order, 2};
    VerifiedPartialArrayInput second{&order, 3};
    VerifiedPartialArrayElement* values =
        new VerifiedPartialArrayElement[4]{&first, &second};
    int result = values[0].input_->value + values[1].input_->value +
        (values[2].input_ ? values[2].input_->value : 7) +
        (values[3].input_ ? values[3].input_->value : 7);
    delete[] values;
    return result + order * 100;
}

extern "C" int verified_cxx_default_array_init() {
    VerifiedPartialArrayElement* values =
        new VerifiedPartialArrayElement[3]{};
    int result = 0;
    for (int index = 0; index < 3; ++index) {
        result += values[index].input_ ? values[index].input_->value : 7;
    }
    delete[] values;
    return result;
}

extern "C" int verified_cxx_partial_array_init_throw() {
    int order = 0;
    VerifiedPartialArrayInput first{&order, 1};
    VerifiedPartialArrayInput second{&order, 2};
    VerifiedPartialArrayInput third{&order, 3};
    try {
        VerifiedPartialArrayElement* values =
            new VerifiedPartialArrayElement[4]{
                verified_partial_array_argument(&first),
                verified_partial_array_argument(&second),
                verified_partial_array_argument(&third)};
        delete[] values;
    } catch (int exception_value) {
        return exception_value + order * 1000;
    }
    return -1;
}

extern "C" int verified_cxx_partial_array_ctor_throw() {
    int order = 0;
    VerifiedPartialArrayInput first{&order, 1};
    VerifiedPartialArrayInput second{&order, 2};
    VerifiedPartialArrayInput third{&order, 3};
    try {
        VerifiedThrowingPartialArrayElement* values =
            new VerifiedThrowingPartialArrayElement[4]{
                &first, &second, &third};
        delete[] values;
    } catch (int exception_value) {
        return exception_value + order * 1000;
    }
    return -1;
}

extern "C" int verified_cxx_partial_array_ctor_normal() {
    int order = 0;
    VerifiedPartialArrayInput first{&order, 1};
    VerifiedPartialArrayInput second{&order, 2};
    VerifiedThrowingPartialArrayElement* values =
        new VerifiedThrowingPartialArrayElement[2]{&first, &second};
    delete[] values;
    return order;
}

extern "C" int verified_cxx_partial_array_dynamic(int count) {
    int order = 0;
    VerifiedPartialArrayInput first{&order, 1};
    VerifiedPartialArrayInput second{&order, 2};
    VerifiedThrowingPartialArrayElement* values =
        new VerifiedThrowingPartialArrayElement[count];
    if (count > 0) values[0].input_ = &first;
    if (count > 1) values[1].input_ = &second;
    delete[] values;
    return order;
}

extern "C" int verified_cxx_partial_array_dynamic_throw(int count) {
    try {
        VerifiedDynamicThrowingArrayElement* values =
            new VerifiedDynamicThrowingArrayElement[count];
        delete[] values;
    } catch (int exception_value) {
        return exception_value;
    }
    return -1;
}

extern "C" int verified_cxx_partial_array_default_argument(int count) {
    VerifiedDynamicDefaultArgumentArrayElement* values =
        new VerifiedDynamicDefaultArgumentArrayElement[count];
    delete[] values;
    return count;
}

extern "C" int verified_cxx_partial_array_default_argument_suffix() {
    VerifiedDynamicDefaultArgumentArrayElement* values =
        new VerifiedDynamicDefaultArgumentArrayElement[3]{9};
    delete[] values;
    return 0;
}

extern "C" int verified_cxx_partial_array_implicit_default(int count) {
    VerifiedImplicitDefaultArrayElement* values =
        new VerifiedImplicitDefaultArrayElement[count];
    delete[] values;
    return count;
}

extern "C" int verified_cxx_partial_array_implicit_default_fixed() {
    VerifiedImplicitDefaultArrayElement* values =
        new VerifiedImplicitDefaultArrayElement[3];
    delete[] values;
    return 0;
}

extern "C" int verified_cxx_partial_array_implicit_dmi(int count) {
    VerifiedImplicitDmiArrayElement* values =
        new VerifiedImplicitDmiArrayElement[count];
    delete[] values;
    return count;
}

extern "C" int verified_cxx_partial_array_implicit_dmi_fixed() {
    VerifiedImplicitDmiArrayElement* values =
        new VerifiedImplicitDmiArrayElement[2];
    delete[] values;
    return 0;
}

extern "C" int verified_cxx_partial_array_implicit_aggregate_dmi(int count) {
    VerifiedImplicitAggregateDmiArrayElement* values =
        new VerifiedImplicitAggregateDmiArrayElement[count];
    delete[] values;
    return count;
}

extern "C" int verified_cxx_partial_array_implicit_aggregate_dmi_fixed() {
    VerifiedImplicitAggregateDmiArrayElement* values =
        new VerifiedImplicitAggregateDmiArrayElement[1];
    delete[] values;
    return 0;
}

extern "C" int verified_cxx_partial_array_implicit_nested_dmi(int count) {
    VerifiedImplicitNestedDmiArrayElement* values =
        new VerifiedImplicitNestedDmiArrayElement[count];
    delete[] values;
    return count;
}

extern "C" int verified_cxx_partial_array_implicit_nested_dmi_fixed() {
    VerifiedImplicitNestedDmiArrayElement* values =
        new VerifiedImplicitNestedDmiArrayElement[1];
    delete[] values;
    return 0;
}

extern "C" int verified_cxx_partial_array_implicit_base_dmi(int count) {
    VerifiedImplicitBaseDmiArrayElement* values =
        new VerifiedImplicitBaseDmiArrayElement[count];
    delete[] values;
    return count;
}

extern "C" int verified_cxx_partial_array_implicit_base_dmi_fixed() {
    VerifiedImplicitBaseDmiArrayElement* values =
        new VerifiedImplicitBaseDmiArrayElement[1];
    delete[] values;
    return 0;
}

extern "C" int verified_cxx_partial_array_implicit_virtual_dmi(int count) {
    VerifiedImplicitVirtualDmiArrayElement* values =
        new VerifiedImplicitVirtualDmiArrayElement[count];
    delete[] values;
    return count;
}

extern "C" int
verified_cxx_partial_array_implicit_virtual_dmi_fixed() {
    VerifiedImplicitVirtualDmiArrayElement* values =
        new VerifiedImplicitVirtualDmiArrayElement[1];
    delete[] values;
    return 0;
}

extern "C" int verified_cxx_partial_array_nested_virtual_dmi(int count) {
    VerifiedNestedVirtualDmiArrayElement* values =
        new VerifiedNestedVirtualDmiArrayElement[count];
    int total = 0;
    for (int index = 0; index < count; ++index) {
        total += values[index].root_value_ + values[index].middle_value_ +
            values[index].leaf_value_;
    }
    delete[] values;
    return total;
}

extern "C" int
verified_cxx_partial_array_nested_virtual_dmi_fixed() {
    VerifiedNestedVirtualDmiArrayElement* values =
        new VerifiedNestedVirtualDmiArrayElement[1];
    int total = values[0].root_value_ + values[0].middle_value_ +
        values[0].leaf_value_;
    delete[] values;
    return total;
}

extern "C" int
verified_cxx_partial_array_shared_nested_virtual_dmi(int count) {
    VerifiedSharedNestedVirtualDmiArrayElement* values =
        new VerifiedSharedNestedVirtualDmiArrayElement[count];
    int total = 0;
    for (int index = 0; index < count; ++index) {
        total += values[index].root_value_ + values[index].left_value_ +
            values[index].right_value_ + values[index].leaf_value_;
    }
    delete[] values;
    return total;
}

extern "C" int
verified_cxx_partial_array_shared_nested_virtual_dmi_fixed() {
    VerifiedSharedNestedVirtualDmiArrayElement* values =
        new VerifiedSharedNestedVirtualDmiArrayElement[1];
    int total = values[0].root_value_ + values[0].left_value_ +
        values[0].right_value_ + values[0].leaf_value_;
    delete[] values;
    return total;
}

extern "C" int verified_cxx_partial_array_implicit_array_dmi(int count) {
    VerifiedImplicitArrayDmiArrayElement* values =
        new VerifiedImplicitArrayDmiArrayElement[count];
    delete[] values;
    return count;
}

extern "C" int verified_cxx_partial_array_implicit_array_dmi_fixed() {
    VerifiedImplicitArrayDmiArrayElement* values =
        new VerifiedImplicitArrayDmiArrayElement[1];
    delete[] values;
    return 0;
}

extern "C" int verified_cxx_partial_array_implicit_ctor_member_dmi(int count) {
    VerifiedImplicitArrayDmiConstructorElement* values =
        new VerifiedImplicitArrayDmiConstructorElement[count];
    delete[] values;
    return count;
}

extern "C" int verified_cxx_partial_array_implicit_ctor_member_dmi_fixed() {
    VerifiedImplicitArrayDmiConstructorElement* values =
        new VerifiedImplicitArrayDmiConstructorElement[1];
    delete[] values;
    return 0;
}

extern "C" int verified_cxx_partial_array_class_value_dmi(int count) {
    VerifiedImplicitClassValueDmiArrayElement* values =
        new VerifiedImplicitClassValueDmiArrayElement[count];
    delete[] values;
    return count;
}

extern "C" int verified_cxx_partial_array_class_value_dmi_fixed() {
    VerifiedImplicitClassValueDmiArrayElement* values =
        new VerifiedImplicitClassValueDmiArrayElement[1];
    delete[] values;
    return 0;
}

extern "C" int verified_cxx_partial_array_default_argument_throw(int count) {
    try {
        VerifiedThrowingDefaultArgumentArrayElement* values =
            new VerifiedThrowingDefaultArgumentArrayElement[count];
        delete[] values;
        return -1;
    } catch (int value) {
        return value;
    }
}

extern "C" int
verified_cxx_partial_array_default_argument_suffix_throw() {
    try {
        VerifiedThrowingDefaultArgumentArrayElement* values =
            new VerifiedThrowingDefaultArgumentArrayElement[3]{9};
        delete[] values;
        return -1;
    } catch (int value) {
        return value;
    }
}

int main() {
    return 0;
}
