template<typename T>
class Value {
public:
    T value;
};

template<>
class Value<int> {
public:
    int value;

    int doubled() {
        return value + value;
    }
};

template<typename T>
class Value<T*> {
public:
    int marker;

    int kind() {
        return marker;
    }
};

template<typename T>
class Value<T**> {
public:
    int marker;

    int kind() {
        return marker + 20;
    }
};

template<typename T>
class Value<T***> {
public:
    int marker;

    int kind() {
        return marker + 30;
    }
};

template<typename T>
class Qualified {
public:
    int kind() {
        return 0;
    }
};

template<typename T>
class Qualified<T*> {
public:
    int kind() {
        return 10;
    }
};

template<typename T>
class Qualified<const T*> {
public:
    int kind() {
        return 20;
    }
};

template<typename T>
class ExactQualified {
public:
    int kind() {
        return 0;
    }
};

template<>
class ExactQualified<const int> {
public:
    int kind() {
        return 30;
    }
};

template<int N>
class Number {
public:
    int kind() {
        return N;
    }
};

template<>
class Number<7> {
public:
    int kind() {
        return 70;
    }
};

template<typename T, int N>
class Pair {
public:
    int kind() {
        return N;
    }
};

template<typename T>
class Pair<T, 4> {
public:
    int kind() {
        return 40;
    }
};

template<typename T, int N>
class Select {
public:
    int kind() {
        return N;
    }
};

template<typename T>
struct AggregateBox {
    T first;
    T second;
};

template<typename T>
AggregateBox<T> make_aggregate_box(T first, T second) {
    return AggregateBox<T>{first, second};
}

template<typename T>
constexpr AggregateBox<T> make_constexpr_aggregate_box(T first, T second) {
    return AggregateBox<T>{first, second};
}

template<typename T>
consteval AggregateBox<T> make_consteval_aggregate_box(T first, T second) {
    return AggregateBox<T>{first, second};
}

constexpr AggregateBox<int> constexpr_aggregate_box =
    make_constexpr_aggregate_box(9, 13);
static_assert(constexpr_aggregate_box.first +
              constexpr_aggregate_box.second == 22);
constexpr AggregateBox<int> consteval_aggregate_box =
    make_consteval_aggregate_box(14, 28);
static_assert(consteval_aggregate_box.first +
              consteval_aggregate_box.second == 42);

template<typename T, typename U = T>
class DefaultType {
public:
    U value;

    int kind() {
        return sizeof(U) + value;
    }
};

template<int N>
class Select<int, N> {
public:
    int kind() {
        return N + 100;
    }
};

template<int N>
class ConstrainedNumber {
public:
    int kind() {
        return 1;
    }
};

template<int N> requires (N > 3)
class ConstrainedNumber<N> {
public:
    int kind() {
        return 50;
    }
};

template<typename... Types>
struct MixedPackPartial {
    static constexpr int kind = 1;
};

template<typename Head, typename... Tail>
struct MixedPackPartial<Head, Tail...> {
    static constexpr int kind = 2;
};

template<typename... Types>
struct DependentMixedPackPartial {
    using type = char;
};

template<typename Head, typename... Tail>
struct DependentMixedPackPartial<Head, Tail...> {
    using type = short;
};

template<typename... Types>
struct DependentMixedPackPartialUse {
    using type = typename DependentMixedPackPartial<Types...>::type;
};

static_assert(sizeof(typename DependentMixedPackPartialUse<>::type) ==
              sizeof(char));
static_assert(sizeof(typename DependentMixedPackPartialUse<int>::type) ==
              sizeof(short));
static_assert(sizeof(typename DependentMixedPackPartialUse<int, short>::type) ==
              sizeof(short));

template<typename... Types>
struct OrderedMixedPackPartial {
    static constexpr int kind = 0;
    using type = char;
};

template<typename Head, typename... Tail>
struct OrderedMixedPackPartial<Head, Tail...> {
    static constexpr int kind = 1;
    using type = short;
};

template<typename Head, typename... Tail>
struct OrderedMixedPackPartial<Head*, Tail...> {
    static constexpr int kind = 2;
    using type = long;
};

