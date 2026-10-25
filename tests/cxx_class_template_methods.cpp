template<typename T>
class Box {
public:
    T value = 5;

    T get() {
        return value;
    }

    T add(T other) {
        return value + other;
    }

    template<typename U = T>
        requires (sizeof(U) <= sizeof(int))
    int constrained_size() const requires (sizeof(U) > sizeof(char)) {
        return sizeof(U);
    }
};

struct IntMemberTemplateValue {
    using value_type = int;
    int value;
};

struct LongMemberTemplateValue {
    using value_type = long;
    long value;
};

class OutOfClassMemberTemplate {
public:
    template<typename U>
    typename U::value_type convert(U value) const;

    template<typename U = int>
    U defaulted_value() const;

    template<int Value = 7>
    int non_type_default() const;
};

template<typename U>
typename U::value_type
OutOfClassMemberTemplate::convert(U value) const {
    return value.value;
}

template<typename U>
U OutOfClassMemberTemplate::defaulted_value() const {
    return static_cast<U>(7);
}

template<int Value>
int OutOfClassMemberTemplate::non_type_default() const {
    return Value;
}

template<typename T>
concept HasConstrainedSize = requires(T value) {
    value.constrained_size();
};

static_assert(HasConstrainedSize<Box<int>>);
static_assert(!HasConstrainedSize<Box<char>>);
static_assert(!HasConstrainedSize<Box<long long>>);

template<typename T>
class ConstructedBox {
    T value;

public:
    ConstructedBox(T input) : value(input) {}

    T get() {
        return value;
    }
};

template<typename T>
class ReferencedBox {
    T& value;

public:
    explicit ReferencedBox(T& input) : value(input) {}

    T get() {
        return value;
    }

    void set(T updated) {
        value = updated;
    }
};

int main() {
    Box<int> box{};
    OutOfClassMemberTemplate member_template{};
    IntMemberTemplateValue int_member_value{13};
    LongMemberTemplateValue long_member_value{17};
    ConstructedBox<int> constructed(7);
    int referenced_value = 5;
    ReferencedBox<int> referenced{referenced_value};
    referenced.set(11);
    return box.get() == 5 && box.add(3) == 8 &&
           member_template.convert(int_member_value) == 13 &&
           member_template.convert(long_member_value) == 17 &&
           member_template.defaulted_value() == 7 &&
           member_template.non_type_default() == 7 &&
           member_template.non_type_default<9>() == 9 &&
           box.constrained_size() == sizeof(int) &&
           constructed.get() == 7 && referenced.get() == 11 &&
           referenced_value == 11 ? 0 : 1;
}
