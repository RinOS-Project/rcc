extern "C" int cleanup_switch_scope_close(int* value);

class CleanupSwitchScope final {
public:
    constexpr explicit CleanupSwitchScope(int* value) noexcept
        : value_(value) {}

    ~CleanupSwitchScope() {
        if (value_ != 0) {
            (void)cleanup_switch_scope_close(value_);
        }
    }

private:
    int* value_;
};

int cleanup_switch_scope_rejected(int* value, int selector) {
    switch (selector) {
        auto handle = CleanupSwitchScope{value};
        case 0:
            return 0;
        default:
            return 1;
    }
}
