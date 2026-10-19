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

extern "C" int verified_partial_array_next_ctor();
extern "C" void verified_partial_array_record_destructor(int value);

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

int main() {
    return 0;
}
