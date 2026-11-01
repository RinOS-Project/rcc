struct ExplicitValue {
    int value;
    explicit ExplicitValue(int input) : value(input) {}
};

static void accept(ExplicitValue value) {
    (void)value;
}

template <typename T>
void accept_template(T value) {
    (void)value;
}

ExplicitValue make_returned_value();

ExplicitValue make_returned_value() {
    return {2};
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
    accept_template<ExplicitValue>({5});
    (void)make_returned_value();
    return 0;
}
