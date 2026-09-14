class Number {
public:
    int value;
};

int operator+(Number left, Number right) {
    return left.value + right.value;
}

int operator+(int left, Number right) {
    return left + right.value;
}

int operator-(Number value) {
    return -value.value;
}

int main() {
    Number left{2};
    Number right{5};
    int sum = left + right;
    int reverse = 2 + right;
    int negated = -left;
    return sum == 7 && reverse == 7 && negated == -2 ? 0 : 1;
}
