struct Base {
    int value;
};

struct Derived : Base {
    int member;
};

Derived value{1, 2};
