struct DependentAliasConstraintOwner {
    template <typename Type>
        requires (sizeof(Type) >= 2)
    using wide_type = Type;
};

template <typename Owner,
          typename Owner::template wide_type<char> Value>
struct InvalidDependentAliasConstraintValue {};

InvalidDependentAliasConstraintValue<DependentAliasConstraintOwner, 'x'>
    invalid_dependent_alias_constraint_value;
