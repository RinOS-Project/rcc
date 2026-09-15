class Delegating {
public:
    explicit Delegating(int value, int delta = 1) : value_(value + delta) {}
    Delegating() : Delegating(6) {}

    int value() const {
        return value_;
    }

private:
    int value_;
};

extern "C" int cxx_delegating_constructor(void) {
    Delegating direct{};
    Delegating explicit_value(3);
    Delegating* heap = new Delegating();
    int result = direct.value() + explicit_value.value() + heap->value();
    delete heap;
    return result == 18 ? 0 : 1;
}
