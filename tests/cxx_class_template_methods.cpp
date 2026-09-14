template<typename T>
class Box {
public:
    T value = 5;

    T get() {
        return value;
    }
};

int main() {
    Box<int> box{};
    return box.get() == 5 ? 0 : 1;
}
