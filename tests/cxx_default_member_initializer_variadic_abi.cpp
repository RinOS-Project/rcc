union DefaultInitializedVariadicUnionAbi {
    unsigned integer = 11u;
    float floating;
};

static_assert(__is_trivially_copyable(
    DefaultInitializedVariadicUnionAbi));
static_assert(!__is_trivial(DefaultInitializedVariadicUnionAbi));

unsigned read_default_initialized_variadic_union_abi(
    int marker, ...) noexcept {
    __builtin_va_list arguments;
    __builtin_va_start(arguments, marker);
    DefaultInitializedVariadicUnionAbi value = __builtin_va_arg(
        arguments, DefaultInitializedVariadicUnionAbi);
    __builtin_va_end(arguments);
    return value.integer;
}

int main() {
    DefaultInitializedVariadicUnionAbi value;
    value.integer = 19u;
    return read_default_initialized_variadic_union_abi(0, value) == 19u
        ? 0 : 1;
}
