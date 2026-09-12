extern "C" int unsafe_release_close(int* value);

class UnsafeRelease final {
public:
    constexpr explicit UnsafeRelease(int* value) noexcept : value_(value) {}

    ~UnsafeRelease() {
        if (value_ != 0) {
            (void)unsafe_release_close(value_);
        }
    }

    int* release() noexcept {
        int* value = value_;
        value_ = 1;
        return value;
    }

private:
    int* value_;
};

int unsafe_release_rejected(int* value) {
    auto handle = UnsafeRelease{value};
    return handle.release() != 0;
}
