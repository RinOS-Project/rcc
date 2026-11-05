template<auto Value>
struct TemplateTemplateValueBox {};

template<typename>
inline constexpr int template_template_value_partial = 0;

template<template<auto M> class Box, auto Value>
inline constexpr int template_template_value_partial<Box<Value>> = 1;

template<typename Owner>
struct TemplateTemplateNestedOwner {
    template<auto Value>
    struct Member {};
};

inline constexpr TemplateTemplateValueBox<37> template_template_value_box{};
inline constexpr TemplateTemplateNestedOwner<int>::Member<42>
    template_template_nested_member{};
inline constexpr TemplateTemplateNestedOwner<long>::Member<43>
    template_template_nested_member_long_owner{};

template<template<auto M> class Box, auto Value>
int template_template_value_function(Box<Value>) {
    return static_cast<int>(Value);
}

static_assert(template_template_value_partial<
                  TemplateTemplateValueBox<37>> == 1);
static_assert(template_template_value_partial<
                  TemplateTemplateNestedOwner<int>::Member<42>> == 1);
static_assert(template_template_value_partial<
                  TemplateTemplateNestedOwner<long>::Member<43>> == 1);
int main() {
    const int* value_box =
        &template_template_value_partial<TemplateTemplateValueBox<37>>;
    const int* nested_member =
        &template_template_value_partial<
            TemplateTemplateNestedOwner<int>::Member<42>>;
    const int* nested_member_long_owner =
        &template_template_value_partial<
            TemplateTemplateNestedOwner<long>::Member<43>>;
    return *value_box == 1 && *nested_member == 1 &&
                   *nested_member_long_owner == 1 &&
                   template_template_value_function(
                       template_template_value_box) == 37 &&
                   template_template_value_function(
                       template_template_nested_member) == 42 &&
                   template_template_value_function(
                       template_template_nested_member_long_owner) == 43
               ? 0
               : 1;
}
