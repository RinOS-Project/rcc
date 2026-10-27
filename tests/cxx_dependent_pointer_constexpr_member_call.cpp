constexpr int dependent_pointer_constexpr_target = 17;
constexpr int dependent_pointer_constexpr_values[] = {5, 11};

struct DependentPointerBase {
    int value;

    constexpr int add(int amount) const {
        return value + amount;
    }
};

struct DependentConstexprVirtualBase {
    int value;

    constexpr explicit DependentConstexprVirtualBase(int initial)
        : value(initial) {}

    constexpr virtual int add(int amount) const {
        return value + amount;
    }
};

struct DependentConstexprVirtualDerived : DependentConstexprVirtualBase {
    constexpr explicit DependentConstexprVirtualDerived(int initial)
        : DependentConstexprVirtualBase(initial) {}

    constexpr int add(int amount) const override {
        return value + amount + 100;
    }
};

constexpr int DependentPointerBase::* dependent_pointer_constexpr_member =
    &DependentPointerBase::value;
constexpr int (DependentPointerBase::*
    dependent_pointer_constexpr_member_function)(int) const =
        &DependentPointerBase::add;
thread_local int (DependentPointerBase::*
    dependent_pointer_tls_member_function)(int) const =
        &DependentPointerBase::add;
constexpr DependentPointerBase
    dependent_member_function_constexpr_object{17};
constexpr DependentConstexprVirtualDerived
    dependent_constexpr_virtual_object{17};
constexpr int (DependentConstexprVirtualBase::*
    dependent_constexpr_virtual_member_function)(int) const =
        &DependentConstexprVirtualBase::add;
struct DependentPointerDerived : DependentPointerBase {
    int tail;
};

struct DependentPointerPaddingBase {
    int padding;
};

struct DependentPointerMultipleDerived : DependentPointerPaddingBase,
                                        DependentPointerBase {
    int tail;
};

struct DependentPointerNestedMiddle : DependentPointerPaddingBase,
                                     DependentPointerBase {
    int middle_tail;
};

struct DependentPointerNestedPaddingBase {
    int padding;
};

struct DependentPointerNestedDerived : DependentPointerNestedPaddingBase,
                                       DependentPointerNestedMiddle {
    int tail;
};

constexpr DependentPointerDerived dependent_pointer_constexpr_derived = {
    {29}, 31};
constexpr DependentPointerMultipleDerived
    dependent_pointer_constexpr_multiple_derived = {{13}, {41}, 43};
constexpr DependentPointerBase dependent_member_pointer_constexpr_object = {
    43};

template<int Value, typename Owner, typename... Tail>
struct DependentPointerConstexprBox {};

struct DependentPointerConstexprOwner {
    static constexpr int value(const int* pointer) {
        return pointer ? 17 : 0;
    }

    static constexpr int value_void(const void* pointer) {
        return pointer ? 23 : 0;
    }

    static constexpr int value_base(const DependentPointerBase* pointer) {
        return pointer ? 37 : 0;
    }

    static constexpr int is_null_member_pointer(
        int DependentPointerBase::* member) {
        return member == nullptr ? 1 : 0;
    }

    static constexpr int read_member_pointer(
        int DependentPointerBase::* member,
        DependentPointerBase object) {
        int DependentPointerBase::* selected = member;
        return object.*selected;
    }

    static constexpr int read_derived_member_pointer(
        int DependentPointerDerived::* member,
        DependentPointerDerived object) {
        int DependentPointerDerived::* selected = member;
        return object.*selected;
    }

    static constexpr int read_multiple_derived_member_pointer(
        int DependentPointerMultipleDerived::* member,
        DependentPointerMultipleDerived object) {
        int DependentPointerMultipleDerived::* selected = member;
        return object.*selected;
    }

    static constexpr int read_const_multiple_derived_member_pointer(
        const int DependentPointerMultipleDerived::* member,
        DependentPointerMultipleDerived object) {
        const int DependentPointerMultipleDerived::* selected = member;
        return object.*selected;
    }

    static constexpr int read_const_nested_derived_member_pointer(
        const int DependentPointerNestedDerived::* member,
        DependentPointerNestedDerived object) {
        const int DependentPointerNestedDerived::* selected = member;
        return object.*selected;
    }

    static constexpr int read_const_member_pointer(
        const int DependentPointerBase::* member,
        DependentPointerBase object) {
        const int DependentPointerBase::* selected = member;
        return object.*selected;
    }

    static constexpr int has_member_function_pointer(
        int (DependentPointerBase::* member)(int) const) {
        int (DependentPointerBase::* selected)(int) const = member;
        return selected != nullptr ? 419 : 420;
    }

    static constexpr int logical_and(const int* pointer) {
        return pointer && 1;
    }

