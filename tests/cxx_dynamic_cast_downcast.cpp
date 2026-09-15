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

class Further : public Derived {};

class Left {
public:
    virtual int left() { return 13; }
};

class Right {
public:
    virtual int right() { return 17; }
};

class Both : public Left, public Right {
public:
    int left() override { return 23; }
    int right() override { return 29; }
};

extern "C" int main() {
    Derived derived{};
    Base base{};
    Base* derived_base = &derived;
    Base* plain_base = &base;
    Derived* exact = dynamic_cast<Derived*>(derived_base);
    Derived* mismatch = dynamic_cast<Derived*>(plain_base);
    Further further{};
    Base* further_base = &further;
    Derived* further_derived = dynamic_cast<Derived*>(further_base);
    Both both{};
    Left* left = &both;
    Right* cross = dynamic_cast<Right*>(left);
    return exact && exact->value() == 42 && mismatch == 0 &&
           further_derived && further_derived->value() == 42 &&
           cross && cross->right() == 29 ? 0 : 1;
}
