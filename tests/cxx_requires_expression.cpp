int main() {
    int value = 40;
    static_assert(requires {});
    static_assert(requires { value + 2; });
    static_assert(requires(int candidate) { candidate + 1; });
    static_assert(requires(int lhs, int rhs) { lhs + rhs; });
    static_assert(!requires { value.no_such_member; });
    return requires {} && requires { value + 2; } &&
                   requires(int candidate) { candidate + 1; } &&
                   !requires { value.no_such_member; }
        ? 0 : 1;
}
