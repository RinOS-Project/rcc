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

int main() {
    Value<int> value{21};
    Value<int*> pointer_value{9};
    Value<int**> pointer_pointer_value{2};
    return value.doubled() == 42 && pointer_value.kind() == 9 &&
                   pointer_pointer_value.kind() == 22 ? 0 : 1;
}
