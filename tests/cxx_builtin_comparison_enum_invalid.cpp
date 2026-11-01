enum FirstComparisonCode { FirstComparisonValue };
enum class SecondComparisonCode { SecondComparisonValue };

struct FirstEnumComparisonConversion {
    operator FirstComparisonCode() const { return FirstComparisonValue; }
};

bool compare_unrelated_enums(FirstEnumComparisonConversion left,
                             SecondComparisonCode right) {
    return left < right;
}
