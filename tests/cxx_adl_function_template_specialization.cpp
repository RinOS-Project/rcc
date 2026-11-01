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

int consume_tokens(Token first, Token second) {
    return first.value + second.value;
}

int consume_shades(Shade first, Shade second) {
    return static_cast<int>(first) + static_cast<int>(second);
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

int consume_nested_tokens(NestedToken first, NestedToken second) {
    return first.value + second.value;
}

int consume_nested_shades(NestedShade first, NestedShade second) {
    return static_cast<int>(first) + static_cast<int>(second);
}

}  // namespace inner
}  // namespace nested_adl

namespace cross_namespace {

int consume_foreign_types(nested_adl::inner::NestedToken token,
                          nested_adl::inner::NestedShade shade) {
    return token.value + static_cast<int>(shade);
}

struct ForeignSignatureConsumer {
    int consume(nested_adl::inner::NestedToken token,
                nested_adl::inner::NestedShade shade);
};

int ForeignSignatureConsumer::consume(
    nested_adl::inner::NestedToken token,
    nested_adl::inner::NestedShade shade) {
    return token.value + static_cast<int>(shade);
}

}  // namespace cross_namespace

int invoke_adl_function_template_specialization() {
    adl_function_template_specialization::Token token{5};
    return inspect(token);
}

int invoke_adl_enum_function_template_specialization() {
    adl_function_template_specialization::Shade shade =
        adl_function_template_specialization::Shade::Blue;
    return inspect(shade);
}

int invoke_ordinary_namespaced_class_and_enum_functions() {
    adl_function_template_specialization::Token first{2};
    adl_function_template_specialization::Token second{3};
    const auto shade = adl_function_template_specialization::Shade::Blue;
    return adl_function_template_specialization::consume_tokens(first, second) +
           adl_function_template_specialization::consume_shades(shade, shade);
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

int invoke_ordinary_nested_class_and_enum_functions() {
    nested_adl::inner::NestedToken first{2};
    nested_adl::inner::NestedToken second{3};
    const auto shade = nested_adl::inner::NestedShade::Green;
    return nested_adl::inner::consume_nested_tokens(first, second) +
           nested_adl::inner::consume_nested_shades(shade, shade);
}

int invoke_foreign_namespace_types() {
    nested_adl::inner::NestedToken token{2};
    const auto shade = nested_adl::inner::NestedShade::Green;
    cross_namespace::ForeignSignatureConsumer consumer;
    return cross_namespace::consume_foreign_types(token, shade) +
           consumer.consume(token, shade);
}

int main() {
    return invoke_adl_function_template_specialization() == 12 &&
                   invoke_adl_enum_function_template_specialization() == 32 &&
                   invoke_ordinary_namespaced_class_and_enum_functions() == 15 &&
                   invoke_nested_adl_function_template_specialization() == 24 &&
                   invoke_nested_adl_enum_function_template_specialization() == 34 &&
                   invoke_ordinary_nested_class_and_enum_functions() == 15 &&
                   invoke_foreign_namespace_types() == 14
        ? 0
        : 1;
}
