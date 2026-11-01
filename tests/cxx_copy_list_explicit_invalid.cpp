struct ExplicitValue {
    int value;
    explicit ExplicitValue(int input) : value(input) {}
};

static void accept(ExplicitValue value) {
    (void)value;
}

struct Aggregate {
    ExplicitValue value;
};

struct ExplicitPreferred {
    explicit ExplicitPreferred(short) {}
    ExplicitPreferred(int) {}
};

ExplicitValue copied_value = {1};
Aggregate nested_value{{3}};
ExplicitPreferred preferred_explicit = {static_cast<short>(6)};

int main() {
    accept({4});
    return 0;
}
