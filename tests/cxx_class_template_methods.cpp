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

template<typename T>
class OutOfClassClassTemplateOwner {
    T value;

public:
    explicit OutOfClassClassTemplateOwner(T input) : value(input) {}

    T get() const;
    operator const T*() const &;
    operator T*() &&;
    T operator+(T rhs) const;
    T operator()(T rhs) const;
    T operator[](T index) const;

    template<typename U = T>
    U add_to(U input) const;

    template<int Multiplier = 2>
    T multiply(T input) const;
};

template<typename T>
class OutOfClassReferenceConversionOwner {
    T value;

public:
    explicit OutOfClassReferenceConversionOwner(T input) : value(input) {}
    operator const T&() const;
};

template<typename T>
class NestedConversionOwner {
public:
    class Inner {
        T value;

    public:
        explicit Inner(T input) : value(input) {}
        operator const T*() const &;
    };
};

template<typename T>
class NestedClassTemplateConversionOwner {
public:
    template<typename U>
    class Inner {
        T owner_value;
        U value;

    public:
        Inner(T owner_input, U input)
            : owner_value(owner_input), value(input) {}
        operator const U*() const &;
    };
};

namespace qualified_template_owner {

template<typename T>
class ConversionOwner {
    T value;

public:
    explicit ConversionOwner(T input) : value(input) {}
    operator const T*() const &;
};

}

namespace qualified_template_owner::nested {

template<typename T>
class DeepConversionOwner {
    T value;

public:
    explicit DeepConversionOwner(T input) : value(input) {}
    operator const T*() const &;
};

}

struct OutOfClassArrowTarget {
    int value;
};

template<typename T>
class OutOfClassArrowOwner {
    T* target;

public:
    explicit OutOfClassArrowOwner(T* input) : target(input) {}
    T* operator->() const;
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
T OutOfClassClassTemplateOwner<T>::get() const {
    return value;
}

template<typename T>
OutOfClassClassTemplateOwner<T>::operator const T*() const & {
    return &value;
}

template<typename T>
OutOfClassClassTemplateOwner<T>::operator T*() && {
    return &value;
}

template<typename T>
OutOfClassReferenceConversionOwner<T>::operator const T&() const {
    return value;
}

template<typename T>
NestedConversionOwner<T>::Inner::operator const T*() const & {
    return &value;
}

template<typename T>
template<typename U>
NestedClassTemplateConversionOwner<T>::Inner<U>::operator const U*() const & {
    return &value;
}

template<typename T>
qualified_template_owner::ConversionOwner<T>::operator const T*() const & {
    return &value;
}

template<typename T>
qualified_template_owner::nested::DeepConversionOwner<T>::operator const T*() const & {
    return &value;
}

template<typename T>
T OutOfClassClassTemplateOwner<T>::operator+(T rhs) const {
    return value + rhs;
}

template<typename T>
T OutOfClassClassTemplateOwner<T>::operator()(T rhs) const {
    return value + rhs;
}

template<typename T>
T OutOfClassClassTemplateOwner<T>::operator[](T index) const {
    return value + index;
}

template<typename T>
T* OutOfClassArrowOwner<T>::operator->() const {
    return target;
}

template<typename T>
template<typename U>
U OutOfClassClassTemplateOwner<T>::add_to(U input) const {
    return input + static_cast<U>(value);
}

template<typename T>
template<int Multiplier>
T OutOfClassClassTemplateOwner<T>::multiply(T input) const {
    return input * Multiplier;
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
    OutOfClassClassTemplateOwner<int> out_of_class_owner{4};
    OutOfClassReferenceConversionOwner<int> reference_conversion_owner{6};
    const int& converted_reference = reference_conversion_owner;
    NestedConversionOwner<int>::Inner nested_owner{7};
    NestedClassTemplateConversionOwner<int>::Inner<long> nested_template_owner{
        12, 13L};
    qualified_template_owner::ConversionOwner<int> qualified_owner{8};
    qualified_template_owner::nested::DeepConversionOwner<int> deep_owner{9};
    OutOfClassArrowTarget arrow_target{31};
    OutOfClassArrowOwner<OutOfClassArrowTarget> arrow_owner{&arrow_target};
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
           out_of_class_owner.get() == 4 &&
           *static_cast<const int*>(out_of_class_owner) == 4 &&
           *static_cast<int*>(OutOfClassClassTemplateOwner<int>{5}) == 5 &&
           converted_reference == 6 &&
           static_cast<const int&>(reference_conversion_owner) == 6 &&
           *static_cast<const int*>(nested_owner) == 7 &&
           *static_cast<const long*>(nested_template_owner) == 13L &&
           *static_cast<const int*>(qualified_owner) == 8 &&
           *static_cast<const int*>(deep_owner) == 9 &&
           out_of_class_owner + 5 == 9 &&
           out_of_class_owner(6) == 10 &&
           out_of_class_owner[7] == 11 &&
           arrow_owner->value == 31 &&
           out_of_class_owner.add_to(3) == 7 &&
           out_of_class_owner.add_to<long>(9L) == 13L &&
           out_of_class_owner.multiply(4) == 8 &&
           out_of_class_owner.multiply<3>(4) == 12 &&
           box.constrained_size() == sizeof(int) &&
           constructed.get() == 7 && referenced.get() == 11 &&
           referenced_value == 11 ? 0 : 1;
}
