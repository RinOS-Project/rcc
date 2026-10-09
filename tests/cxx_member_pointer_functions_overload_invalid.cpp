struct OverloadedMemberFunctionOwner {
    int apply(int value) { return value; }
    int apply(double value) { return static_cast<int>(value); }
};

int (OverloadedMemberFunctionOwner::*invalid_method)() =
    &OverloadedMemberFunctionOwner::apply;

void overloaded_address_without_target() {
    &OverloadedMemberFunctionOwner::apply;
}
