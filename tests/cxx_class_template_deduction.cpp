template<typename T>
struct CtadBox {
    T value;

    CtadBox(T input) : value(input) {}
};

template<typename T>
struct GuidedBox {
    T value;

    GuidedBox(int input) : value(input) {}
};

template<typename T>
GuidedBox(T) -> GuidedBox<int>;

int main() {
    CtadBox parenthesized(7);
    CtadBox braced{9};
    GuidedBox guided(11);
    if (parenthesized.value != 7) return 1;
    if (braced.value != 9) return 2;
    if (guided.value != 11) return 3;
    return 0;
}
