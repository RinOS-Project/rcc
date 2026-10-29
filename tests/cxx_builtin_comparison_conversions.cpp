struct ArithmeticComparisonConversion {
    int value;
    operator int() const { return value; }
};

int main() {
    ArithmeticComparisonConversion left{2};
    ArithmeticComparisonConversion right{5};
    int scalar = 2;
    long wide = 5;
    double floating = 3.5;

    return (left < right) && (right > left) &&
           (left <= right) && (right >= left) &&
           (left != right) && !(left == right) &&
           (left == scalar) && (scalar == left) &&
           (right != scalar) &&
           (left < wide) && (wide > left) &&
           (left < floating) && (floating > left) ? 0 : 1;
}
