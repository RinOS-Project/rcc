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

template<typename T>
struct AggregateBox {
    T value;
    T other;
};

int main() {
    CtadBox parenthesized(7);
    CtadBox braced{9};
    GuidedBox guided(11);
    AggregateBox aggregate_braced{13, 14};
#if __cplusplus >= 202002L
    AggregateBox aggregate_parenthesized(15, 16);
#endif
    if (parenthesized.value != 7) return 1;
    if (braced.value != 9) return 2;
    if (guided.value != 11) return 3;
    if (aggregate_braced.value != 13 || aggregate_braced.other != 14) return 4;
#if __cplusplus >= 202002L
    if (aggregate_parenthesized.value != 15 ||
        aggregate_parenthesized.other != 16) return 5;
#endif
    return 0;
}
