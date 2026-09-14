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

int main() {
    Box<int> box{};
    return box.get() == 5 && box.add(3) == 8 ? 0 : 1;
}
