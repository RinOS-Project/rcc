namespace adl_function_template_specialization {

struct Token {
    int value;
};

enum class Shade {
    Blue = 5
};

template <typename T>
int inspect(const T&) {
    return 100;
}

template <>
int inspect<Token>(const Token& token) {
    return token.value + 7;
}

template <>
int inspect<Shade>(const Shade& shade) {
    return static_cast<int>(shade) + 27;
}

}  // namespace adl_function_template_specialization

namespace nested_adl {
namespace inner {

struct NestedToken {
    int value;
};

enum class NestedShade {
    Green = 5
};

template <typename T>
int inspect_nested(const T&) {
    return 100;
}

template <typename T>
int inspect_nested_enum(const T&) {
    return 200;
}

template <>
int inspect_nested<NestedToken>(const NestedToken& token) {
    return token.value + 19;
}

template <>
int inspect_nested_enum<NestedShade>(const NestedShade& shade) {
    return static_cast<int>(shade) + 29;
}

}  // namespace inner
}  // namespace nested_adl

int invoke_adl_function_template_specialization() {
    adl_function_template_specialization::Token token{5};
    return inspect(token);
}

int invoke_adl_enum_function_template_specialization() {
    adl_function_template_specialization::Shade shade =
        adl_function_template_specialization::Shade::Blue;
    return inspect(shade);
}

int invoke_nested_adl_function_template_specialization() {
    nested_adl::inner::NestedToken token{5};
    return inspect_nested(token);
}

int invoke_nested_adl_enum_function_template_specialization() {
    nested_adl::inner::NestedShade shade =
        nested_adl::inner::NestedShade::Green;
    return inspect_nested_enum(shade);
}

int main() {
    return invoke_adl_function_template_specialization() == 12 &&
                   invoke_adl_enum_function_template_specialization() == 32 &&
                   invoke_nested_adl_function_template_specialization() == 24 &&
                   invoke_nested_adl_enum_function_template_specialization() == 34
        ? 0
        : 1;
}
