int main() {
    int value = 40;
    static_assert(requires {});
    static_assert(requires { value + 2; });
    static_assert(!requires { value.no_such_member; });
    return requires {} && requires { value + 2; } &&
                   !requires { value.no_such_member; }
        ? 0 : 1;
}
