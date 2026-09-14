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
};

template<typename T>
class ConstructedBox {
    T value;

public:
    ConstructedBox(T input) : value(input) {}

    T get() {
        return value;
    }
};

int main() {
    Box<int> box{};
    ConstructedBox<int> constructed(7);
    return box.get() == 5 && box.add(3) == 8 &&
           constructed.get() == 7 ? 0 : 1;
}
