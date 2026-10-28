struct InequalityLeft {
    operator int() const { return 1; }
    friend bool operator!=(const InequalityLeft&, int) { return false; }
};

struct InequalityRight {
    operator int() const { return 2; }
    friend bool operator==(const InequalityRight&, int) { return false; }
};

bool compare_inequality(InequalityLeft left, InequalityRight right) {
    return left != right;
}