template<typename... Types>
struct DependentOrderedMixedPackPartialUse {
    using type = typename OrderedMixedPackPartial<Types...>::type;
};

static_assert(OrderedMixedPackPartial<>::kind == 0);
static_assert(OrderedMixedPackPartial<int>::kind == 1);
static_assert(OrderedMixedPackPartial<int*, char>::kind == 2);
static_assert(sizeof(typename DependentOrderedMixedPackPartialUse<
                  int*, char, long>::type) == sizeof(long));

template<typename... Types>
struct ConstrainedOrderedPackPartial {
    static constexpr int kind = 0;
    using type = char;
};

template<typename Head, typename... Tail>
struct ConstrainedOrderedPackPartial<Head, Tail...> {
    static constexpr int kind = 1;
    using type = short;
};

template<typename Head, typename... Tail>
    requires (sizeof(Head) > 0)
struct ConstrainedOrderedPackPartial<Head, Tail...> {
    static constexpr int kind = 2;
    using type = long;
};

template<typename... Types>
struct DependentConstrainedOrderedPackPartialUse {
    using type = typename ConstrainedOrderedPackPartial<Types...>::type;
};

static_assert(ConstrainedOrderedPackPartial<int>::kind == 2);
static_assert(ConstrainedOrderedPackPartial<int, char>::kind == 2);
static_assert(sizeof(typename DependentConstrainedOrderedPackPartialUse<
                  int, char>::type) == sizeof(long));

template<typename... Types>
struct StructuralBeforeConstraintPack {
    static constexpr int kind = 0;
    using type = char;
};

template<typename Head, typename... Tail>
    requires (sizeof(Head) > 0)
struct StructuralBeforeConstraintPack<Head, Tail...> {
    static constexpr int kind = 1;
    using type = short;
};

template<typename Head, typename... Tail>
struct StructuralBeforeConstraintPack<Head*, Tail...> {
    static constexpr int kind = 2;
    using type = long;
};

template<typename... Types>
struct DependentStructuralBeforeConstraintPackUse {
    using type = typename StructuralBeforeConstraintPack<Types...>::type;
};

static_assert(StructuralBeforeConstraintPack<int*, char>::kind == 2);
static_assert(sizeof(typename DependentStructuralBeforeConstraintPackUse<
                  int*, char>::type) == sizeof(long));

template<typename T>
concept HasFirstTag = requires { typename T::first_tag; };

template<typename T>
concept HasSecondTag = requires { typename T::second_tag; };

template<typename First, typename Second>
struct CrossedConstraintPack {
    static constexpr int kind = 0;
};

template<typename T, typename U> requires HasFirstTag<T>
struct CrossedConstraintPack<T*, U> {
    static constexpr int kind = 1;
};

template<typename T, typename U> requires HasSecondTag<U>
struct CrossedConstraintPack<T, U*> {
    static constexpr int kind = 2;
};

struct FirstTagOnly { using first_tag = int; };
struct SecondTagOnly { using second_tag = int; };

static_assert(CrossedConstraintPack<FirstTagOnly*, FirstTagOnly*>::kind == 1);
static_assert(CrossedConstraintPack<SecondTagOnly*, SecondTagOnly*>::kind == 2);

template<typename T>
struct UnequalExpansionBox {};

template<typename... Types>
struct UnequalExpansionPack {
    static constexpr int kind = 0;
};

template<typename Head, typename... Tail>
struct UnequalExpansionPack<Head, UnequalExpansionBox<Tail>...> {
    static constexpr int kind = 1;
};

template<typename Head, typename... Tail>
struct UnequalExpansionPack<UnequalExpansionBox<Head>, Tail...> {
    static constexpr int kind = 2;
};

static_assert(UnequalExpansionPack<>::kind == 0);
static_assert(UnequalExpansionPack<int, UnequalExpansionBox<char>>::kind == 1);
static_assert(UnequalExpansionPack<UnequalExpansionBox<int>, char>::kind == 2);

struct NestedOrderGeneral {};
struct NestedOrderSecondPointer {};
struct NestedOrderTemplateId {};
struct NestedOrderInnerPointer {};

