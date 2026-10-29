struct AmbiguousArithmeticComparisonConversion {
    operator int() const { return 0; }
    operator long() const { return 0; }
};

bool compare(AmbiguousArithmeticComparisonConversion left,
             AmbiguousArithmeticComparisonConversion right) {
    return left < right;
}
