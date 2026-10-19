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

int main() {
    return 0;
}
