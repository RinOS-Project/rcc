extern "C" int cleanup_copy_close(int* value);

class CleanupCopy final {
public:
    constexpr explicit CleanupCopy(int* value) noexcept : value_(value) {}
    CleanupCopy(const CleanupCopy&) = delete;
    CleanupCopy& operator=(const CleanupCopy&) = delete;

    ~CleanupCopy() {
        if (value_ != 0) {
            (void)cleanup_copy_close(value_);
        }
    }

private:
    int* value_;
};

int cleanup_copy_rejected(int* value) {
    auto first = CleanupCopy{value};
    auto second = first;
    return 0;
}
