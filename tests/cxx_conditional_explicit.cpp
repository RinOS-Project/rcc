struct Implicit {
    explicit(false) Implicit(int value) : value(value) {}

    operator int() const { return value; }

    int value;
};

struct Explicit {
    explicit(true) Explicit(int value) : value(value) {}

    operator int() const { return value; }

    int value;
};

int main() {
    Implicit implicit_value = 7;
    Explicit explicit_value(9);
    int first = implicit_value;
    int second = explicit_value;
    if (first != 7) return 2;
    if (second != 9) return 3;
    return 0;
}
