namespace adl_function_template_specialization {

struct Token {
    int value;
};

template <typename T>
int inspect(const T&) {
    return 100;
}

template <>
int inspect<Token>(const Token& token) {
    return token.value + 7;
}

}  // namespace adl_function_template_specialization

namespace nested_adl {
namespace inner {

struct NestedToken {
    int value;
};

template <typename T>
int inspect_nested(const T&) {
    return 100;
}

template <>
int inspect_nested<NestedToken>(const NestedToken& token) {
    return token.value + 19;
}

}  // namespace inner
}  // namespace nested_adl

int invoke_adl_function_template_specialization() {
    adl_function_template_specialization::Token token{5};
    return inspect(token);
}

int invoke_nested_adl_function_template_specialization() {
    nested_adl::inner::NestedToken token{5};
    return inspect_nested(token);
}

int main() {
    return invoke_adl_function_template_specialization() == 12 &&
                   invoke_nested_adl_function_template_specialization() == 24
        ? 0 : 1;
}
