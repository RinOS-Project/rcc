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

class PrivateMemberFunctionPointerOwner {
    int value;

    int add(int amount) {
        return value + amount;
    }

    friend int invoke_private_member_function_pointer(
        PrivateMemberFunctionPointerOwner& object, int amount);

public:
    void set_value(int input) {
        value = input;
    }
};

int invoke_private_member_function_pointer(
    PrivateMemberFunctionPointerOwner& object, int amount) {
    int (PrivateMemberFunctionPointerOwner::*method)(int) =
        &PrivateMemberFunctionPointerOwner::add;
    return (object.*method)(amount);
}

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

struct VirtualMemberFunctionBase {
    int value;

    virtual int add(int amount) {
        return value + amount;
    }
};

struct VirtualMemberFunctionDerived : VirtualMemberFunctionBase {
    int add(int amount) {
        return value + amount + 100;
    }
};

struct VirtualMemberFunctionPathBase {
    int value;

    int add(int amount) {
        return value + amount;
    }
};

struct VirtualMemberFunctionPathDerived
    : virtual VirtualMemberFunctionPathBase {};

struct MemberFunctionAdjustmentPrefix {
    int prefix;
};

struct MemberFunctionAdjustmentBase {
    int value;

    int add(int amount) {
        return value + amount;
    }

    int select(int amount) {
        return value + amount + 10;
    }

    int select(double amount) {
        return value + static_cast<int>(amount) + 20;
    }
};

struct MemberFunctionAdjustmentDerived
    : MemberFunctionAdjustmentPrefix, MemberFunctionAdjustmentBase {};

struct RefNoexceptMemberFunctionOwner {
    int value;

    int read() & noexcept {
        return value + 10;
    }

    int read() && noexcept {
        return value + 20;
    }

    int quiet(int amount) noexcept {
        return value + amount + 30;
    }

    int throwing(int amount) {
        return value + amount + 40;
    }
};

typedef int (MemberFunctionOwner::*MemberFunctionMethod)(int);

MemberFunctionMethod global_member_function = &MemberFunctionOwner::add;

MemberFunctionMethod identity_member_function(MemberFunctionMethod method) {
    return method;
}

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

int invoke_virtual_member(
    VirtualMemberFunctionBase& object,
    int (VirtualMemberFunctionBase::*method)(int), int value) {
    return (object.*method)(value);
}

int invoke_lvalue_qualified_member(
    RefNoexceptMemberFunctionOwner& object,
    int (RefNoexceptMemberFunctionOwner::*method)() & noexcept) {
    return (object.*method)();
}

int invoke_rvalue_qualified_member(
    RefNoexceptMemberFunctionOwner&& object,
    int (RefNoexceptMemberFunctionOwner::*method)() && noexcept) {
    return (static_cast<RefNoexceptMemberFunctionOwner&&>(object).*method)();
}

int invoke_noexcept_member(
    RefNoexceptMemberFunctionOwner& object,
    int (RefNoexceptMemberFunctionOwner::*method)(int) noexcept,
    int value) {
    return (object.*method)(value);
}

int invoke_throwing_member(
    RefNoexceptMemberFunctionOwner& object,
    int (RefNoexceptMemberFunctionOwner::*method)(int), int value) {
    return (object.*method)(value);
}

