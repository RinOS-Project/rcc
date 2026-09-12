extern "C" int unsafe_delegate_close(int* value);
extern "C" void unsafe_delegate_observe(void);

class UnsafeDelegateStatus final {
public:
    constexpr UnsafeDelegateStatus() noexcept : code_(0) {}
    constexpr explicit UnsafeDelegateStatus(int code) noexcept : code_(code) {}

    constexpr int code() const noexcept { return code_; }

private:
    int code_;
};

class UnsafeCloseDelegate final {
public:
    constexpr explicit UnsafeCloseDelegate(int* value) noexcept
        : value_(value) {}

    ~UnsafeCloseDelegate() {
        if (value_ != 0) {
            (void)unsafe_delegate_close(value_);
        }
    }

    UnsafeDelegateStatus close() noexcept {
        if (value_ == 0) return UnsafeDelegateStatus{};
        int result = unsafe_delegate_close(value_);
        if (result == 0) value_ = 0;
        return UnsafeDelegateStatus{result};
    }

    UnsafeDelegateStatus reset() noexcept {
        auto result = close();
        unsafe_delegate_observe();
        return result;
    }

private:
    int* value_;
};

int unsafe_close_delegate_rejected(int* value) {
    auto handle = UnsafeCloseDelegate{value};
    return handle.reset().code();
}