    static constexpr int logical_or(const int* pointer) {
        return pointer || 0;
    }

    static constexpr int read(const int* pointer) {
        return pointer ? *pointer : 0;
    }

    static constexpr int read_reference(const int* const& pointer) {
        return pointer ? *pointer : 0;
    }

    static constexpr int read_next(const int* pointer) {
        return pointer ? *(pointer + 1) : 0;
    }
};

constexpr int invoke_member_function_pointer(
    int (DependentPointerBase::* member)(int) const,
    DependentPointerBase object, int amount) {
    return (object.*member)(amount);
}

constexpr int invoke_member_function_pointer_arrow(
    int (DependentPointerBase::* member)(int) const,
    const DependentPointerBase* object, int amount) {
    return (object->*member)(amount);
}

constexpr int invoke_member_function_pointer_arrow_from_local(int amount) {
    DependentPointerBase object{17};
    return invoke_member_function_pointer_arrow(
        &DependentPointerBase::add, &object, amount);
}

constexpr int invoke_virtual_member_function_pointer_arrow(
    int (DependentConstexprVirtualBase::* member)(int) const,
    const DependentConstexprVirtualBase* object, int amount) {
    return (object->*member)(amount);
}

constexpr int invoke_virtual_member_function_pointer_arrow_from_local(
    int amount) {
    constexpr DependentConstexprVirtualDerived object{17};
    return invoke_virtual_member_function_pointer_arrow(
        ::dependent_constexpr_virtual_member_function, &object, amount);
}

static_assert(DependentPointerConstexprOwner::value(
    &::dependent_pointer_constexpr_target) == 17);
static_assert(DependentPointerConstexprOwner::value_void(
    &::dependent_pointer_constexpr_target) == 23);
static_assert(DependentPointerConstexprOwner::value_base(
    &::dependent_pointer_constexpr_derived) == 37);
static_assert(DependentPointerConstexprOwner::is_null_member_pointer(
    nullptr) == 1);
static_assert(DependentPointerConstexprOwner::is_null_member_pointer(
    &DependentPointerBase::value) == 0);
static_assert(DependentPointerConstexprOwner::read_member_pointer(
    ::dependent_pointer_constexpr_member,
    ::dependent_member_pointer_constexpr_object) == 43);
static_assert(DependentPointerConstexprOwner::read_const_member_pointer(
    ::dependent_pointer_constexpr_member,
    ::dependent_member_pointer_constexpr_object) == 43);
static_assert(DependentPointerConstexprOwner::has_member_function_pointer(
    &DependentPointerBase::add) == 419);
static_assert(DependentPointerConstexprOwner::has_member_function_pointer(
    nullptr) == 420);
static_assert(DependentPointerConstexprOwner::has_member_function_pointer(
    ::dependent_pointer_constexpr_member_function) == 419);
static_assert(invoke_member_function_pointer(
    &DependentPointerBase::add, DependentPointerBase{17}, 25) == 42);
static_assert(invoke_member_function_pointer(
    ::dependent_pointer_constexpr_member_function,
    DependentPointerBase{17}, 25) == 42);
static_assert(invoke_member_function_pointer_arrow(
    &DependentPointerBase::add,
    &dependent_member_function_constexpr_object, 25) == 42);
static_assert(invoke_member_function_pointer_arrow_from_local(25) == 42);
static_assert(invoke_virtual_member_function_pointer_arrow(
    ::dependent_constexpr_virtual_member_function,
    &::dependent_constexpr_virtual_object, 25) == 142);
static_assert(invoke_virtual_member_function_pointer_arrow_from_local(25) ==
              142);
template<typename Owner>
using DependentMemberFunctionPointerCall = DependentPointerConstexprBox<
    Owner::has_member_function_pointer(&DependentPointerBase::add), Owner>;
static_assert(sizeof(DependentMemberFunctionPointerCall<
                  DependentPointerConstexprOwner>) ==
              sizeof(DependentPointerConstexprBox<
                  419, DependentPointerConstexprOwner>));
static_assert(DependentPointerConstexprOwner::value(nullptr) == 0);
static_assert(DependentPointerConstexprOwner::logical_and(
    &::dependent_pointer_constexpr_target) == 1);
static_assert(DependentPointerConstexprOwner::logical_and(nullptr) == 0);
static_assert(DependentPointerConstexprOwner::logical_or(
    &::dependent_pointer_constexpr_target) == 1);
static_assert(DependentPointerConstexprOwner::logical_or(nullptr) == 0);
static_assert(DependentPointerConstexprOwner::read(
    &::dependent_pointer_constexpr_values[1]) == 11);
static_assert(DependentPointerConstexprOwner::read_reference(
    &::dependent_pointer_constexpr_values[1]) == 11);
static_assert(DependentPointerConstexprOwner::read_next(
    &::dependent_pointer_constexpr_values[0]) == 11);

