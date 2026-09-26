static int text_equal(const char* left, const char* right) {
    int index = 0;
    while (left[index] && right[index] && left[index] == right[index]) {
        ++index;
    }
    return left[index] == right[index];
}

struct FunctionNameMember {
    int method(void) {
        return text_equal(__func__, "method") ? 0 : 1;
    }
};

extern "C" int probe_cxx_predefined_function(void) {
    const char* standard_name = __func__;
    const char* compatibility_name = __FUNCTION__;
    return text_equal(standard_name, "probe_cxx_predefined_function") &&
           text_equal(compatibility_name, "probe_cxx_predefined_function")
        ? 0 : 1;
}

extern "C" int probe_cxx_predefined_location(void) {
    int first_line = __LINE__;
    int second_line = __LINE__;
    const char* source_file = __FILE__;
    return second_line == first_line + 1 &&
           text_equal(source_file, "tests/predefined_function_identifiers.cpp")
        ? 0 : 1;
}

extern "C" int probe_cxx_nested_predefined_function(void) {
    FunctionNameMember member{};
    auto lambda = []() {
        return text_equal(__func__, "operator()") ? 0 : 1;
    };
    return member.method() + lambda();
}
