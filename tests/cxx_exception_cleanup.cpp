extern "C" int cxx_exception_cleanup_close(int* value);
extern "C" int cxx_exception_cleanup_total = 0;

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

extern "C" int main() {
    return cxx_exception_cleanup_direct() == 141 &&
                   cxx_exception_cleanup_handler() == 7 &&
                   cxx_exception_cleanup_total == 2 &&
                   cxx_exception_cleanup_rethrow() == 105 &&
                   cxx_exception_cleanup_total == 3
               ? 0
               : 1;
}
