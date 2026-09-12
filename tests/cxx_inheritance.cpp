class Base {
public:
    int base_value;
};

class Derived : public Base {
public:
    int derived_value;
};

int read_base(Derived* value) {
    return value->base_value + value->derived_value;
}
