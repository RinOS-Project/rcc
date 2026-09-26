template<int N>
int nested_requirement_probe() {
    return requires { requires (N > 0); } ? 1 : 0;
}

int potentially_throwing();

template<typename T>
int compound_return_constraint_probe(T value) {
    return requires { { value } -> std::same_as<T>; } ? 1 : 0;
}

template<int N>
int compound_requirement_probe() {
    return requires { { N + 1 } noexcept; } ? 1 : 0;
}

int main() {
    int value = 40;
    static_assert(requires {});
    static_assert(requires { value + 2; });
    static_assert(requires(int candidate) { candidate + 1; });
    static_assert(requires(int lhs, int rhs) { lhs + rhs; });
    static_assert(!requires { value.no_such_member; });
    static_assert(requires { requires (1 < 2); });
    static_assert(!requires { requires (2 < 1); });
    static_assert(requires { { value + 2 }; });
    static_assert(requires { { value + 2 } noexcept; });
    static_assert(requires { { value + 2 } -> int; });
    static_assert(requires { { value + 2 } -> std::same_as<int>; });
    static_assert(requires { { value + 2 } -> std::convertible_to<long>; });
    static_assert(!requires { { value + 2 } -> bool; });
    static_assert(!requires { { value + 2 } -> std::same_as<bool>; });
    static_assert(!requires { { value + 2 } -> std::convertible_to<int*>; });
    static_assert(requires { { potentially_throwing() }; });
    static_assert(!requires { { potentially_throwing() } noexcept; });
    return requires {} && requires { value + 2; } &&
                   requires(int candidate) { candidate + 1; } &&
                   !requires { value.no_such_member; } &&
                   requires { requires (1 == 1); } &&
                   !requires { requires (1 == 0); } &&
                   requires { { value + 2 } -> std::same_as<int>; } &&
                   !requires { { value + 2 } -> std::same_as<bool>; } &&
                   compound_return_constraint_probe(value) == 1 &&
                   nested_requirement_probe<1>() == 1 &&
                   nested_requirement_probe<0>() == 0 &&
                   compound_requirement_probe<1>() == 1
        ? 0 : 1;
}
