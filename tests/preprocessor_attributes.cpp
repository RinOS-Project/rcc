#if !defined(__has_cpp_attribute)
#error "RCC++ must expose __has_cpp_attribute"
#endif

#if __has_cpp_attribute(nodiscard) != 201907L
#error "RCC++ must report its C++20 nodiscard support"
#endif

#if __has_cpp_attribute(deprecated) != 201309L
#error "RCC++ must report its deprecated support"
#endif

#if __has_cpp_attribute(maybe_unused) != 201603L
#error "RCC++ must report its maybe_unused support"
#endif

#if __has_cpp_attribute(fallthrough) != 201603L
#error "RCC++ must report its fallthrough support"
#endif

#if __has_cpp_attribute(likely) != 201803L
#error "RCC++ must report its likely support"
#endif

#if __has_cpp_attribute(unlikely) != 201803L
#error "RCC++ must report its unlikely support"
#endif

[[nodiscard]] int preprocessor_attribute_probe(int value) {
    return value;
}

[[deprecated]] int preprocessor_deprecated_probe(void) {
    return 0;
}

[[maybe_unused]] static int preprocessor_maybe_unused_probe = 1;

static int preprocessor_statement_attribute_probe(int value) {
    if (value > 0) {
        return value;
    }
    [[unlikely]] if (value < 0) {
        return -value;
    }
    switch (value) {
    case 0:
        [[fallthrough]];
    [[likely]] case 1:
        return 1;
    [[unlikely]] default:
        return 2;
    }
}

int main() {
    return preprocessor_attribute_probe(0) +
           preprocessor_deprecated_probe() +
           preprocessor_statement_attribute_probe(0) == 1 ? 0 : 1;
}
