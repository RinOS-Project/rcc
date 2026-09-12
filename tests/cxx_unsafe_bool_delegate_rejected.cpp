extern "C" int unsafe_bool_delegate_check(int value);

class UnsafeBoolDelegate final {
public:
    constexpr explicit UnsafeBoolDelegate(int value) noexcept
        : value_(value) {}

    constexpr bool ok() const noexcept {
        return unsafe_bool_delegate_check(value_) != 0;
    }

    constexpr explicit operator bool() const noexcept { return ok(); }

private:
    int value_;
};

int unsafe_bool_delegate_rejected(int input) {
    auto value = UnsafeBoolDelegate{input};
    return value ? 1 : 0;
}
