struct MemberFunctionIrOwner {
    int value;

    int add(int amount) { return value + amount; }
    int add(int amount) const { return value + amount + 100; }
    int add(double amount) { return value + static_cast<int>(amount); }
};

typedef int (MemberFunctionIrOwner::*MemberFunctionIrMethod)(int);

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

int invoke_member_overloaded_argument(MemberFunctionIrOwner& object,
                                      int value) {
    return invoke_member_dot(object, &MemberFunctionIrOwner::add, value);
}

int invoke_member_overloaded_local(MemberFunctionIrOwner& object,
                                   int value) {
    int (MemberFunctionIrOwner::*method)(int) =
        &MemberFunctionIrOwner::add;
    return (object.*method)(value);
}

MemberFunctionIrMethod identity_member_function_ir(
    MemberFunctionIrMethod method) {
    return method;
}

int invoke_returned_member_function_ir(MemberFunctionIrOwner& object,
                                       MemberFunctionIrMethod method,
                                       int value) {
    return (object.*identity_member_function_ir(method))(value);
}

bool null_member_function_round_trip_ir() {
    return identity_member_function_ir(nullptr) == nullptr;
}
