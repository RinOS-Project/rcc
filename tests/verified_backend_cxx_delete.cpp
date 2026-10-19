extern "C" void* rin_malloc(unsigned long size);
extern "C" void rin_free(void* pointer);

class VerifiedDeleteFirst final {
public:
    explicit VerifiedDeleteFirst(int* order) : order_(order) {}

    ~VerifiedDeleteFirst() {
        *order_ = *order_ * 10 + 1;
    }

private:
    int* order_;
};

class VerifiedDeleteSecond final {
public:
    explicit VerifiedDeleteSecond(int* order) : order_(order) {}

    ~VerifiedDeleteSecond() {
        *order_ = *order_ * 10 + 2;
    }

private:
    int* order_;
};

class VerifiedDeleteScope final {
public:
    VerifiedDeleteScope(int* order, bool throw_root)
        : first_(order), second_(order), throw_root_(throw_root) {}

    ~VerifiedDeleteScope() {
        if (throw_root_) throw 37;
    }

private:
    VerifiedDeleteFirst first_;
    VerifiedDeleteSecond second_;
    bool throw_root_;
};

struct VerifiedArrayDeleteInput {
    int* order;
    int* should_throw;
    int value;
};

class VerifiedArrayDelete final {
public:
    explicit VerifiedArrayDelete(const VerifiedArrayDeleteInput* input)
        : input_(input) {}

    ~VerifiedArrayDelete() {
        if (*input_->should_throw && input_->value == 3) throw 43;
        *input_->order = *input_->order * 10 + input_->value;
    }

private:
    const VerifiedArrayDeleteInput* input_;
};

extern "C" int verified_cxx_delete_throw() {
    int order = 0;
    try {
        VerifiedDeleteScope* object = new VerifiedDeleteScope(&order, true);
        delete object;
    } catch (int exception_value) {
        return exception_value + order * 1000;
    }
    return -1;
}

extern "C" int verified_cxx_delete_normal() {
    int order = 0;
    try {
        VerifiedDeleteScope* object = new VerifiedDeleteScope(&order, false);
        delete object;
    } catch (int exception_value) {
        return exception_value + order * 1000;
    }
    return order * 100;
}

extern "C" int verified_cxx_array_delete_throw() {
    int order = 0;
    int should_throw = 1;
    VerifiedArrayDeleteInput first{&order, &should_throw, 1};
    VerifiedArrayDeleteInput second{&order, &should_throw, 2};
    VerifiedArrayDeleteInput third{&order, &should_throw, 3};
    try {
        VerifiedArrayDelete* values =
            new VerifiedArrayDelete[3]{&first, &second, &third};
        delete[] values;
    } catch (int exception_value) {
        return exception_value + order * 1000;
    }
    return -1;
}

extern "C" int verified_cxx_array_delete_normal() {
    int order = 0;
    int should_throw = 0;
    VerifiedArrayDeleteInput first{&order, &should_throw, 1};
    VerifiedArrayDeleteInput second{&order, &should_throw, 2};
    VerifiedArrayDeleteInput third{&order, &should_throw, 3};
    VerifiedArrayDelete* values =
        new VerifiedArrayDelete[3]{&first, &second, &third};
    delete[] values;
    return order * 100;
}

int main() {
    return 0;
}
