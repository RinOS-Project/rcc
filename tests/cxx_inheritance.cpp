class Base {
public:
    int base_value;

    int base_sum(int extra) {
        return base_value + extra;
    }
};

class Derived : public Base {
public:
    int derived_value;
};

int read_base(Derived* value) {
    return value->base_value + value->derived_value;
}

int main() {
    Derived value;
    Derived* pointer = &value;
    value.base_value = 7;
    value.derived_value = 5;
    return read_base(&value) == 12 && value.base_sum(3) == 10 &&
                   pointer->base_sum(4) == 11 ? 0 : 1;
}
