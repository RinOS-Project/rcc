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

constexpr AggregateBox<int> constexpr_aggregate_box =
    make_constexpr_aggregate_box(9, 13);
static_assert(constexpr_aggregate_box.first +
              constexpr_aggregate_box.second == 22);

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

int main() {
    Value<int> value{21};
    Value<int*> pointer_value{9};
    Value<int**> pointer_pointer_value{2};
    Value<int***> pointer_pointer_pointer_value{3};
    Number<3> ordinary_number;
    Number<7> specialized_number;
    Pair<int, 3> ordinary_pair;
    Pair<long, 4> specialized_pair;
    Select<long, 3> ordinary_select;
    Select<int, 3> specialized_select;
    DefaultType<int> default_type{5};
    AggregateBox<int> aggregate_box = make_aggregate_box(20, 22);
    return value.doubled() == 42 && pointer_value.kind() == 9 &&
                   pointer_pointer_value.kind() == 22 &&
                   pointer_pointer_pointer_value.kind() == 33 &&
                   ordinary_number.kind() == 3 &&
                   specialized_number.kind() == 70 &&
                   ordinary_pair.kind() == 3 &&
                   specialized_pair.kind() == 40 &&
                   ordinary_select.kind() == 3 &&
                   specialized_select.kind() == 103 &&
                   default_type.kind() == 9 &&
                   aggregate_box.first + aggregate_box.second == 42 &&
                   constexpr_aggregate_box.first +
                       constexpr_aggregate_box.second == 22
               ? 0
               : 1;
}
