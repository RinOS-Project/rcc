extern "C" int unsafe_move_close(int* value);
extern "C" void unsafe_move_observe(void);

class UnsafeMove final {
public:
    constexpr explicit UnsafeMove(int* value) noexcept : value_(value) {}

    UnsafeMove(UnsafeMove&& other) noexcept : value_(other.release()) {
        unsafe_move_observe();
    }

    ~UnsafeMove() {
        if (value_ != 0) {
            (void)unsafe_move_close(value_);
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

int unsafe_move_rejected(int* input) {
    auto source = UnsafeMove{input};
    auto target = UnsafeMove{static_cast<UnsafeMove&&>(source)};
    return target.release() == input;
}
