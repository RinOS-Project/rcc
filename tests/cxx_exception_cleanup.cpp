extern "C" int cxx_exception_cleanup_close(int* value);
extern "C" int cxx_exception_cleanup_callee(int value);
extern "C" int cxx_exception_cleanup_total = 0;
extern "C" int cxx_exception_destructor_total = 0;
extern "C" int cxx_exception_array_order = 0;
extern "C" void* rin_malloc(unsigned long size);
extern "C" void rin_free(void* pointer);
extern "C" void rin_cpp_exception_register_current_cleanup(
    void (*callback)(void*), void* object);
static int explicit_throwing_destructor_should_throw;
static int explicit_throwing_child_root_should_throw;

class ExplicitThrowingChild {
public:
    ~ExplicitThrowingChild() noexcept(false) {
        *order = *order * 10 + digit;
        if (digit == 2 && *throw_child) throw 53;
    }

    int* order;
    int* throw_child;
    int digit;
};

class ExplicitThrowingChildRoot {
public:
    ~ExplicitThrowingChildRoot() noexcept(false) {
        *order = *order * 10 + 9;
        if (explicit_throwing_child_root_should_throw) throw 59;
    }

    int* order;
    ExplicitThrowingChild first;
    ExplicitThrowingChild second;
};

class ExplicitThrowingMember {
public:
    explicit ExplicitThrowingMember(int* value) noexcept : order(value) {}

    ~ExplicitThrowingMember() noexcept {
        *order = *order * 10 + 1;
    }

    int* order;
};

class ExplicitThrowingLeft {
public:
    explicit ExplicitThrowingLeft(int* value) noexcept
        : order(value), member(value) {}

    ~ExplicitThrowingLeft() noexcept {
        *order = *order * 10 + 2;
    }

    int* order;
    ExplicitThrowingMember member;
};

class ExplicitThrowingRight {
public:
    explicit ExplicitThrowingRight(int* value) noexcept : order(value) {}

    ~ExplicitThrowingRight() noexcept {
        *order = *order * 10 + 3;
    }

    int* order;
};

class ExplicitThrowingDerived : public ExplicitThrowingLeft,
                                public ExplicitThrowingRight {
public:
    ExplicitThrowingDerived(
        int* value, ExplicitThrowingDerived** receiver_slot) noexcept
        : ExplicitThrowingLeft(value), ExplicitThrowingRight(value), order(value),
          receiver(receiver_slot) {}

    ~ExplicitThrowingDerived() noexcept(false) {
        *receiver = 0;
        *order = *order * 10 + 9;
        if (explicit_throwing_destructor_should_throw) throw 47;
    }

    int* order;
    ExplicitThrowingDerived** receiver;
};

class ExceptionGuard final {
public:
    constexpr explicit ExceptionGuard(int* value) noexcept : value_(value) {}

    ~ExceptionGuard() {
        if (value_ != 0) {
            (void)cxx_exception_cleanup_close(value_);
        }
    }

private:
    int* value_;
};

extern "C" int cxx_exception_cleanup_close(int* value) {
    ++*value;
    ++cxx_exception_cleanup_total;
    return 0;
}

extern "C" int cxx_exception_cleanup_direct() {
    int count = 0;
    try {
        auto guard = ExceptionGuard{&count};
        throw 41;
    } catch (int caught) {
        return caught + count * 100;
    }
}

static void cxx_exception_cleanup_throw_on_leave(void*) {
    throw 61;
}

extern "C" int cxx_exception_cleanup_throwing_on_leave() {
    int caught = 0;
    try {
        int token = 0;
        rin_cpp_exception_register_current_cleanup(
            cxx_exception_cleanup_throw_on_leave, &token);
    } catch (int exception_value) {
        caught = exception_value;
    }
    return caught;
}

extern "C" int cxx_exception_cleanup_handler() {
    int count = 0;
    try {
        throw 7;
    } catch (int caught) {
        auto guard = ExceptionGuard{&count};
        return caught + count * 100;
    }
}

extern "C" int cxx_exception_cleanup_rethrow() {
    int count = 0;
    try {
        try {
            auto guard = ExceptionGuard{&count};
            throw 5;
        } catch (int) {
            throw;
        }
    } catch (int caught) {
        return caught + count * 100;
    }
}

class NativeExceptionGuard final {
public:
    NativeExceptionGuard(int* value, int* order, int id)
        : value_(value), order_(order), id_(id) {}

