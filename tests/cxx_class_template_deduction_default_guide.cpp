template <typename T>
struct DefaultGuideBox {
    T value;
};

template <typename T>
DefaultGuideBox(T, int = 0) -> DefaultGuideBox<T>;

int main() {
    DefaultGuideBox first{17};
    return first.value == 17 ? 0 : 1;
}
