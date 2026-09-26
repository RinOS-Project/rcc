#if !defined(__has_cpp_attribute)
#error "RCC++ must expose __has_cpp_attribute"
#endif

#if __has_cpp_attribute(nodiscard) != 201907L
#error "RCC++ must report its C++20 nodiscard support"
#endif

#if __has_cpp_attribute(deprecated) != 201309L
#error "RCC++ must report its deprecated support"
#endif

#if __has_cpp_attribute(maybe_unused)
#error "RCC++ must not claim unsupported attributes"
#endif

[[nodiscard]] int preprocessor_attribute_probe(int value) {
    return value;
}

[[deprecated]] int preprocessor_deprecated_probe(void) {
    return 0;
}

int main() {
    return preprocessor_attribute_probe(0) +
           preprocessor_deprecated_probe();
}
