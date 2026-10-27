template<int Value, typename Owner, typename... Tail>
struct DependentPointerConstexprBox {};

struct DependentPointerConstexprOwner {
    static constexpr int value(const int* pointer) {
        return pointer ? *pointer : 0;
    }
};

static_assert(DependentPointerConstexprOwner::value(
    static_cast<const int*>(nullptr)) == 0);
static_assert(DependentPointerConstexprOwner::value(nullptr) == 0);

template<typename Owner, typename... Tail>
int select_dependent_pointer_constexpr_call(
    DependentPointerConstexprBox<
        Owner::value(static_cast<const int*>(nullptr)), Owner, Tail...>) {
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
                0, DependentPointerConstexprOwner, int>{}) != 301) {
        return 1;
    }
    if (select_dependent_nullptr_constexpr_call(
            DependentPointerConstexprBox<
                0, DependentPointerConstexprOwner, int>{}) != 303) {
        return 2;
    }
    return 0;
}
