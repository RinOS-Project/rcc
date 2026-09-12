extern "C" int unsafe_bool_check(int value);

class UnsafeBool final {
public:
    constexpr explicit UnsafeBool(int value) noexcept : value_(value) {}

    constexpr explicit operator bool() const noexcept {
        return unsafe_bool_check(value_) != 0;
    }

private:
    int value_;
};

int unsafe_bool_rejected(int input) {
    auto value = UnsafeBool{input};
    return value ? 1 : 0;
}