template<typename Owner, typename... Tail>
int select_dependent_pointer_constexpr_call(
    DependentPointerConstexprBox<
        Owner::value(&::dependent_pointer_constexpr_target), Owner, Tail...>) {
    return 301;
}

template<typename Owner, typename... Tail>
int select_dependent_pointer_constexpr_call(
    DependentPointerConstexprBox<1, Owner, Tail...>) {
    return 302;
}

template<typename Owner, typename... Tail>
int select_dependent_void_pointer_conversion_call(
    DependentPointerConstexprBox<
        Owner::value_void(&::dependent_pointer_constexpr_target),
        Owner, Tail...>) {
    return 305;
}

template<typename Owner, typename... Tail>
int select_dependent_void_pointer_conversion_call(
    DependentPointerConstexprBox<1, Owner, Tail...>) {
    return 306;
}

template<typename Owner, typename... Tail>
int select_dependent_derived_pointer_conversion_call(
    DependentPointerConstexprBox<
        Owner::value_base(&::dependent_pointer_constexpr_derived),
        Owner, Tail...>) {
    return 307;
}

template<typename Owner, typename... Tail>
int select_dependent_derived_pointer_conversion_call(
    DependentPointerConstexprBox<1, Owner, Tail...>) {
    return 308;
}

template<typename Owner, typename... Tail>
int select_dependent_null_member_pointer_call(
    DependentPointerConstexprBox<
        Owner::is_null_member_pointer(nullptr), Owner, Tail...>) {
    return 351;
}

template<typename Owner, typename... Tail>
int select_dependent_null_member_pointer_call(
    DependentPointerConstexprBox<0, Owner, Tail...>) {
    return 352;
}

template<typename Owner, typename... Tail>
int select_dependent_member_pointer_application_call(
    DependentPointerConstexprBox<
        Owner::read_const_member_pointer(
            ::dependent_pointer_constexpr_member,
            ::dependent_member_pointer_constexpr_object),
        Owner, Tail...>) {
    return 361;
}

template<typename Owner, typename... Tail>
int select_dependent_member_pointer_application_call(
    DependentPointerConstexprBox<0, Owner, Tail...>) {
    return 362;
}

template<typename Owner, typename... Tail>
int select_dependent_member_pointer_owner_conversion_call(
    DependentPointerConstexprBox<
        Owner::read_derived_member_pointer(
            ::dependent_pointer_constexpr_member,
            DependentPointerDerived{{29}, 31}),
        Owner, Tail...>) {
    return 371;
}

template<typename Owner, typename... Tail>
int select_dependent_member_pointer_owner_conversion_call(
    DependentPointerConstexprBox<0, Owner, Tail...>) {
    return 372;
}

template<typename Owner, typename... Tail>
int select_dependent_member_pointer_multiple_owner_conversion_call(
    DependentPointerConstexprBox<
        Owner::read_multiple_derived_member_pointer(
            ::dependent_pointer_constexpr_member,
            DependentPointerMultipleDerived{{13}, {41}, 43}),
        Owner, Tail...>) {
    return 381;
}

template<typename Owner, typename... Tail>
int select_dependent_member_pointer_multiple_owner_conversion_call(
    DependentPointerConstexprBox<0, Owner, Tail...>) {
    return 382;
}

template<typename Owner, typename... Tail>
int select_dependent_member_pointer_const_multiple_owner_conversion_call(
    DependentPointerConstexprBox<
        Owner::read_const_multiple_derived_member_pointer(
            ::dependent_pointer_constexpr_member,
            DependentPointerMultipleDerived{{13}, {41}, 43}),
        Owner, Tail...>) {
    return 391;
}

template<typename Owner, typename... Tail>
int select_dependent_member_pointer_const_multiple_owner_conversion_call(
    DependentPointerConstexprBox<0, Owner, Tail...>) {
    return 392;
}

template<typename Owner, typename... Tail>
int select_dependent_member_pointer_const_nested_owner_conversion_call(
    DependentPointerConstexprBox<
        Owner::read_const_nested_derived_member_pointer(
            ::dependent_pointer_constexpr_member,
            DependentPointerNestedDerived{{7}, {{13}, {41}, 42}, 43}),
        Owner, Tail...>) {
    return 401;
}

template<typename Owner, typename... Tail>
int select_dependent_member_pointer_const_nested_owner_conversion_call(
    DependentPointerConstexprBox<0, Owner, Tail...>) {
    return 402;
}

template<typename Owner, typename... Tail>
int select_dependent_array_element_pointer_call(
    DependentPointerConstexprBox<
        Owner::value(&::dependent_pointer_constexpr_values[1]),
        Owner, Tail...>) {
    return 311;
}

