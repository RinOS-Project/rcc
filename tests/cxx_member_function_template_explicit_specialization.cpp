struct MemberFunctionSpecializationOwner {
    template <typename T>
    int value(T input);

    template <int Amount>
    int amount();

    template <typename T>
    int qualified(T input) const & noexcept;

    template <typename T>
    int inferred(T input);

    template <typename T>
    int overloaded(T input);

    template <typename T>
    int overloaded(T* input);

    template <typename T>
    int ellipsis(T input, ...);

    template <typename T>
    constexpr int constant(T input);

    template <typename T>
    consteval int immediate(T input);

    template <typename... Types>
    int packed(Types... values);
};

template <>
int MemberFunctionSpecializationOwner::value<int>(int input);

template <>
int MemberFunctionSpecializationOwner::value<int>(int input) {
    return input + 40;
}

template <>
int MemberFunctionSpecializationOwner::amount<7>();

template <>
int MemberFunctionSpecializationOwner::amount<7>() {
    return 42;
}

template <>
int MemberFunctionSpecializationOwner::packed<
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int
>(
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int
);

template <>
int MemberFunctionSpecializationOwner::packed<
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int
>(
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int
) {
    return 77;
}

template <>
int MemberFunctionSpecializationOwner::qualified<int>(int input) const &
    noexcept;

template <>
int MemberFunctionSpecializationOwner::qualified<int>(int input) const &
    noexcept {
    return input + 80;
}

template <>
int MemberFunctionSpecializationOwner::inferred(int input);

template <>
int MemberFunctionSpecializationOwner::inferred(int input) {
    return input + 50;
}

template <>
int MemberFunctionSpecializationOwner::overloaded<int>(int* input);

template <>
int MemberFunctionSpecializationOwner::overloaded<int>(int* input) {
    return *input + 90;
}

template <>
int MemberFunctionSpecializationOwner::ellipsis<int>(int input, ...);

template <>
int MemberFunctionSpecializationOwner::ellipsis<int>(int input, ...) {
    return input + 100;
}

template <>
int MemberFunctionSpecializationOwner::constant<int>(int input);

template <>
int MemberFunctionSpecializationOwner::constant<int>(int input) {
    return input + 60;
}

template <>
int MemberFunctionSpecializationOwner::immediate<int>(int input);

template <>
int MemberFunctionSpecializationOwner::immediate<int>(int input) {
    return input + 70;
}

template <typename T>
int MemberFunctionSpecializationOwner::value(T input) {
    return static_cast<int>(input) + 1;
}

template <int Amount>
int MemberFunctionSpecializationOwner::amount() {
    return Amount;
}

template <typename T>
int MemberFunctionSpecializationOwner::qualified(T input) const & noexcept {
    return static_cast<int>(input) + 5;
}

template <typename T>
int MemberFunctionSpecializationOwner::inferred(T input) {
    return static_cast<int>(input) + 2;
}

template <typename T>
int MemberFunctionSpecializationOwner::overloaded(T input) {
    return static_cast<int>(input) + 6;
}

template <typename T>
int MemberFunctionSpecializationOwner::overloaded(T* input) {
    return static_cast<int>(*input) + 7;
}

template <typename T>
int MemberFunctionSpecializationOwner::ellipsis(T input, ...) {
    return static_cast<int>(input) + 8;
}

template <typename T>
constexpr int MemberFunctionSpecializationOwner::constant(T input) {
    return static_cast<int>(input) + 3;
}

template <typename T>
consteval int MemberFunctionSpecializationOwner::immediate(T input) {
    return static_cast<int>(input) + 4;
}

int main() {
    MemberFunctionSpecializationOwner owner;
    const MemberFunctionSpecializationOwner& const_owner = owner;
    int pointed_value = 3;
    return owner.value(2) == 42 && owner.value(2.0) == 3 &&
                   owner.amount<7>() == 42 && owner.amount<8>() == 8 &&
                   owner.packed<
                       int, int, int, int, int, int, int, int,
                       int, int, int, int, int, int, int, int,
                       int, int, int, int, int, int, int, int,
                       int, int, int, int, int, int, int, int,
                       int
                   >(
                       0, 1, 2, 3, 4, 5, 6, 7,
                       8, 9, 10, 11, 12, 13, 14, 15,
                       16, 17, 18, 19, 20, 21, 22, 23,
                       24, 25, 26, 27, 28, 29, 30, 31,
                       32
                   ) == 77 &&
                   const_owner.qualified(2) == 82 &&
                   owner.inferred(2) == 52 && owner.inferred(2.0) == 4 &&
                   owner.overloaded(2) == 8 &&
                   owner.overloaded(&pointed_value) == 93 &&
                   owner.ellipsis(2, 3) == 102 &&
                   owner.constant(2) == 62 && owner.immediate(2) == 72
               ? 0
               : 1;
}
