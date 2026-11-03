template<int N> requires (N > 0 && N < 10)
int constrained_add(int value) {
    return value + N;
}

template<int N, int M = N * 2> requires (M >= N)
int constrained_scale(int value) {
    return value + M;
}

struct ConstraintNoDefault {
    ConstraintNoDefault() = delete;
    explicit ConstraintNoDefault(int) {}
};

template<class T>
struct ConstrainedMemberOverloads {
    int select(int value) requires (__is_constructible(T)) {
        return value + 3;
    }
    int select(long value) requires (!__is_constructible(T)) {
        return static_cast<int>(value) + 9;
    }
    static int select_static(int value) requires (__is_constructible(T)) {
        return value + 5;
    }
    static int select_static(long value) requires (!__is_constructible(T)) {
        return static_cast<int>(value) + 11;
    }
};

template<class T>
struct ConstrainedMemberConstructors {
    int value;

    ConstrainedMemberConstructors() requires (__is_constructible(T))
        : value(5) {}
    explicit ConstrainedMemberConstructors(int initial)
        requires (!__is_constructible(T))
        : value(initial) {}
};

static_assert(__is_constructible(ConstrainedMemberConstructors<int>),
              "the satisfied constrained default constructor is viable");
static_assert(!__is_constructible(
                  ConstrainedMemberConstructors<ConstraintNoDefault>),
              "an unsatisfied constrained default constructor is not viable");
static_assert(__is_constructible(
                  ConstrainedMemberConstructors<ConstraintNoDefault>, int),
              "the satisfied constrained value constructor is viable");

int main() {
    ConstrainedMemberConstructors<int> default_constructed;
    ConstrainedMemberConstructors<ConstraintNoDefault> value_constructed(17);
    return constrained_add<3>(4) == 7 &&
                   constrained_scale<3>(4) == 10 &&
                   ConstrainedMemberOverloads<int>().select(4) == 7 &&
                   ConstrainedMemberOverloads<ConstraintNoDefault>().select(4) == 13 &&
                   ConstrainedMemberOverloads<int>::select_static(4) == 9 &&
                   ConstrainedMemberOverloads<ConstraintNoDefault>::select_static(4) == 15 &&
                   default_constructed.value == 5 &&
                   value_constructed.value == 17
               ? 0
               : 1;
}
