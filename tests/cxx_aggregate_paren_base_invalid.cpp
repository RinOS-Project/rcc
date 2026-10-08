struct Prefix {
    int prefix;
};

struct Base {
    int base;
};

struct Derived : Prefix, Base {
    int value;
};

Derived value(1, 2, 3);
