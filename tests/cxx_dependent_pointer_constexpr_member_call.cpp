constexpr int dependent_pointer_constexpr_target = 17;
constexpr int dependent_pointer_constexpr_values[] = {5, 11};

template<int Value, typename Owner, typename... Tail>
struct DependentPointerConstexprBox {};

struct DependentPointerConstexprOwner {
    static constexpr int value(const int* pointer) {
        return pointer ? 17 : 0;
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
};

static_assert(DependentPointerConstexprOwner::value(
    &::dependent_pointer_constexpr_target) == 17);
static_assert(DependentPointerConstexprOwner::value(nullptr) == 0);
static_assert(DependentPointerConstexprOwner::logical_and(
    &::dependent_pointer_constexpr_target) == 1);
static_assert(DependentPointerConstexprOwner::logical_and(nullptr) == 0);
static_assert(DependentPointerConstexprOwner::logical_or(
    &::dependent_pointer_constexpr_target) == 1);
static_assert(DependentPointerConstexprOwner::logical_or(nullptr) == 0);
static_assert(DependentPointerConstexprOwner::read(
    &::dependent_pointer_constexpr_values[1]) == 11);

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
        Owner::read(&::dependent_pointer_constexpr_target),
        Owner, Tail...>) {
    return 321;
}

template<typename Owner, typename... Tail>
int select_dependent_pointer_dereference_call(
    DependentPointerConstexprBox<1, Owner, Tail...>) {
    return 322;
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
    if (select_dependent_array_element_pointer_call(
            DependentPointerConstexprBox<
                17, DependentPointerConstexprOwner, int>{}) != 311) {
        return 2;
    }
    if (select_dependent_pointer_dereference_call(
            DependentPointerConstexprBox<
                17, DependentPointerConstexprOwner, int>{}) != 321) {
        return 3;
    }
    if (select_dependent_nullptr_constexpr_call(
            DependentPointerConstexprBox<
                0, DependentPointerConstexprOwner, int>{}) != 303) {
        return 4;
    }
    return 0;
}
