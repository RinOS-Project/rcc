class Secret final {
public:
    constexpr explicit Secret(int input) noexcept : value(input) {}

private:
    int value;
};

extern "C" int expose_secret(int input) {
    Secret secret{input};
    return secret.value;
}
