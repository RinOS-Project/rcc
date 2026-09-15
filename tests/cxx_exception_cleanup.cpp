extern "C" int cxx_exception_cleanup_close(int* value);
extern "C" int cxx_exception_cleanup_callee(int value);
extern "C" int cxx_exception_cleanup_total = 0;
extern "C" int cxx_exception_destructor_total = 0;

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

extern "C" int main() {
    return cxx_exception_cleanup_direct() == 141 &&
                   cxx_exception_cleanup_handler() == 7 &&
                   cxx_exception_cleanup_total == 2 &&
                   cxx_exception_cleanup_rethrow() == 105 &&
                   cxx_exception_cleanup_total == 3 &&
                   cxx_exception_cleanup_across_call() == 21223 &&
                   cxx_exception_destructor_total == 2 &&
                   cxx_exception_cleanup_nested_members() == 21029 &&
                   cxx_exception_cleanup_delete_throw() == 21037
               ? 0
               : 1;
}
