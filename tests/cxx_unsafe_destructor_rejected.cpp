extern "C" int unsafe_destructor_close(int* value);

class UnsafeDestructor final {
public:
    constexpr explicit UnsafeDestructor(int* value) noexcept : value_(value) {}

    ~UnsafeDestructor() {
        (void)unsafe_destructor_close(value_);
    }

private:
    int* value_;
};

int unsafe_destructor_rejected(int* value) {
    auto handle = UnsafeDestructor{value};
    return 0;
}
