static int verified_partial_cleanup_count;

class VerifiedFirstGuard final {
public:
    explicit VerifiedFirstGuard(int* order) : order_(order) {}
    ~VerifiedFirstGuard() { *order_ = *order_ * 10 + 1; }

private:
    int* order_;
};

class VerifiedSecondGuard final {
public:
    explicit VerifiedSecondGuard(int* order) : order_(order) {}
    ~VerifiedSecondGuard() { *order_ = *order_ * 10 + 2; }

private:
    int* order_;
};

class VerifiedNestedGuards final {
public:
    VerifiedNestedGuards(int* first, int* second)
        : first_(first), second_(second) {}

private:
    VerifiedFirstGuard first_;
    VerifiedSecondGuard second_;
};

extern "C" int cxx_exception_cleanup_verified_throw(int value) {
    throw value;
}

extern "C" int cxx_exception_cleanup_verified_nested_members() {
    int order = 0;
    try {
        VerifiedNestedGuards guards(&order, &order);
        cxx_exception_cleanup_verified_throw(23);
    } catch (int caught) {
        return caught + order * 1000;
    }
    return -1;
}

class VerifiedCompletedMember final {
public:
    explicit VerifiedCompletedMember(int value) : value_(value) {}
    ~VerifiedCompletedMember() {
        verified_partial_cleanup_count += value_;
    }

private:
    int value_;
};

class VerifiedThrowingMember final {
public:
    VerifiedThrowingMember() { throw 19; }
    ~VerifiedThrowingMember() {
        verified_partial_cleanup_count += 1000;
    }
};

template <typename T>
struct VerifiedEmptyBase {};

template <typename T>
class VerifiedDependentDmi final : VerifiedEmptyBase<T> {
public:
    VerifiedCompletedMember completed{17};
    VerifiedThrowingMember later{};
};

extern "C" int cxx_exception_cleanup_verified_dependent_dmi() {
    int caught = 0;
    verified_partial_cleanup_count = 0;
    try {
        VerifiedDependentDmi<int> value;
    } catch (int exception_value) {
        caught = exception_value;
    }
    return caught + verified_partial_cleanup_count * 100;
}
