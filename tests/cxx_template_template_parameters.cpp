template <typename Type>
struct wrapper {};

template <typename Left, typename Right,
          template <typename> class LeftTransform,
          template <typename> class RightTransform>
struct binary_template_template {};

template <typename Left, typename Right>
struct use_binary_template_template
    : binary_template_template<Left, Right, wrapper, wrapper> {};

template <typename From>
struct qualifier {
    template <typename To>
    using apply = To;
};

template <template <typename> class Transform>
struct template_template_holder {};

template <typename Type>
struct use_dependent_alias_template
    : template_template_holder<qualifier<Type>::template apply> {};

template<
    class, class, class, class, class, class, class, class, class, class, class,
    class, class, class, class, class, class, class, class, class, class,
    class, class, class, class, class, class, class, class, class, class, class, class>
struct wide_template_template_target {};

template<
    template<
        class, class, class, class, class, class, class, class, class, class,
        class, class, class, class, class, class, class, class, class, class,
        class, class, class, class, class, class, class, class, class, class,
        class, class, class> class Template>
struct wide_template_template_apply {
    using type = Template<
        int, int, int, int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int, int, int, int, int>;
};

using wide_template_template_result =
    typename wide_template_template_apply<wide_template_template_target>::type;

template<
    int, int, int, int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int, int, int, int>
struct wide_non_type_template_template_target {};

template<
    template<
        int, int, int, int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int, int, int, int> class Template>
struct wide_non_type_template_template_apply {
    using type = Template<
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>;
};

using wide_non_type_template_template_result =
    typename wide_non_type_template_template_apply<
        wide_non_type_template_template_target>::type;

template<typename First, typename... Rest>
struct TemplateTemplateTypePackBox {
    using first_type = First;
    char elements[sizeof...(Rest) + 1];
};

template<template<typename, typename...> class Template,
         typename First, typename... Rest>
struct TemplateTemplateNestedPackAlias {
    using type = Template<First, Rest...>;
};

template<typename T>
struct TemplateTemplateNestedPackSelection {
    static constexpr int selected = 0;
};

template<template<typename, typename...> class Template,
         typename First, typename... Rest>
struct TemplateTemplateNestedPackSelection<Template<First, Rest...>> {
    static constexpr int selected = 1;
    using first_type = First;
    static constexpr int rest_count = sizeof...(Rest);
};

using TemplateTemplateNestedPackAliasType =
    TemplateTemplateNestedPackAlias<TemplateTemplateTypePackBox,
                                    int, char, long>::type;
using TemplateTemplateNestedPackEmptyAliasType =
    TemplateTemplateNestedPackAlias<TemplateTemplateTypePackBox,
                                    int>::type;

static_assert(sizeof(use_binary_template_template<int, long>) == 1,
              "class templates accept compatible template-template arguments");
static_assert(sizeof(use_dependent_alias_template<int>) == 1,
              "dependent nested alias templates are template arguments");
static_assert(sizeof(wide_template_template_result) == 1,
              "wide template-template parameters substitute all arguments");
static_assert(sizeof(wide_non_type_template_template_result) == 1,
              "wide non-type template-template parameters substitute all arguments");
static_assert(sizeof(TemplateTemplateNestedPackAliasType) == 3,
              "nested template-ids expand type packs in aliases");
static_assert(sizeof(TemplateTemplateNestedPackEmptyAliasType) == 1,
              "nested template-ids accept an empty type-pack expansion");
static_assert(TemplateTemplateNestedPackSelection<
                  TemplateTemplateTypePackBox<int>>::selected == 1,
              "template-template partial specialization accepts an empty pack");
static_assert(TemplateTemplateNestedPackSelection<
                  TemplateTemplateTypePackBox<int>>::rest_count == 0,
              "template-template partial specialization records an empty pack");
static_assert(TemplateTemplateNestedPackSelection<
                  TemplateTemplateTypePackBox<int, char, long>>::selected ==
              1,
              "template-template partial specialization expands a type pack");
static_assert(sizeof(TemplateTemplateNestedPackSelection<
                  TemplateTemplateTypePackBox<int, char, long>>::first_type) ==
              sizeof(int),
              "template-template partial specialization deduces its fixed type");
static_assert(TemplateTemplateNestedPackSelection<
                  TemplateTemplateTypePackBox<int, char, long>>::rest_count ==
              2,
              "template-template partial specialization deduces its pack");
