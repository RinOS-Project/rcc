template <typename OuterType>
struct NestedClassTemplateOwnerChain {
    template <typename InnerType>
    struct Inner {
        template <typename LeafType>
        struct Leaf {
            LeafType leaf;

            int value() const {
                return static_cast<int>(leaf);
            }
        };
    };
};

template <typename OuterType, typename InnerType, typename LeafType>
struct NestedClassTemplateOwnerChainAlias {
    using Value = typename NestedClassTemplateOwnerChain<
        OuterType>::template Inner<InnerType>::template Leaf<LeafType>;
};

template <typename OuterType, typename InnerType, typename LeafType>
int make_nested_class_template_owner_chain_value(
    OuterType outer, InnerType inner, LeafType leaf) {
    typename NestedClassTemplateOwnerChain<
        OuterType>::template Inner<InnerType>::template Leaf<LeafType> value{};
    (void)outer;
    (void)inner;
    value.leaf = leaf;
    return value.value();
}

using IntegerOwnerChainValue =
    NestedClassTemplateOwnerChainAlias<int, long, short>::Value;
using LongOwnerChainValue =
    NestedClassTemplateOwnerChainAlias<long, int, long>::Value;

int main() {
    IntegerOwnerChainValue integer_owner{};
    LongOwnerChainValue long_owner{};
    integer_owner.leaf = 18;
    long_owner.leaf = 18;
    return integer_owner.value() == 18 &&
                   long_owner.value() == 18 &&
                   make_nested_class_template_owner_chain_value<int, long,
                       short>(11, 13, 18) == 18 &&
                   make_nested_class_template_owner_chain_value<long, int,
                       long>(10, 14, 18) == 18
        ? 0 : 1;
}