    ~NativeExceptionGuard() {
        ++*value_;
        ++cxx_exception_destructor_total;
        *order_ = *order_ * 10 + id_;
    }

private:
    int* value_;
    int* order_;
    int id_;
};

class NestedFirstExceptionGuard final {
public:
    explicit NestedFirstExceptionGuard(int* order) : order_(order) {}

    ~NestedFirstExceptionGuard() {
        *order_ = *order_ * 10 + 1;
    }

private:
    int* order_;
};

class NestedSecondExceptionGuard final {
public:
    explicit NestedSecondExceptionGuard(int* order) : order_(order) {}

    ~NestedSecondExceptionGuard() {
        *order_ = *order_ * 10 + 2;
    }

private:
    int* order_;
};

class NestedExceptionScope final {
public:
    NestedExceptionScope(int* first, int* second)
        : first_(first), second_(second) {}

private:
    NestedFirstExceptionGuard first_;
    NestedSecondExceptionGuard second_;
};

class NestedDeleteThrowingScope final {
public:
    NestedDeleteThrowingScope(int* first, int* second)
        : first_(first), second_(second) {}

    ~NestedDeleteThrowingScope() {
        throw 37;
    }

private:
    NestedFirstExceptionGuard first_;
    NestedSecondExceptionGuard second_;
};

static int dependent_dmi_partial_cleanup_count;

template <typename T>
struct DependentDmiExceptionBase {};

class DependentDmiCompletedMember final {
public:
    explicit DependentDmiCompletedMember(int value) : value_(value) {}

    ~DependentDmiCompletedMember() {
        dependent_dmi_partial_cleanup_count += value_;
    }

private:
    int value_;
};

class DependentDmiThrowingMember final {
public:
    DependentDmiThrowingMember() {
        throw 19;
    }

    ~DependentDmiThrowingMember() {
        dependent_dmi_partial_cleanup_count += 1000;
    }
};

template <typename T>
class DependentDmiThrowAfterCompleted final : DependentDmiExceptionBase<T> {
public:
    DependentDmiCompletedMember completed{17};
    DependentDmiThrowingMember later{};
};

class CompletedDmiThenThrow final {
public:
    DependentDmiCompletedMember completed{17};
};

extern "C" int cxx_exception_cleanup_dependent_dmi() {
    int caught = 0;
    dependent_dmi_partial_cleanup_count = 0;
    try {
        DependentDmiThrowAfterCompleted<int> failed;
    } catch (int value) {
        caught = value;
    }
    return caught + dependent_dmi_partial_cleanup_count * 100;
}

extern "C" int cxx_exception_cleanup_completed_dmi_then_throw() {
    int caught = 0;
    dependent_dmi_partial_cleanup_count = 0;
    try {
        CompletedDmiThenThrow completed;
        throw 23;
    } catch (int value) {
        caught = value;
    }
    return caught + dependent_dmi_partial_cleanup_count * 100;
}

class ArrayDeleteThrowing final {
public:
    explicit ArrayDeleteThrowing(int value) : value_(value) {}

    ~ArrayDeleteThrowing() {
        if (value_ == 3) throw 43;
        cxx_exception_array_order = cxx_exception_array_order * 10 + value_;
    }

private:
    int value_;
};

extern "C" int cxx_exception_cleanup_callee(int value) {
    throw value;
}

extern "C" int cxx_exception_cleanup_across_call() {
    int count = 0;
    int order = 0;
    try {
        NativeExceptionGuard first{&count, &order, 1};
        NativeExceptionGuard second{&count, &order, 2};
        cxx_exception_cleanup_callee(23);
    } catch (int caught) {
        return caught + count * 100 + order * 1000;
    }
}

extern "C" int cxx_exception_cleanup_nested_members() {
    int order = 0;
    try {
        NestedExceptionScope scope{&order, &order};
        cxx_exception_cleanup_callee(29);
    } catch (int caught) {
        return caught + order * 1000;
    }
}

extern "C" int cxx_exception_cleanup_delete_throw() {
    int order = 0;
    try {
        NestedDeleteThrowingScope* scope =
            new NestedDeleteThrowingScope(&order, &order);
        delete scope;
    } catch (int caught) {
        return caught + order * 1000;
    }
}

