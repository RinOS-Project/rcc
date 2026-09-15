class ExplicitOnly {
public:
    explicit ExplicitOnly(int value) : value_(value) {}

private:
    int value_;
};

extern "C" int cxx_explicit_copy_initialization_invalid(void) {
    ExplicitOnly value = 3;
    return (int)sizeof(value);
}
