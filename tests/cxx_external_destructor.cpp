extern "C" int external_destructor_close(int* value);

class ExternalDestructor final {
public:
    constexpr explicit ExternalDestructor(int* value) noexcept : value_(value) {}

    ~ExternalDestructor() {
        (void)external_destructor_close(value_);
    }

private:
    int* value_;
};

int external_destructor_lowered(int* value) {
    auto handle = ExternalDestructor{value};
    return 0;
}
