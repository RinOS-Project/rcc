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

int main() {
    Value<int> value{21};
    return value.doubled() == 42 ? 0 : 1;
}
