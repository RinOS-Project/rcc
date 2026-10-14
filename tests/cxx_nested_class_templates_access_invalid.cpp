class NestedTemplateAccess {
    template <typename T>
    struct Hidden {
        T value;
    };
};

NestedTemplateAccess::Hidden<int> inaccessible_value;

template <typename OwnerType>
class NestedTemplateAccessTemplateOwner {
    template <typename ValueType>
    struct Hidden {
        ValueType value;
    };
};

NestedTemplateAccessTemplateOwner<int>::Hidden<long>
    inaccessible_template_owner;

class NestedTemplateAccessPrivateOwner {
    template <typename ValueType>
    struct Hidden {
        ValueType value;
    };
};

struct NestedTemplateAccessPrivateDerived
    : NestedTemplateAccessPrivateOwner {
    using Hidden = typename NestedTemplateAccessPrivateOwner::Hidden<long>;
};

template <typename OwnerType>
struct NestedTemplateDependentAccessUse {
    using Hidden = typename NestedTemplateAccessTemplateOwner<
        OwnerType>::template Hidden<long>;
};

using InaccessibleDependentTemplateOwner =
    NestedTemplateDependentAccessUse<int>::Hidden;
