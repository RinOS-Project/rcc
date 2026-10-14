struct TemplateTemplateAliasArgumentOwner {
    template <template <typename> class Container>
    using value_type = int;
};

template <typename Type>
struct SingleTemplateArgument {};

template <typename First, typename Second>
struct TwoTemplateArguments {};

template <typename Owner,
          typename Owner::template value_type<TwoTemplateArguments> Value>
struct InvalidTemplateTemplateAliasArgument {};

InvalidTemplateTemplateAliasArgument<TemplateTemplateAliasArgumentOwner, 1>
    invalid_template_template_alias_argument;
