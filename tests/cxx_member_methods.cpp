struct Point {
    int x;
    int y;

    int scaled_sum(int scale) {
        return x * scale + y;
    }

    long scaled_sum(long scale) {
        return x * scale + y + 100;
    }

    int x_value() const {
        return x;
    }
};

int cxx_member_method_probe() {
    Point point;
    point.x = 6;
    point.y = 5;
    return point.scaled_sum(7) + point.x_value();
}

int cxx_pointer_method_probe() {
    Point point;
    point.x = 8;
    point.y = 3;
    Point* pointer = &point;
    return pointer->scaled_sum(2) + pointer->x_value();
}

int cxx_member_overload_probe() {
    Point point;
    point.x = 6;
    point.y = 5;
    return point.scaled_sum((long)7) == 147 ? 0 : 1;
}

namespace out_of_class_methods {
struct Point {
    int x;
    int add(int amount);
    long add(long amount);
    int x_value() const;
    static int twice(int value);
    int add_default(int amount = 4);
    int* address();
    int& reference();
    int ref_qualified() &;
    int ref_qualified() &&;
    int safe_value() const noexcept;
};

class PrivateValue {
    int value;

public:
    int get() const;
    void set(int value);
};

union Word {
    int integer;
    unsigned char bytes[4];
    int value() const;
};
}

namespace out_of_class_constructors {
struct Values {
    int first;
    int second;
    Values();
    explicit Values(int);
    Values(int, int);
};

struct DefaultedArgument {
    int value;
    explicit DefaultedArgument(int seed = 8);
};
}

out_of_class_constructors::Values::Values()
    : first(3), second(4) {}

out_of_class_constructors::Values::Values(int seed)
    : first(seed), second(seed + 1) {}

out_of_class_constructors::Values::Values(int left, int right)
    : first(left), second(right) {
    first += second;
}

namespace out_of_class_constructors {
DefaultedArgument::DefaultedArgument(int initial_value)
    : value(initial_value) {}
}

int out_of_class_methods::Point::add(int amount) {
    return x + amount;
}

long out_of_class_methods::Point::add(long amount) {
    return x + amount + 100;
}

int out_of_class_methods::Point::x_value() const {
    return this->x;
}

int out_of_class_methods::Point::twice(int value) {
    return value * 2;
}

int out_of_class_methods::Point::add_default(int amount) {
    return x + amount;
}

int* out_of_class_methods::Point::address() {
    return &x;
}

int& out_of_class_methods::Point::reference() {
    return x;
}

int out_of_class_methods::Point::ref_qualified() & {
    return x;
}

int out_of_class_methods::Point::ref_qualified() && {
    return x + 10;
}

int out_of_class_methods::Point::safe_value() const noexcept {
    return x;
}

int out_of_class_methods::PrivateValue::get() const {
    return value;
}

void out_of_class_methods::PrivateValue::set(int new_value) {
    value = new_value;
}

int out_of_class_methods::Word::value() const {
    return integer;
}

int cxx_out_of_class_member_method_probe() {
    out_of_class_methods::Point point;
    point.x = 6;
    *point.address() = 5;
    point.reference() = 6;
    double precise = 2.5;
    double& precise_reference = precise;
    precise_reference = 7.25;
    point.x = 6;
    out_of_class_methods::Point temporary;
    temporary.x = 8;
    out_of_class_methods::Word word;
    word.integer = 9;
    out_of_class_methods::PrivateValue private_value;
    private_value.set(2);
    int default_value = point.add_default();
    int lvalue_ref_qualified = point.ref_qualified();
    int rvalue_ref_qualified =
        static_cast<out_of_class_methods::Point&&>(temporary).ref_qualified();
    return point.add(7) + (int)point.add((long)7) + point.x_value() +
           out_of_class_methods::Point::twice(3) + word.value() +
           default_value + lvalue_ref_qualified + rvalue_ref_qualified +
           point.safe_value() + private_value.get() + point.x +
           (precise == 7.25 ? 0 : 1000);
}

int cxx_out_of_class_constructor_probe() {
    out_of_class_constructors::Values defaults;
    out_of_class_constructors::Values one(5);
    out_of_class_constructors::Values pair(3, 4);
    out_of_class_constructors::DefaultedArgument defaulted;
    return defaults.first + defaults.second + one.first + one.second +
           pair.first + pair.second + defaulted.value;
}

int main() {
    return cxx_member_method_probe() == 53 &&
           cxx_pointer_method_probe() == 27 &&
           cxx_member_overload_probe() == 0 &&
           cxx_out_of_class_member_method_probe() ==
               13 + 113 + 6 + 6 + 9 + 10 + 6 + 18 + 6 + 2 + 6 &&
           cxx_out_of_class_constructor_probe() == 37
               ? 0 : 1;
}
