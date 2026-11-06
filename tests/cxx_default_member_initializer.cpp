class DefaultMemberInitializer {
public:
    explicit DefaultMemberInitializer(int value) : override_(value) {}

    DefaultMemberInitializer() : extra_(2) {
        override_ += 1;
    }

    int total() const {
        return inherited_ + override_ + extra_;
    }

private:
    int inherited_ = 5;
    int override_ = 7;
    int extra_ = 9;
};

class ImplicitDefaultMemberInitializer {
public:
    int total() const {
        return first_ + second_;
    }

private:
    int first_ = 6;
    int second_ = 4;
};

struct DefaultedDefaultMemberInitializer {
    int value = 47;

    DefaultedDefaultMemberInitializer() = default;
};

struct DefaultArgumentDefaultConstructor {
    explicit DefaultArgumentDefaultConstructor(int initial = 53)
        : value(initial) {}

    int value;
};

static int default_member_initialization_order;

static int record_default_member_initialization_order(int value) {
    default_member_initialization_order =
        default_member_initialization_order * 10 + value;
    return value;
}

struct VirtualBaseDefaultMemberOrder {
    int value = record_default_member_initialization_order(1);
};

struct DirectBaseDefaultMemberOrder {
    int value = record_default_member_initialization_order(2);
};

struct MemberDefaultMemberOrder {
    int value = record_default_member_initialization_order(3);
};

struct DefaultMemberInitializationOrder
    : virtual VirtualBaseDefaultMemberOrder,
      DirectBaseDefaultMemberOrder {
    MemberDefaultMemberOrder member;
    int last = record_default_member_initialization_order(4);
};

struct DefaultMemberPair {
    int first;
    int second;
};

class AggregateDefaultMemberInitializer {
public:
    int total() const {
        return pair_.first + pair_.second;
    }

private:
    DefaultMemberPair pair_ = DefaultMemberPair{3, 4};
};

class NonScalarDefaultMember {
public:
    int value = 3;
};

class ArrayDefaultMemberInitializer {
public:
    NonScalarDefaultMember nested{};
};

struct ReferenceDefaultMemberInitializer {
    int value = 11;
    int& alias = value;
};

struct InheritedReferenceDefaultMemberBase {
    int value = 13;
};

struct InheritedReferenceDefaultMemberInitializer
    : InheritedReferenceDefaultMemberBase {
    int& alias = value;
};

struct ExplicitThisDefaultMemberInitializer {
    int value = 29;
    int alias = this->value;
};

class ExplicitArrayDefaultMemberElement {
public:
    explicit ExplicitArrayDefaultMemberElement(int value) : value(value) {}

    int value;
};

struct ClassArrayDefaultMemberInitializer {
    ExplicitArrayDefaultMemberElement values[2] = {
        ExplicitArrayDefaultMemberElement(17),
        ExplicitArrayDefaultMemberElement(19)};
};

union ScalarDefaultMemberUnion {
    int integer;
    double floating;
};

static int scalar_default_member_union_value() noexcept {
    return 23;
}

struct UnionDefaultMemberInitializer {
    ScalarDefaultMemberUnion value = {
        .integer = scalar_default_member_union_value()};
};

static int default_member_cleanup_order;

struct FirstDefaultMemberCleanup {
    ~FirstDefaultMemberCleanup() {
        default_member_cleanup_order =
            default_member_cleanup_order * 10 + 1;
    }
};

struct SecondDefaultMemberCleanup {
    ~SecondDefaultMemberCleanup() {
        default_member_cleanup_order =
            default_member_cleanup_order * 10 + 2;
    }
};

static int throw_default_member_initializer() {
    throw 31;
}

struct PartialDefaultMemberInitializer {
    FirstDefaultMemberCleanup first;
    SecondDefaultMemberCleanup second;
    int value = throw_default_member_initializer();
};

extern "C" {
int cxx_default_member_static_initializations;
int cxx_default_member_static_destructions;
}

static int initialize_static_default_member() {
    ++cxx_default_member_static_initializations;
    return 37;
}

