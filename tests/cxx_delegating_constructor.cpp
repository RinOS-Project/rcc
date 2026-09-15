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

class DelegatingOverload {
public:
    explicit DelegatingOverload(long value) : value_(value) {}
    explicit DelegatingOverload(int value) : value_(value + 100) {}
    DelegatingOverload() : DelegatingOverload(5) {}

    int value() const {
        return value_;
    }

private:
    int value_;
};

extern "C" int cxx_delegating_constructor(void) {
    Delegating direct;
    Delegating explicit_value(3);
    Delegating* heap = new Delegating();
    DelegatingOverload overload;
    int result = direct.value() + explicit_value.value() + heap->value() +
                 overload.value();
    delete heap;
    return result == 123 ? 0 : 1;
}
