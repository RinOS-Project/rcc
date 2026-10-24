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

static_assert(sizeof(use_binary_template_template<int, long>) == 1,
              "class templates accept compatible template-template arguments");
static_assert(sizeof(use_dependent_alias_template<int>) == 1,
              "dependent nested alias templates are template arguments");
