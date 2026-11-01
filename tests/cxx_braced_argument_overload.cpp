struct One {
    int first;
};

struct Two {
    int first;
    int second;
};

struct Nested {
    Two value;
};

struct AggregateChoice {
    int value;
};

struct Constructed {
    int value;
    Constructed(int input) : value(input) {}
};

int choose_arity(One) { return 1; }
int choose_arity(Two) { return 2; }
int choose_nested(Nested value) { return value.value.second; }
int choose_scalar(int) { return 3; }
int choose_scalar(double) { return 4; }
int choose_integral(int) { return 9; }
int choose_integral(unsigned char) { return 10; }
int choose_scalar_or_aggregate(AggregateChoice) { return 5; }
int choose_scalar_or_aggregate(int) { return 6; }
int choose_reference(const Two&) { return 7; }
int choose_reference(Two&&) { return 8; }
int choose_scalar_reference(int& value) {
    ++value;
    return value;
}
int choose_scalar_reference(const int& value) { return -value; }
int choose_scalar_reference(int&& value) { return value + 100; }
int take_constructed(Constructed value) { return value.value; }
int zero_scalar(int value) { return value; }

int main() {
    if (choose_arity({11, 22}) != 2) return 1;
    if (choose_nested({{31, 32}}) != 32) return 2;
    if (choose_scalar({1}) != 3) return 3;
    if (choose_scalar_or_aggregate({1}) != 6) return 4;
    if (choose_integral({256}) != 9) return 5;
    if (choose_reference({41, 42}) != 8) return 6;
    int source = 61;
    if (choose_scalar_reference({source}) != 62 || source != 62) return 7;
    if (choose_scalar_reference({71}) != 171) return 8;
    if (take_constructed({51}) != 51) return 9;
    if (zero_scalar({}) != 0) return 10;
    return 0;
}
