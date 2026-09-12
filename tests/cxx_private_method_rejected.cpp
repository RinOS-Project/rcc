class PrivateMethod final {
public:
    constexpr explicit PrivateMethod(int input) noexcept : value_(input) {}

private:
    constexpr int secret() const noexcept { return value_; }
    int value_;
};

extern "C" int expose_private_method(int input) {
    PrivateMethod value{input};
    return value.secret();
}
