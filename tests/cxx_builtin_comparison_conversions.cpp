struct ArithmeticComparisonConversion {
    int value;
    operator int() const { return value; }
};

struct PointerComparisonConversion {
    int* value;
    operator int*() const { return value; }
};

enum ComparisonCode { ComparisonLow = 2, ComparisonHigh = 5 };
enum OtherComparisonCode { OtherComparisonValue = 7 };

struct EnumComparisonConversion {
    ComparisonCode value;
    operator ComparisonCode() const { return value; }
};

int main() {
    ArithmeticComparisonConversion left{2};
    ArithmeticComparisonConversion right{5};
    int scalar = 2;
    long wide = 5;
    double floating = 3.5;
    int values[] = {2, 5};
    PointerComparisonConversion pointer_left{&values[0]};
    PointerComparisonConversion pointer_right{&values[1]};
    EnumComparisonConversion enum_left{ComparisonLow};
    EnumComparisonConversion enum_right{ComparisonHigh};

    return (left < right) && (right > left) &&
           (left <= right) && (right >= left) &&
           (left != right) && !(left == right) &&
           (left == scalar) && (scalar == left) &&
           (right != scalar) &&
           (left < wide) && (wide > left) &&
           (left < floating) && (floating > left) &&
           (pointer_left == &values[0]) && (&values[0] == pointer_left) &&
           (pointer_left != &values[1]) &&
           (pointer_left < pointer_right) &&
           (&values[0] < pointer_right) &&
           (enum_left == ComparisonLow) &&
           (enum_left < enum_right) &&
           (enum_left < OtherComparisonValue) ? 0 : 1;
}
