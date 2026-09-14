class Number {
public:
    int value;

    int operator+(int rhs) {
        return value + rhs;
    }
};

int main() {
    Number number{5};
    return number + 7 == 12 ? 0 : 1;
}
