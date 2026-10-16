struct StructuralMemberFunctionTarget {
    int value;

    int add(int amount) const {
        return value + amount;
    }

    int subtract(int amount) const {
        return value - amount;
    }
};

struct StructuralMemberFunctionKey {
    int (StructuralMemberFunctionTarget::*method)(int) const;
};

template <StructuralMemberFunctionKey Value>
struct StructuralMemberFunctionOwner {
    static int invoke(const StructuralMemberFunctionTarget& target,
                      int amount) {
        return (target.*Value.method)(amount);
    }
};

int main() {
    const StructuralMemberFunctionTarget target{40};
    return StructuralMemberFunctionOwner<
               StructuralMemberFunctionKey{
                   &StructuralMemberFunctionTarget::add}>::invoke(target, 2)
                == 42 &&
            StructuralMemberFunctionOwner<
                StructuralMemberFunctionKey{
                    &StructuralMemberFunctionTarget::subtract}>::invoke(
                target, 2) == 38
        ? 0 : 1;
}
