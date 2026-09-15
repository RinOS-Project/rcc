extern "C" int cxx_exception_cleanup_call_close(int* value);

class ExceptionCallGuard final {
public:
    constexpr explicit ExceptionCallGuard(int* value) noexcept : value_(value) {}
    ~ExceptionCallGuard() {
        if (value_ != 0) {
            (void)cxx_exception_cleanup_call_close(value_);
        }
    }

private:
    int* value_;
};

extern "C" int cxx_exception_cleanup_call_rejected(int* value) {
    try {
        auto guard = ExceptionCallGuard{value};
        (void)cxx_exception_cleanup_call_close(value);
        throw 1;
    } catch (int) {
        return 0;
    }
}
