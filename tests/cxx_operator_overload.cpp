class Number {
public:
    int value;

    int operator+(int rhs) {
        return value + rhs;
    }
};

struct Addressable {
    int value;
};

Addressable operator&(Addressable left, Addressable right);

struct UnrelatedOperatorOperand {
    int value;
};

UnrelatedOperatorOperand operator+(UnrelatedOperatorOperand left, int right);

namespace associated_operator {
struct Operand {
    int value;
};

Operand operator+(Operand left, int right) {
    return {left.value + right};
}
}  // namespace associated_operator

Addressable* address_of_reference(Addressable& value) {
    return &value;
}

struct ReferenceHolder {
    int& value;

    int* address() const {
        return &value;
    }
};

int main() {
    Number number{5};
    Addressable addressable{11};
    int stored = 17;
    ReferenceHolder holder{stored};
    associated_operator::Operand adl_operand{19};
    associated_operator::Operand adl_result = adl_operand + 2;
    return number + 7 == 12 &&
                   address_of_reference(addressable) == &addressable &&
                   holder.address() == &stored && adl_result.value == 21
        ? 0 : 1;
}
