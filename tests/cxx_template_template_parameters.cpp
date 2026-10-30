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

static_assert(sizeof(use_binary_template_template<int, long>) == 1,
              "class templates accept compatible template-template arguments");
static_assert(sizeof(use_dependent_alias_template<int>) == 1,
              "dependent nested alias templates are template arguments");
static_assert(sizeof(wide_template_template_result) == 1,
              "wide template-template parameters substitute all arguments");
static_assert(sizeof(wide_non_type_template_template_result) == 1,
              "wide non-type template-template parameters substitute all arguments");
