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

int main() {
    Value<int> value{21};
    Value<int*> pointer_value{9};
    Value<int**> pointer_pointer_value{2};
    Number<3> ordinary_number;
    Number<7> specialized_number;
    Pair<int, 3> ordinary_pair;
    Pair<long, 4> specialized_pair;
    return value.doubled() == 42 && pointer_value.kind() == 9 &&
                   pointer_pointer_value.kind() == 22 &&
                   ordinary_number.kind() == 3 &&
                   specialized_number.kind() == 70 &&
                   ordinary_pair.kind() == 3 &&
                   specialized_pair.kind() == 40 ? 0 : 1;
}