extern "C" int cxx_exception_cleanup_array_delete_throw() {
    cxx_exception_array_order = 0;
    try {
        ArrayDeleteThrowing* values =
            new ArrayDeleteThrowing[3]{1, 2, 3};
        delete[] values;
    } catch (int caught) {
        return caught + cxx_exception_array_order * 1000;
    }
    return -1;
}

extern "C" int cxx_exception_cleanup_explicit_throwing_destructor_callee(
    int* order, ExplicitThrowingDerived** receiver,
    ExplicitThrowingDerived** storage) {
    ExplicitThrowingDerived* value =
        new ExplicitThrowingDerived(order, receiver);
    *storage = value;
    value->~ExplicitThrowingDerived();
    return -1;
}

extern "C" int cxx_exception_cleanup_explicit_throwing_child_callee(
    int* order, int* throw_child, ExplicitThrowingChildRoot** storage) {
    ExplicitThrowingChildRoot* value =
        static_cast<ExplicitThrowingChildRoot*>(
            rin_malloc(sizeof(ExplicitThrowingChildRoot)));
    if (!value) return -1;
    value->order = order;
    value->first.order = order;
    value->first.throw_child = throw_child;
    value->first.digit = 1;
    value->second.order = order;
    value->second.throw_child = throw_child;
    value->second.digit = 2;
    *storage = value;
    value->~ExplicitThrowingChildRoot();
    return -1;
}

static int run_explicit_throwing_destructor_case(int should_throw) {
    int order = 0;
    int caught = 0;
    ExplicitThrowingDerived* value = 0;
    ExplicitThrowingDerived* storage = 0;
    explicit_throwing_destructor_should_throw = should_throw;
    try {
        cxx_exception_cleanup_explicit_throwing_destructor_callee(
            &order, &value, &storage);
    } catch (int exception_value) {
        caught = exception_value;
    }
    rin_free(static_cast<void*>(storage));
    return caught == (should_throw ? 47 : 0) &&
            order == 9321 && value == 0
        ? 0 : 1;
}

extern "C" int cxx_exception_cleanup_explicit_throwing_destructor() {
    return run_explicit_throwing_destructor_case(1) +
        run_explicit_throwing_destructor_case(0);
}

static int run_explicit_throwing_child_case(int should_throw) {
    int order = 0;
    int caught = 0;
    ExplicitThrowingChildRoot* storage = 0;
    explicit_throwing_child_root_should_throw = 0;
    try {
        cxx_exception_cleanup_explicit_throwing_child_callee(
            &order, &should_throw, &storage);
    } catch (int exception_value) {
        caught = exception_value;
    }
    rin_free(static_cast<void*>(storage));
    return caught == (should_throw ? 53 : 0) && order == 921 ? 0 : 1;
}

extern "C" int cxx_exception_cleanup_explicit_throwing_child() {
    return run_explicit_throwing_child_case(1) +
        run_explicit_throwing_child_case(0);
}

extern "C" int cxx_exception_cleanup_double_throw() {
    int order = 0;
    int should_throw = 1;
    ExplicitThrowingChildRoot* storage = 0;
    explicit_throwing_child_root_should_throw = 1;
    try {
        cxx_exception_cleanup_explicit_throwing_child_callee(
            &order, &should_throw, &storage);
    } catch (int) {
        return 0;
    }
    return 0;
}

extern "C" int main() {
    return cxx_exception_cleanup_direct() == 141 &&
                   cxx_exception_cleanup_throwing_on_leave() == 61 &&
                   cxx_exception_cleanup_handler() == 7 &&
                   cxx_exception_cleanup_total == 2 &&
                   cxx_exception_cleanup_rethrow() == 105 &&
                   cxx_exception_cleanup_total == 3 &&
                   cxx_exception_cleanup_across_call() == 21223 &&
                   cxx_exception_destructor_total == 2 &&
                   cxx_exception_cleanup_nested_members() == 21029 &&
                   cxx_exception_cleanup_delete_throw() == 21037 &&
                   cxx_exception_cleanup_dependent_dmi() == 1719 &&
                   cxx_exception_cleanup_completed_dmi_then_throw() == 1723 &&
                   cxx_exception_cleanup_array_delete_throw() == 21043 &&
                   cxx_exception_cleanup_explicit_throwing_destructor() == 0 &&
                   cxx_exception_cleanup_explicit_throwing_child() == 0
               ? 0
               : 1;
}
