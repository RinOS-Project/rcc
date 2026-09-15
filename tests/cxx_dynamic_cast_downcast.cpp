class Base {
public:
    virtual int value() { return 11; }
};

class Derived : public Base {
public:
    int value() override { return 42; }
};

class Unrelated {
public:
    virtual int value() { return 7; }
};

extern "C" int main() {
    Derived derived{};
    Base base{};
    Base* derived_base = &derived;
    Base* plain_base = &base;
    Derived* exact = dynamic_cast<Derived*>(derived_base);
    Derived* mismatch = dynamic_cast<Derived*>(plain_base);
    return exact && exact->value() == 42 && mismatch == 0 ? 0 : 1;
}
