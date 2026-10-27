constexpr int dependent_pointer_constexpr_target = 17;
constexpr int dependent_pointer_constexpr_values[] = {5, 11};

struct DependentPointerBase {
    int value;
};

constexpr int DependentPointerBase::* dependent_pointer_constexpr_member =
    &DependentPointerBase::value;

struct DependentPointerDerived : DependentPointerBase {
    int tail;
};

constexpr DependentPointerDerived dependent_pointer_constexpr_derived = {
    {29}, 31};
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

    static constexpr int read_const_member_pointer(
        const int DependentPointerBase::* member,
        DependentPointerBase object) {
        const int DependentPointerBase::* selected = member;
        return object.*selected;
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
