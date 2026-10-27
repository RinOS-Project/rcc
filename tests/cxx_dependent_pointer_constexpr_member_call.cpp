constexpr int dependent_pointer_constexpr_target = 17;

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
    if (select_dependent_nullptr_constexpr_call(
            DependentPointerConstexprBox<
                0, DependentPointerConstexprOwner, int>{}) != 303) {
        return 2;
    }
    return 0;
}
