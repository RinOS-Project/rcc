struct MemberFunctionOwner {
    int value;

    int add(int amount) {
        return value + amount;
    }

    int scale(int factor) {
        return value * factor;
    }

    int add_const(int amount) const {
        return value + amount;
    }

    int add_volatile(int amount) volatile {
        return value + amount;
    }

    int add_cv(int amount) const volatile {
        return value + amount;
    }
};

int invoke_dot(MemberFunctionOwner& object,
               int (MemberFunctionOwner::*method)(int), int value) {
    return (object.*method)(value);
}

int invoke_arrow(MemberFunctionOwner* object,
                 int (MemberFunctionOwner::*method)(int), int value) {
    return (object->*method)(value);
}

int invoke_const(const MemberFunctionOwner& object,
                 int (MemberFunctionOwner::*method)(int) const, int value) {
    return (object.*method)(value);
}

int invoke_volatile(volatile MemberFunctionOwner& object,
                    int (MemberFunctionOwner::*method)(int) volatile,
                    int value) {
    return (object.*method)(value);
}

int invoke_cv(const volatile MemberFunctionOwner& object,
              int (MemberFunctionOwner::*method)(int) const volatile,
              int value) {
    return (object.*method)(value);
}

int main() {
    MemberFunctionOwner object;
    const MemberFunctionOwner const_object = {11};
    volatile MemberFunctionOwner volatile_object = {13};
    const volatile MemberFunctionOwner cv_object = {17};
    int (MemberFunctionOwner::*method)(int);
    int (MemberFunctionOwner::*const_method)(int) const;
    int (MemberFunctionOwner::*volatile_method)(int) volatile;
    int (MemberFunctionOwner::*cv_method)(int) const volatile;

    object.value = 7;
    method = &MemberFunctionOwner::add;
    if (invoke_dot(object, method, 5) != 12) return 1;
    if (invoke_arrow(&object, method, 9) != 16) return 2;

    method = &MemberFunctionOwner::scale;
    if (invoke_dot(object, method, 3) != 21) return 3;
    if (invoke_arrow(&object, method, 4) != 28) return 4;

    const_method = &MemberFunctionOwner::add_const;
    if (invoke_const(const_object, const_method, 5) != 16) return 5;
    if (invoke_const(object, const_method, 6) != 13) return 6;
    volatile_method = &MemberFunctionOwner::add_volatile;
    if (invoke_volatile(volatile_object, volatile_method, 7) != 20) return 7;
    cv_method = &MemberFunctionOwner::add_cv;
    if (invoke_cv(cv_object, cv_method, 9) != 26) return 8;
    return 0;
}
