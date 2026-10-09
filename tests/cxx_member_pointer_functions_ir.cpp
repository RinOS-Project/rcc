struct MemberFunctionIrOwner {
    int value;
};

int invoke_member_dot(
    MemberFunctionIrOwner& object,
    int (MemberFunctionIrOwner::*method)(int), int value) {
    return (object.*method)(value);
}

int invoke_member_arrow(
    MemberFunctionIrOwner* object,
    int (MemberFunctionIrOwner::*method)(int) const, int value) {
    return (object->*method)(value);
}

int invoke_member_volatile(
    volatile MemberFunctionIrOwner& object,
    int (MemberFunctionIrOwner::*method)(int) volatile, int value) {
    return (object.*method)(value);
}

int invoke_member_cv(
    const volatile MemberFunctionIrOwner& object,
    int (MemberFunctionIrOwner::*method)(int) const volatile, int value) {
    return (object.*method)(value);
}
