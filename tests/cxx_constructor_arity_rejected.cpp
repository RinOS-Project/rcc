class OneArgument final {
public:
    constexpr explicit OneArgument(int input) noexcept : value(input) {}

    int value;
};

extern "C" int construct_without_argument(void) {
    OneArgument value{};
    return value.value;
}
