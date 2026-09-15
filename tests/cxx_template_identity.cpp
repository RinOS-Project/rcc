template <typename T>
T identity(T value) {
    return value;
}

template <typename T>
auto inferred_identity(T value) {
    return value;
}

auto plain_identity(int value) {
    return value;
}

auto trailing_identity(int value) -> int {
    return value;
}

extern "C" int probe_template_identity(void) {
    return identity<int>(41) + identity(1) + inferred_identity(0) +
                   plain_identity(0) + trailing_identity(0) == 42
               ? 0
               : 1;
}
