template<typename T>
struct GuidedBox {
    GuidedBox(int input) : value(input) {}
    T value;
};

template<typename T>
GuidedBox(T) -> GuidedBox<int>;

int main() {
    GuidedBox value(7);
    return value.value;
}
