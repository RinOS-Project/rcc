class Base {
public:
    int base_value;

    int base_sum(int extra) {
        return base_value + extra;
    }
};

class Other {
public:
    int other_value;

    int other_sum(int extra) {
        return other_value + extra;
    }
};

class Derived : public Base, public Other {
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
    value.other_value = 9;
    value.derived_value = 5;
    return read_base(&value) == 12 && value.base_sum(3) == 10 &&
                   pointer->base_sum(4) == 11 &&
                   value.other_sum(2) == 11 &&
                   pointer->other_sum(3) == 12 ? 0 : 1;
}