int main() {
    MemberFunctionOwner object;
    PrivateMemberFunctionPointerOwner private_pointer_object;
    InlineMemberFunctionPointerOwner inline_pointer_object;
    InlineLaterStaticFunctionPointerOwner inline_static_pointer_object;
    const MemberFunctionOwner const_object = {11};
    volatile MemberFunctionOwner volatile_object = {13};
    const volatile MemberFunctionOwner cv_object = {17};
    int (MemberFunctionOwner::*method)(int) = nullptr;
    int (MemberFunctionOwner::*const_method)(int) const;
    int (MemberFunctionOwner::*double_method)(double) =
        &MemberFunctionOwner::add;
    int (MemberFunctionOwner::*volatile_method)(int) volatile;
    int (MemberFunctionOwner::*cv_method)(int) const volatile;
    InheritedMemberFunctionDerived inherited_object;
    int (InheritedMemberFunctionBase::*inherited_method)(int);
    int (MemberFunctionOwner::*null_method)(int) = nullptr;
    VirtualMemberFunctionDerived virtual_object;
    int (VirtualMemberFunctionBase::*virtual_method)(int) =
        &VirtualMemberFunctionBase::add;
    VirtualMemberFunctionPathDerived virtual_path_object;
    int (VirtualMemberFunctionPathBase::*virtual_path_method)(int) =
        &VirtualMemberFunctionPathBase::add;
    MemberFunctionAdjustmentDerived adjusted_object;
    int (MemberFunctionAdjustmentBase::*base_method)(int) =
        &MemberFunctionAdjustmentBase::add;
    int (MemberFunctionAdjustmentDerived::*derived_method)(int) = base_method;
    int (MemberFunctionAdjustmentDerived::*derived_overload)(int) =
        &MemberFunctionAdjustmentDerived::select;
    int (MemberFunctionAdjustmentBase::*null_base_method)(int) = nullptr;
    int (MemberFunctionAdjustmentDerived::*null_derived_method)(int) =
        null_base_method;
    int (MemberFunctionAdjustmentBase::*round_trip_owner_method)(int) =
        static_cast<int (MemberFunctionAdjustmentBase::*)(int)>(derived_method);
    MemberFunctionAdjustmentBase& adjusted_base = adjusted_object;
    RefNoexceptMemberFunctionOwner qualified_object;
    int (RefNoexceptMemberFunctionOwner::*lvalue_method)() & noexcept =
        &RefNoexceptMemberFunctionOwner::read;
    int (RefNoexceptMemberFunctionOwner::*rvalue_method)() && noexcept =
        &RefNoexceptMemberFunctionOwner::read;
    int (RefNoexceptMemberFunctionOwner::*noexcept_method)(int) noexcept =
        &RefNoexceptMemberFunctionOwner::quiet;
    int (RefNoexceptMemberFunctionOwner::*widened_noexcept_method)(int) =
        noexcept_method;
    int (RefNoexceptMemberFunctionOwner::*throwing_method)(int) =
        &RefNoexceptMemberFunctionOwner::throwing;

    object.value = 7;
    if (sizeof(MemberFunctionMethod) != 2 * sizeof(void*)) return 14;
    if (null_method != nullptr) return 15;
    if (null_method == global_member_function) return 28;
    if (method != nullptr) return 16;
    inline_pointer_object.set_value(19);
    if (inline_pointer_object.invoke(8) != 27) return 12;
    if (inline_static_pointer_object.invoke(8) != 39) return 13;
    method = &MemberFunctionOwner::add;
    method = identity_member_function(method);
    if (method != identity_member_function(global_member_function)) return 17;
    if (identity_member_function(nullptr) != nullptr) return 18;
    private_pointer_object.set_value(35);
    if (invoke_private_member_function_pointer(private_pointer_object, 6) != 41) {
        return 33;
    }
    if ((object.*global_member_function)(2) != 9) return 19;
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
    virtual_object.value = 30;
    if (invoke_virtual_member(virtual_object, virtual_method, 7) != 137) {
        return 20;
    }
    virtual_path_object.value = 63;
    if ((virtual_path_object.*virtual_path_method)(4) != 67) return 32;
    adjusted_object.prefix = 3;
    adjusted_object.value = 41;
    if ((adjusted_object.*derived_method)(1) != 42) return 21;
    if ((adjusted_object.*derived_overload)(2) != 53) return 22;
    if ((adjusted_base.*round_trip_owner_method)(2) != 43) return 23;
    if (null_derived_method != nullptr) return 30;
    if (round_trip_owner_method != base_method) return 31;
    qualified_object.value = 50;
    if (invoke_lvalue_qualified_member(qualified_object, lvalue_method) != 60) {
        return 24;
    }
    if (invoke_rvalue_qualified_member(
            static_cast<RefNoexceptMemberFunctionOwner&&>(qualified_object),
            rvalue_method) != 70) {
        return 25;
    }
    if (invoke_noexcept_member(qualified_object, noexcept_method, 2) != 82) {
        return 26;
    }
    if (invoke_throwing_member(
            qualified_object, widened_noexcept_method, 2) != 82) {
        return 29;
    }
    if (invoke_throwing_member(qualified_object, throwing_method, 3) != 93) {
        return 27;
    }
    return 0;
}
