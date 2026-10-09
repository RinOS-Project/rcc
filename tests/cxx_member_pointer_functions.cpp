struct MemberFunctionOwner {
    int value;

    int add(int amount) {
        return value + amount;
    }

    int add(int amount) const {
        return value + amount + 100;
    }

    int add(double amount) {
        return value + static_cast<int>(amount);
    }

    int scale(int factor) {
        return value * factor;
    }

    int add_volatile(int amount) volatile {
        return value + amount;
    }

    int add_cv(int amount) const volatile {
        return value + amount;
    }
};

class InlineMemberFunctionPointerOwner {
    int value;

    int add(int amount) {
        return value + amount;
    }

public:
    void set_value(int input) {
        value = input;
    }

    int invoke(int amount) {
        auto method = &InlineMemberFunctionPointerOwner::add;
        return (this->*method)(amount);
    }
};

class InlineLaterStaticFunctionPointerOwner {
public:
    int invoke(int value) {
        auto function = &InlineLaterStaticFunctionPointerOwner::transform;
        return function(value);
    }

    static int transform(int value) {
        return value + 31;
    }
};

struct InheritedMemberFunctionBase {
    int value;

    int add(int amount) {
        return value + amount;
    }
};

struct InheritedMemberFunctionDerived : InheritedMemberFunctionBase {};

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

int invoke_double_overload(MemberFunctionOwner& object,
                           int (MemberFunctionOwner::*method)(double),
                           double value) {
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

int invoke_inherited_member(
    InheritedMemberFunctionDerived& object,
    int (InheritedMemberFunctionBase::*method)(int), int value) {
    return (object.*method)(value);
}

int main() {
    MemberFunctionOwner object;
    InlineMemberFunctionPointerOwner inline_pointer_object;
    InlineLaterStaticFunctionPointerOwner inline_static_pointer_object;
    const MemberFunctionOwner const_object = {11};
    volatile MemberFunctionOwner volatile_object = {13};
    const volatile MemberFunctionOwner cv_object = {17};
    int (MemberFunctionOwner::*method)(int);
    int (MemberFunctionOwner::*const_method)(int) const;
    int (MemberFunctionOwner::*double_method)(double) =
        &MemberFunctionOwner::add;
    int (MemberFunctionOwner::*volatile_method)(int) volatile;
    int (MemberFunctionOwner::*cv_method)(int) const volatile;
    InheritedMemberFunctionDerived inherited_object;
    int (InheritedMemberFunctionBase::*inherited_method)(int);

    object.value = 7;
    inline_pointer_object.set_value(19);
    if (inline_pointer_object.invoke(8) != 27) return 12;
    if (inline_static_pointer_object.invoke(8) != 39) return 13;
    method = &MemberFunctionOwner::add;
    if (invoke_dot(object, method, 5) != 12) return 1;
    if (invoke_arrow(&object, method, 9) != 16) return 2;

    method = &MemberFunctionOwner::scale;
    if (invoke_dot(object, method, 3) != 21) return 3;
    if (invoke_arrow(&object, method, 4) != 28) return 4;

    const_method = &MemberFunctionOwner::add;
    if (invoke_const(const_object, const_method, 5) != 116) return 5;
    if (invoke_const(object, const_method, 6) != 113) return 6;
    if (invoke_double_overload(object, double_method, 2.5) != 9) return 10;
    if (invoke_double_overload(object, &MemberFunctionOwner::add, 3.5) != 10) {
        return 11;
    }
    volatile_method = &MemberFunctionOwner::add_volatile;
    if (invoke_volatile(volatile_object, volatile_method, 7) != 20) return 7;
    cv_method = &MemberFunctionOwner::add_cv;
    if (invoke_cv(cv_object, cv_method, 9) != 26) return 8;
    inherited_object.value = 23;
    inherited_method = &InheritedMemberFunctionDerived::add;
    if (invoke_inherited_member(inherited_object, inherited_method, 5) != 28) {
        return 9;
    }
    return 0;
}
