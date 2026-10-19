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

int main() {
    return 0;
}
