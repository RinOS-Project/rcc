class OperatorForms {
public:
    int value;

    int operator-() { return -value; }
    int operator++() { return ++value; }
    int operator++(int) {
        int old = value;
        ++value;
        return old;
    }
    int operator[](int index) { return value + index; }
    int operator()(int multiplier) { return value * multiplier; }
};

int main() {
    OperatorForms item{5};
    return -item == -5 && item++ == 5 && item.value == 6 &&
           item[2] == 8 && item(3) == 18 ? 0 : 1;
}
