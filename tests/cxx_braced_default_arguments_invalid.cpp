#include <initializer_list>

void consume_defaulted(std::initializer_list<int> values = {1.5}) {
    (void)values;
}

void consume_scalar_default(int value = {1.5}) {
    (void)value;
}

struct ExplicitDefault {
    explicit ExplicitDefault(int value = 1) { (void)value; }
};

void consume_explicit_default(ExplicitDefault value = {}) {
    (void)value;
}

int main() {
    return 0;
}
