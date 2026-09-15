extern "C" int cxx_global_constructor_calls = 0;
extern "C" int cxx_global_destructor_calls = 0;

extern "C" int cxx_global_constructor_seed() {
    ++cxx_global_constructor_calls;
    return 40;
}

class GlobalConstructed final {
public:
    explicit GlobalConstructed(int input) {
        value_ = input + 2;
    }

    ~GlobalConstructed() {
        ++cxx_global_destructor_calls;
    }

    int value() const {
        return value_;
    }

private:
    int value_;
};

GlobalConstructed global_constructed{cxx_global_constructor_seed()};

extern "C" int cxx_global_constructor(void) {
    return cxx_global_constructor_calls == 1 &&
                   cxx_global_destructor_calls == 0 &&
                   global_constructed.value() == 42
               ? 0
               : 1;
}

int main(void) {
    return cxx_global_constructor();
}
