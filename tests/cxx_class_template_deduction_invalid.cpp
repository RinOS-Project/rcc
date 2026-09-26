template<typename T>
struct CtadBox {
    CtadBox(T input) : value(input) {}
    T value;
};

int main() {
    CtadBox value(7);
    return value.value;
}
