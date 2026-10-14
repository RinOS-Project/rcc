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

template <typename OwnerType>
struct NestedTemplateDependentChainAccessOwner {
    template <typename InnerType>
    struct Inner {
    private:
        template <typename LeafType>
        struct HiddenLeaf {
            LeafType value;
        };
    };
};

template <typename OwnerType>
struct NestedTemplateDependentChainAccessUse {
    using HiddenLeaf = typename NestedTemplateDependentChainAccessOwner<
        OwnerType>::template Inner<int>::template HiddenLeaf<long>;
};

using InaccessibleDependentTemplateOwnerChain =
    NestedTemplateDependentChainAccessUse<int>::HiddenLeaf;
