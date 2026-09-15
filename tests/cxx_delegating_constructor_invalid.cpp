class Cyclic {
public:
    Cyclic() : Cyclic(1) {}
    explicit Cyclic(int value) : Cyclic() { (void)value; }
};

extern "C" int cxx_invalid_delegating_constructor(void) {
    Cyclic value{};
    return (int)sizeof(value);
}
