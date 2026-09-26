int main() {
    return requires(int value = 1) { value + 1; } ? 0 : 1;
}

static_assert(requires { { 1 } -> std::not_a_supported_constraint<int>; });
