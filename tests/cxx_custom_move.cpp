extern "C" int custom_move_close(int* value);
extern "C" void custom_move_observe(void);

class CustomMove final {
public:
    constexpr explicit CustomMove(int* value) noexcept : value_(value) {}

    CustomMove(CustomMove&& other) noexcept : value_(other.release()) {
        custom_move_observe();
    }

    ~CustomMove() {
        if (value_ != 0) {
            (void)custom_move_close(value_);
        }
    }

    int* release() noexcept {
        int* value = value_;
        value_ = 0;
        return value;
    }

private:
    int* value_;
};

int custom_move_supported(int* input) {
    auto source = CustomMove{input};
    auto target = CustomMove{static_cast<CustomMove&&>(source)};
    return target.release() == input;
}
