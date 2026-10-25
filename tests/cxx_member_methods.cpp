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

namespace out_of_class_operators {
struct Scalar {
    int value;
    Scalar operator+(int amount) const;
};

struct Assigned {
    int value;
    Assigned& operator=(int new_value);
    Assigned& operator+=(int amount);
};

struct Logical {
    int value;
    bool operator!() const;
    bool operator==(int other) const;
};

struct Indexed {
    int elements[2];
    int& operator[](int index);
};

struct Callable {
    int offset;
    int operator()(int value) const;
};

struct Negatable {
    int value;
    int operator-() const;
};

struct Dereference {
    int value;
    int& operator*() &;
    int&& operator*() &&;
};

struct IncrementCounter {
    int value;
    int operator++();
    int operator++(int);
};

struct ArrowTarget {
    int value;
};

struct ArrowHandle {
    ArrowTarget* target;
    ArrowTarget* operator->();
};
}

namespace out_of_class_conversions {
using FunctionPointer = int (*)(int);

struct Truthy {
    int value;
    operator bool() const noexcept;
};

struct Numeric {
    int value;
    operator int() const;
};

struct Pointer {
    int* value;
    operator int*() const;
};

struct Reference {
    int value;
    operator int&();
};

struct Function {
    operator FunctionPointer() const;
};
}

int out_of_class_conversion_function(int value) {
    return value + 37;
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

struct Tracked {
    int* count;
    ~Tracked() noexcept;
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

out_of_class_constructors::Tracked::~Tracked() noexcept {
    *count += 3;
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

out_of_class_operators::Scalar
out_of_class_operators::Scalar::operator+(int amount) const {
    Scalar result;
    result.value = value + amount;
    return result;
}

out_of_class_operators::Assigned&
out_of_class_operators::Assigned::operator=(int new_value) {
    value = new_value;
    return *this;
}

out_of_class_operators::Assigned&
out_of_class_operators::Assigned::operator+=(int amount) {
    value += amount;
    return *this;
}

bool out_of_class_operators::Logical::operator!() const {
    return value == 0;
}

bool out_of_class_operators::Logical::operator==(int other) const {
    return value == other;
}

int& out_of_class_operators::Indexed::operator[](int index) {
    return elements[index];
}

int out_of_class_operators::Callable::operator()(int value) const {
    return offset + value;
}

int out_of_class_operators::Negatable::operator-() const {
    return -value;
}

int& out_of_class_operators::Dereference::operator*() & {
    return value;
}

int&& out_of_class_operators::Dereference::operator*() && {
    return static_cast<int&&>(value);
}

int out_of_class_operators::IncrementCounter::operator++() {
    return ++value;
}

int out_of_class_operators::IncrementCounter::operator++(int) {
    return value++;
}

out_of_class_operators::ArrowTarget*
out_of_class_operators::ArrowHandle::operator->() {
    return target;
}

out_of_class_conversions::Truthy::operator bool() const noexcept {
    return value != 0;
}

out_of_class_conversions::Numeric::operator int() const {
    return value;
}

out_of_class_conversions::Pointer::operator int*() const {
    return value;
}

out_of_class_conversions::Reference::operator int&() {
    return value;
}

out_of_class_conversions::Function::operator FunctionPointer() const {
    return &out_of_class_conversion_function;
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

int cxx_out_of_class_destructor_probe() {
    int count = 0;
    {
        out_of_class_constructors::Tracked value;
        value.count = &count;
    }
    return count == 3 ? 0 : 1;
}

int cxx_out_of_class_operator_probe() {
    out_of_class_operators::Scalar left;
    left.value = 5;
    out_of_class_operators::Scalar result = left + 7;
    out_of_class_operators::Assigned assigned{};
    out_of_class_operators::Assigned& assigned_alias = (assigned = 23);
    assigned_alias += 5;
    assigned += 3;
    out_of_class_operators::Logical logical{};
    logical.value = 4;
    return result.value == 12 && assigned.value == 31 &&
           &assigned_alias == &assigned && !logical == false &&
           !(logical == 5) ? 0 : 1;
}

int cxx_out_of_class_call_operators_probe() {
    out_of_class_operators::Indexed indexed;
    indexed[0] = 4;
    indexed[1] = 8;
    out_of_class_operators::Callable callable;
    callable.offset = 11;
    int first = indexed[0];
    int second = indexed[1];
    int called = callable(7);
    out_of_class_operators::Negatable negatable;
    negatable.value = 9;
    out_of_class_operators::IncrementCounter counter;
    counter.value = 3;
    int postfix = counter++;
    int prefix = ++counter;
    out_of_class_operators::Dereference lvalue_dereference{13};
    out_of_class_operators::Dereference xvalue_dereference{17};
    int& lvalue_result = *lvalue_dereference;
    int&& xvalue_result =
        *static_cast<out_of_class_operators::Dereference&&>(
            xvalue_dereference);
    lvalue_result = 19;
    xvalue_result = 23;
    out_of_class_operators::ArrowTarget target;
    target.value = 29;
    out_of_class_operators::ArrowHandle handle;
    handle.target = &target;
    int arrow_value = handle->value;
    return first == 4 && second == 8 && called == 18 &&
           -negatable == -9 && postfix == 3 && prefix == 5 &&
           counter.value == 5 && lvalue_dereference.value == 19 &&
           xvalue_dereference.value == 23 && arrow_value == 29 ? 0 : 1;
}

int cxx_out_of_class_conversion_probe() {
    out_of_class_conversions::Truthy value;
    value.value = 1;
    bool converted = value;
    out_of_class_conversions::Numeric numeric;
    numeric.value = 31;
    int converted_numeric = numeric;
    int target = 47;
    out_of_class_conversions::Pointer pointer;
    pointer.value = &target;
    int* converted_pointer = pointer;
    out_of_class_conversions::Reference reference;
    reference.value = 12;
    int& converted_reference = reference;
    converted_reference = 53;
    out_of_class_conversions::Function function;
    out_of_class_conversions::FunctionPointer converted_function = function;
    return converted && converted_numeric == 31 &&
           converted_pointer == &target && *converted_pointer == 47
           && reference.value == 53 && converted_function(5) == 42
        ? 0 : 1;
}

int main() {
    return cxx_member_method_probe() == 53 &&
           cxx_pointer_method_probe() == 27 &&
           cxx_member_overload_probe() == 0 &&
           cxx_out_of_class_member_method_probe() ==
               13 + 113 + 6 + 6 + 9 + 10 + 6 + 18 + 6 + 2 + 6 &&
           cxx_out_of_class_constructor_probe() == 37 &&
           cxx_out_of_class_destructor_probe() == 0 &&
           cxx_out_of_class_operator_probe() == 0 &&
           cxx_out_of_class_call_operators_probe() == 0 &&
           cxx_out_of_class_conversion_probe() == 0
               ? 0 : 1;
}