template<typename First, typename Second>
struct NestedTemplateOrder {
    using selected = NestedOrderGeneral;
};

template<typename First, typename Second>
struct NestedTemplateOrder<First, Second*> {
    using selected = NestedOrderSecondPointer;
};

template<typename Inner, typename Second>
struct NestedTemplateOrder<UnequalExpansionBox<Inner>, Second*> {
    using selected = NestedOrderTemplateId;
};

template<typename Inner, typename Second>
struct NestedTemplateOrder<UnequalExpansionBox<Inner*>, Second*> {
    using selected = NestedOrderInnerPointer;
};

template<typename Left, typename Right>
struct NestedOrderSameType {};

template<typename Type>
struct NestedOrderSameType<Type, Type> {
    using result = char;
};

static_assert(sizeof(typename NestedOrderSameType<
                  NestedTemplateOrder<UnequalExpansionBox<int>, char*>::selected,
                  NestedOrderTemplateId>::result) == sizeof(char));
static_assert(sizeof(typename NestedOrderSameType<
                  NestedTemplateOrder<UnequalExpansionBox<int*>, char*>::selected,
                  NestedOrderInnerPointer>::result) == sizeof(char));

int main() {
    Value<int> value{21};
    Value<int*> pointer_value{9};
    Value<int**> pointer_pointer_value{2};
    Value<int***> pointer_pointer_pointer_value{3};
    Qualified<int*> qualified_pointer;
    Qualified<const int*> qualified_const_pointer;
    ExactQualified<int> exact_unqualified;
    ExactQualified<const int> exact_const;
    Number<3> ordinary_number;
    Number<7> specialized_number;
    Pair<int, 3> ordinary_pair;
    Pair<long, 4> specialized_pair;
    Select<long, 3> ordinary_select;
    Select<int, 3> specialized_select;
    ConstrainedNumber<2> constrained_primary;
    ConstrainedNumber<5> constrained_specialized;
    DefaultType<int> default_type{5};
    AggregateBox<int> aggregate_box = make_aggregate_box(20, 22);
    constexpr int mixed_pack_empty = MixedPackPartial<>::kind;
    constexpr int mixed_pack_single = MixedPackPartial<int>::kind;
    constexpr int mixed_pack_double = MixedPackPartial<int, short>::kind;
    constexpr int mixed_pack_triple =
        MixedPackPartial<int, short, long>::kind;
    constexpr int ordered_mixed_pack_pointer =
        OrderedMixedPackPartial<int*, char>::kind;
    constexpr int constrained_ordered_pack =
        ConstrainedOrderedPackPartial<int, char>::kind;
    constexpr int structural_before_constraint_pack =
        StructuralBeforeConstraintPack<int*, char>::kind;
    return value.doubled() == 42 && pointer_value.kind() == 9 &&
                   pointer_pointer_value.kind() == 22 &&
                   pointer_pointer_pointer_value.kind() == 33 &&
                   qualified_pointer.kind() == 10 &&
                   qualified_const_pointer.kind() == 20 &&
                   exact_unqualified.kind() == 0 &&
                   exact_const.kind() == 30 &&
                   ordinary_number.kind() == 3 &&
                   specialized_number.kind() == 70 &&
                   ordinary_pair.kind() == 3 &&
                   specialized_pair.kind() == 40 &&
                   ordinary_select.kind() == 3 &&
                   specialized_select.kind() == 103 &&
                   constrained_primary.kind() == 1 &&
                   constrained_specialized.kind() == 50 &&
                   default_type.kind() == 9 &&
                   aggregate_box.first + aggregate_box.second == 42 &&
                   constexpr_aggregate_box.first +
                       constexpr_aggregate_box.second == 22 &&
                   consteval_aggregate_box.first +
                       consteval_aggregate_box.second == 42 &&
                   mixed_pack_empty == 1 && mixed_pack_single == 2 &&
                   mixed_pack_double == 2 && mixed_pack_triple == 2 &&
                   ordered_mixed_pack_pointer == 2 &&
                   constrained_ordered_pack == 2 &&
                   structural_before_constraint_pack == 2
               ? 0
               : 1;
}
