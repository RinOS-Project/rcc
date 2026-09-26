template<typename T>
struct CtadBox {
    T value;

    CtadBox(T input) : value(input) {}
};

int main() {
    CtadBox parenthesized(7);
    CtadBox braced{9};
    if (parenthesized.value != 7) return 1;
    if (braced.value != 9) return 2;
    return 0;
}