struct StaticDefaultMemberInitializer {
    int value = initialize_static_default_member();

    ~StaticDefaultMemberInitializer() {
        ++cxx_default_member_static_destructions;
    }
};

static StaticDefaultMemberInitializer& static_default_member_initializer() {
    static StaticDefaultMemberInitializer value;
    return value;
}

extern "C" int cxx_default_member_initializer() {
    DefaultMemberInitializer explicit_value(4);
    DefaultMemberInitializer default_value{};
    DefaultMemberInitializer* heap = new DefaultMemberInitializer{};
    ImplicitDefaultMemberInitializer* implicit =
        new ImplicitDefaultMemberInitializer{};
    ImplicitDefaultMemberInitializer* implicit_paren =
        new ImplicitDefaultMemberInitializer();
    ImplicitDefaultMemberInitializer* implicit_plain =
        new ImplicitDefaultMemberInitializer;
    ImplicitDefaultMemberInitializer* implicit_array =
        new ImplicitDefaultMemberInitializer[2];
    ImplicitDefaultMemberInitializer* implicit_value_array =
        new ImplicitDefaultMemberInitializer[2]{};
    ImplicitDefaultMemberInitializer* implicit_paren_array =
        new ImplicitDefaultMemberInitializer[2]();
    DefaultedDefaultMemberInitializer defaulted_member;
    DefaultedDefaultMemberInitializer value_initialized_defaulted_member{};
    DefaultArgumentDefaultConstructor default_argument_member;
    DefaultArgumentDefaultConstructor* default_argument_array =
        new DefaultArgumentDefaultConstructor[2];
    DefaultMemberInitializationOrder initialization_order;
    AggregateDefaultMemberInitializer aggregate{};
    AggregateDefaultMemberInitializer* aggregate_heap =
        new AggregateDefaultMemberInitializer{};
    ArrayDefaultMemberInitializer* aggregate_array =
        new ArrayDefaultMemberInitializer[2]{};
    ReferenceDefaultMemberInitializer reference_member;
    InheritedReferenceDefaultMemberInitializer inherited_reference_member;
    ExplicitThisDefaultMemberInitializer explicit_this_member;
    ClassArrayDefaultMemberInitializer class_array_member;
    UnionDefaultMemberInitializer union_member;
    int caught_default_member_exception = 0;
    try {
        PartialDefaultMemberInitializer partial;
        (void)partial;
    } catch (int value) {
        caught_default_member_exception = value;
    }
    if (caught_default_member_exception != 31 ||
        default_member_cleanup_order != 21) {
        return 1;
    }
    if (default_member_initialization_order != 1234) return 1;
    if (static_default_member_initializer().value != 37 ||
        static_default_member_initializer().value != 37 ||
        cxx_default_member_static_initializations != 1 ||
        cxx_default_member_static_destructions != 0) {
        return 1;
    }
    int result = explicit_value.total() + default_value.total() +
                 heap->total() + implicit->total() + implicit_paren->total() +
                 implicit_plain->total() + implicit_array[0].total() +
                 implicit_array[1].total() +
                 implicit_value_array[0].total() +
                 implicit_value_array[1].total() +
                 implicit_paren_array[0].total() +
                 implicit_paren_array[1].total() + aggregate.total() +
                 aggregate_heap->total() + aggregate_array[0].nested.value +
                 aggregate_array[1].nested.value + defaulted_member.value +
                 value_initialized_defaulted_member.value +
                 default_argument_member.value +
                 default_argument_array[0].value +
                 default_argument_array[1].value +
                 reference_member.alias +
                 inherited_reference_member.alias +
                 explicit_this_member.alias +
                 class_array_member.values[0].value +
                 class_array_member.values[1].value +
                 union_member.value.integer;
    delete heap;
    delete implicit;
    delete implicit_paren;
    delete implicit_plain;
    delete[] implicit_array;
    delete[] implicit_value_array;
    delete[] implicit_paren_array;
    delete aggregate_heap;
    delete[] aggregate_array;
    delete[] default_argument_array;
    return result == 523 ? 0 : 1;
}
