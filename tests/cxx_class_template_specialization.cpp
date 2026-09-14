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

int main() {
    Value<int> value{21};
    Value<int*> pointer_value{9};
    return value.doubled() == 42 && pointer_value.kind() == 9 ? 0 : 1;
}
