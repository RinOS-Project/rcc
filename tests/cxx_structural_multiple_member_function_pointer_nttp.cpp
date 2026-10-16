struct StructuralMultipleMemberFunctionTarget {
    int value;

    int add(int amount) const {
        return value + amount;
    }

    int subtract(int amount) const {
        return value - amount;
    }
};

struct StructuralMultipleMemberFunctionKey {
    int (StructuralMultipleMemberFunctionTarget::*first)(int) const;
    int (StructuralMultipleMemberFunctionTarget::*second)(int) const;
};

template <StructuralMultipleMemberFunctionKey Value>
struct StructuralMultipleMemberFunctionOwner {
    static int select(const StructuralMultipleMemberFunctionTarget& target,
                      bool second, int amount) {
        return second ? (target.*Value.second)(amount)
                      : (target.*Value.first)(amount);
    }
};

int main() {
    const StructuralMultipleMemberFunctionTarget target{40};
    return StructuralMultipleMemberFunctionOwner<
               StructuralMultipleMemberFunctionKey{
                   &StructuralMultipleMemberFunctionTarget::add,
                   &StructuralMultipleMemberFunctionTarget::subtract}>::select(
                target, false, 2) == 42 &&
            StructuralMultipleMemberFunctionOwner<
                StructuralMultipleMemberFunctionKey{
                    &StructuralMultipleMemberFunctionTarget::subtract,
                    &StructuralMultipleMemberFunctionTarget::add}>::select(
                target, false, 2) == 38
        ? 0 : 1;
}
