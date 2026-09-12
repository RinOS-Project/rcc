extern "C" int cleanup_flow_close(int* value);

class CleanupFlow final {
public:
    constexpr explicit CleanupFlow(int* value) noexcept : value_(value) {}

    ~CleanupFlow() {
        if (value_ != 0) {
            (void)cleanup_flow_close(value_);
        }
    }

private:
    int* value_;
};

int cleanup_control_flow_rejected(int* value) {
    goto done;
    auto handle = CleanupFlow{value};
done:
    return 0;
}
