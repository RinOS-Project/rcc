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

struct ConstraintValueType {
    using value_type = int;
    int size() const { return 1; }
};

template<class T>
concept ConstraintObject = requires { typename T::value_type; };

template<class T>
concept ConstraintSmallObject = ConstraintObject<T> && requires(const T& value) {
    value.size();
};

template<class T>
concept ConstraintSized = sizeof(T) > 0;

template<class T>
concept ConstraintSmall = ConstraintSized<T> && sizeof(T) <= 4;

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
    int select_ordered() requires ConstraintObject<T> {
        return 19;
    }
    int select_ordered() requires ConstraintSmallObject<T> {
        return 23;
    }
    int select_unconstrained() {
        return 43;
    }
    int select_unconstrained() requires ConstraintObject<T> {
        return 47;
    }
    static int select_static_ordered() requires ConstraintObject<T> {
        return 29;
    }
    static int select_static_ordered() requires ConstraintSmallObject<T> {
        return 31;
    }
    int select_size_ordered() requires ConstraintSized<T> {
        return 37;
    }
    int select_size_ordered() requires ConstraintSmall<T> {
        return 41;
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
                   ConstrainedMemberOverloads<ConstraintValueType>()
                           .select_ordered() == 23 &&
                   ConstrainedMemberOverloads<ConstraintValueType>()
                           .select_unconstrained() == 47 &&
                   ConstrainedMemberOverloads<ConstraintNoDefault>()
                           .select_unconstrained() == 43 &&
                   ConstrainedMemberOverloads<ConstraintValueType>::
                           select_static_ordered() == 31 &&
                   ConstrainedMemberOverloads<int>().select_size_ordered() == 41 &&
                   default_constructed.value == 5 &&
                   value_constructed.value == 17
               ? 0
               : 1;
}