template<typename Owner, typename... Tail>
int select_dependent_array_element_pointer_call(
    DependentPointerConstexprBox<1, Owner, Tail...>) {
    return 312;
}

template<typename Owner, typename... Tail>
int select_dependent_pointer_dereference_call(
    DependentPointerConstexprBox<
        Owner::read(&::dependent_pointer_constexpr_values[1]),
        Owner, Tail...>) {
    return 321;
}

template<typename Owner, typename... Tail>
int select_dependent_pointer_dereference_call(
    DependentPointerConstexprBox<1, Owner, Tail...>) {
    return 322;
}

template<typename Owner, typename... Tail>
int select_dependent_pointer_reference_call(
    DependentPointerConstexprBox<
        Owner::read_reference(&::dependent_pointer_constexpr_values[1]),
        Owner, Tail...>) {
    return 331;
}

template<typename Owner, typename... Tail>
int select_dependent_pointer_reference_call(
    DependentPointerConstexprBox<1, Owner, Tail...>) {
    return 332;
}

template<typename Owner, typename... Tail>
int select_dependent_pointer_arithmetic_call(
    DependentPointerConstexprBox<
        Owner::read_next(&::dependent_pointer_constexpr_values[0]),
        Owner, Tail...>) {
    return 341;
}

template<typename Owner, typename... Tail>
int select_dependent_pointer_arithmetic_call(
    DependentPointerConstexprBox<1, Owner, Tail...>) {
    return 342;
}

template<typename Owner, typename... Tail>
int select_dependent_nullptr_constexpr_call(
    DependentPointerConstexprBox<Owner::value(nullptr), Owner, Tail...>) {
    return 303;
}

template<typename Owner, typename... Tail>
int select_dependent_nullptr_constexpr_call(
    DependentPointerConstexprBox<1, Owner, Tail...>) {
    return 304;
}

int main() {
    DependentPointerBase member_function_pointer_object{17};
    if ((member_function_pointer_object.*
         dependent_pointer_constexpr_member_function)(25) != 42) {
        return 15;
    }
    if ((member_function_pointer_object.*
         dependent_pointer_tls_member_function)(25) != 42) {
        return 16;
    }
    if (select_dependent_pointer_constexpr_call(
            DependentPointerConstexprBox<
                17, DependentPointerConstexprOwner, int>{}) != 301) {
        return 1;
    }
    if (select_dependent_void_pointer_conversion_call(
            DependentPointerConstexprBox<
                23, DependentPointerConstexprOwner, int>{}) != 305) {
        return 2;
    }
    if (select_dependent_derived_pointer_conversion_call(
            DependentPointerConstexprBox<
                37, DependentPointerConstexprOwner, int>{}) != 307) {
        return 3;
    }
    if (select_dependent_null_member_pointer_call(
            DependentPointerConstexprBox<
                1, DependentPointerConstexprOwner, int>{}) != 351) {
        return 4;
    }
    if (select_dependent_member_pointer_application_call(
            DependentPointerConstexprBox<
                43, DependentPointerConstexprOwner, int>{}) != 361) {
        return 10;
    }
    if (select_dependent_member_pointer_owner_conversion_call(
            DependentPointerConstexprBox<
                29, DependentPointerConstexprOwner, int>{}) != 371) {
        return 11;
    }
    if (select_dependent_member_pointer_multiple_owner_conversion_call(
            DependentPointerConstexprBox<
                41, DependentPointerConstexprOwner, int>{}) != 381) {
        return 12;
    }
    if (select_dependent_member_pointer_const_multiple_owner_conversion_call(
            DependentPointerConstexprBox<
                41, DependentPointerConstexprOwner, int>{}) != 391) {
        return 13;
    }
    if (select_dependent_member_pointer_const_nested_owner_conversion_call(
            DependentPointerConstexprBox<
                41, DependentPointerConstexprOwner, int>{}) != 401) {
        return 14;
    }
    if (select_dependent_array_element_pointer_call(
            DependentPointerConstexprBox<
                17, DependentPointerConstexprOwner, int>{}) != 311) {
        return 5;
    }
    if (select_dependent_pointer_dereference_call(
            DependentPointerConstexprBox<
                11, DependentPointerConstexprOwner, int>{}) != 321) {
        return 6;
    }
    if (select_dependent_pointer_reference_call(
            DependentPointerConstexprBox<
                11, DependentPointerConstexprOwner, int>{}) != 331) {
        return 7;
    }
    if (select_dependent_pointer_arithmetic_call(
            DependentPointerConstexprBox<
                11, DependentPointerConstexprOwner, int>{}) != 341) {
        return 8;
    }
    if (select_dependent_nullptr_constexpr_call(
            DependentPointerConstexprBox<
                0, DependentPointerConstexprOwner, int>{}) != 303) {
        return 9;
    }
    return 0;
}
