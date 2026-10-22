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
    ConstructedBox<int> constructed(7);
    int referenced_value = 5;
    ReferencedBox<int> referenced{referenced_value};
    referenced.set(11);
    return box.get() == 5 && box.add(3) == 8 &&
           constructed.get() == 7 && referenced.get() == 11 &&
           referenced_value == 11 ? 0 : 1;
}
