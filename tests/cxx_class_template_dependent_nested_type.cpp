template <typename T>
struct NestedTypeBase {
    using value_type = T;

    T value;

    explicit NestedTypeBase(T initial) : value(initial) {}
};

template <typename T>
struct NestedTypeDerived : NestedTypeBase<T> {
    using value_type = T;

    explicit NestedTypeDerived(T initial) : NestedTypeBase<T>(initial) {}

    typename NestedTypeDerived<T>::value_type read() const {
        typename NestedTypeDerived<T>::value_type result = this->value;
        return result;
    }

    typename NestedTypeBase<T>::value_type read_base_type() const {
        return this->value;
    }
};

struct NestedTypeCarrier {
    using value_type = int;
};

struct NestedTypeChainInner {
    using value_type = long;
};

struct NestedTypeChainOuter {
    using value_type = NestedTypeChainInner;
};

template <typename T>
struct NestedTypeChain {
    typename T::value_type::value_type value;

    typename T::value_type::value_type read() const {
        return value;
    }
};

template <int OuterValue>
struct NestedNonTypeOwner {
    template <typename InnerType>
    struct Inner {
        template <typename LeafType>
        struct Leaf {
            int read() const {
                return OuterValue + sizeof(InnerType) + sizeof(LeafType);
            }
        };
    };
};

template <int OuterValue, typename InnerType, typename LeafType>
int read_nested_non_type_owner_chain() {
    typename NestedNonTypeOwner<OuterValue>::template Inner<InnerType>::
        template Leaf<LeafType> value;
    return value.read();
}

template <typename T>
struct NestedTemplateTemplateCarrier {
    using value_type = T;
};

template <template <typename> class ValueTemplate>
struct DirectTemplateTemplateOwner {
    using value_type = typename ValueTemplate<int>::value_type;
    value_type payload;
};

int read_direct_template_template_owner() {
    DirectTemplateTemplateOwner<NestedTemplateTemplateCarrier> value;
    return sizeof(value.payload);
}

template <template <typename> class OuterTemplate>
struct NestedTemplateTemplateOwner {
    template <typename InnerType>
    struct Inner {
        using value_type =
            typename OuterTemplate<InnerType>::value_type;
        value_type payload;

        template <typename LeafType>
        struct Leaf {
            using value_type =
                typename OuterTemplate<LeafType>::value_type;
            value_type payload;
        };
    };
};

template <typename InnerType>
int read_nested_template_template_owner_chain() {
    typename NestedTemplateTemplateOwner<
        NestedTemplateTemplateCarrier>::template Inner<InnerType> value;
    return sizeof(value.payload);
}

template <typename LeafType>
int read_deep_nested_template_template_owner_chain() {
    typename NestedTemplateTemplateOwner<
        NestedTemplateTemplateCarrier>::template Inner<char>::
        template Leaf<LeafType> value;
    return sizeof(value.payload);
}

template <typename... Types>
struct NestedOwnerTypePackStorage {
    unsigned char elements[sizeof...(Types) + 1];
};

template <typename OwnerType, typename LocalType>
struct NestedOwnerTypePackElement {
    OwnerType owner;
    LocalType local;
};

template <typename... OuterTypes>
struct NestedOwnerTypePack {
    template <typename InnerType>
    struct Inner {
        using value_type = NestedOwnerTypePackStorage<
            NestedOwnerTypePackElement<OuterTypes, InnerType>...>;
        value_type payload;

        template <typename LeafType>
        struct Leaf {
            using value_type = NestedOwnerTypePackStorage<
                NestedOwnerTypePackElement<OuterTypes, LeafType>...>;
            value_type payload;
        };
    };
};

template <typename... OuterTypes>
int read_nested_owner_type_pack() {
    typename NestedOwnerTypePack<OuterTypes...>::template Inner<char> value;
    return sizeof(value.payload);
}

template <typename... OuterTypes>
int read_deep_nested_owner_type_pack() {
    typename NestedOwnerTypePack<OuterTypes...>::template Inner<char>::
        template Leaf<long long> value;
    return sizeof(value.payload);
}

template <int Base, int... Values>
struct NestedOwnerValuePackStorage {
    unsigned char elements[Base + sizeof...(Values) + 1];
};

template <int ByteCount>
struct NestedOwnerValuePackBytes {
    unsigned char elements[ByteCount];
};

template <typename... Elements>
struct NestedOwnerValuePackAggregate : Elements... {};

template <typename OwnerType, int ByteCount>
struct NestedOwnerMixedPackElement {
    OwnerType owner;
    unsigned char elements[ByteCount];
};

template <typename... Elements>
struct NestedOwnerMixedPackAggregate : Elements... {};

template <int... OuterValues>
struct NestedOwnerValuePack {
    template <typename InnerType>
    struct Inner {
        using value_type = NestedOwnerValuePackStorage<1, OuterValues...>;
        value_type payload;

        template <typename LeafType>
        struct Leaf {
            using value_type = NestedOwnerValuePackStorage<1, OuterValues...>;
            value_type payload;
        };
    };
};

template <typename... OuterTypes>
struct NestedOwnerMixedPack {
    template <int... InnerValues>
    struct Inner {
        using value_type = NestedOwnerMixedPackAggregate<
            NestedOwnerMixedPackElement<OuterTypes, InnerValues>...>;
        value_type payload;

        int inner_payload_size() const { return sizeof(payload); }

        bool packed_widths_match() const {
            return ((sizeof(OuterTypes) == InnerValues) && ...);
        }
    };
};

bool nested_owner_fold_values_equal(int left, int right) {
    return left == right;
}

template <typename T>
struct NestedOwnerFoldOneByte {
    unsigned char value;
};

template <typename T>
struct NestedOwnerFoldTwoBytes {
    unsigned char value[2];
};

template <template <typename> class... OuterTemplates>
struct NestedOwnerTemplateTemplateFold {
    template <int... InnerValues>
    struct Inner {
        bool sizes_match() const {
            return ((sizeof(OuterTemplates<int>) == InnerValues) && ...);
        }
    };
};

template <int... OuterValues>
struct NestedOwnerValueFold {
    template <int... InnerValues>
    struct Inner {
        bool values_match() const {
            return ((OuterValues == InnerValues) && ...);
        }

        bool values_match_via_call() const {
            return (nested_owner_fold_values_equal(OuterValues, InnerValues) &&
                    ...);
        }
    };
};

int read_explicit_nested_owner_mixed_pack_layout() {
    using value_type = NestedOwnerMixedPackAggregate<
        NestedOwnerMixedPackElement<short, 1>,
        NestedOwnerMixedPackElement<char, 2>>;
    return sizeof(value_type);
}

template <int... OuterValues>
struct NestedOwnerTransformedValuePack {
    template <typename InnerType>
    struct Inner {
        using value_type = NestedOwnerValuePackAggregate<
            NestedOwnerValuePackBytes<OuterValues + 1>...>;
        value_type payload;

        template <typename LeafType>
        struct Leaf {
            using value_type = NestedOwnerValuePackAggregate<
                NestedOwnerValuePackBytes<OuterValues + 1>...>;
            value_type payload;
        };
    };
};

template <int... OuterValues>
int read_nested_owner_value_pack() {
    using inner_type = typename NestedOwnerValuePack<
        OuterValues...>::template Inner<short>;
    using value_type = typename inner_type::value_type;
    return sizeof(value_type);
}

template <int... OuterValues>
int read_deep_nested_owner_value_pack() {
    using leaf_type = typename NestedOwnerValuePack<
        OuterValues...>::template Inner<char>::template Leaf<long long>;
    using value_type = typename leaf_type::value_type;
    return sizeof(value_type);
}

template <int... OuterValues>
int read_nested_owner_transformed_value_pack() {
    typename NestedOwnerTransformedValuePack<OuterValues...>::template
        Inner<short> value;
    return sizeof(value.payload);
}

template <int... OuterValues>
int read_deep_nested_owner_transformed_value_pack() {
    typename NestedOwnerTransformedValuePack<OuterValues...>::template
        Inner<char>::template Leaf<long long> value;
    return sizeof(value.payload);
}

template <typename T>
typename T::value_type::value_type function_template_nested_type_chain() {
    return 43;
}

template <typename T>
struct TypeParameterNestedType {
    typename T::value_type value;

    explicit TypeParameterNestedType(typename T::value_type initial)
        : value(initial) {}

    typename T::value_type read() const {
        typename T::value_type result = this->value;
        return result;
    }
};

static int dmi_effect_count;

static int dmi_adjust(int value) {
    ++dmi_effect_count;
    return value + 2;
}

template <typename T>
struct DmiBase {};

template <typename T>
struct DmiEffects : DmiBase<T> {
    T seed = 5;
    T called = dmi_adjust(5);
    T side_effect = (++dmi_effect_count, 8);
    T values[2] = { 7, 11 };
    T matrix[2][3] = { { 1, 3, 5 }, { 7, 9, 11 } };
};

static int dmi_cleanup_count;

struct DmiCleanupMember {
    int value;

    explicit DmiCleanupMember(int initial) : value(initial) {}

    ~DmiCleanupMember() {
        dmi_cleanup_count += value;
    }
};

template <typename T>
struct DmiCleanup : DmiBase<T> {
    DmiCleanupMember member{13};
};

int main() {
    using empty_mixed_pack_inner_owner =
        typename NestedOwnerMixedPack<>::template Inner<>;
    using two_element_mixed_pack_inner_owner =
        typename NestedOwnerMixedPack<short, char>::template Inner<1, 2>;
    using matching_mixed_pack_inner_owner =
        typename NestedOwnerMixedPack<short, char>::template Inner<2, 1>;
    empty_mixed_pack_inner_owner empty_mixed_pack;
    two_element_mixed_pack_inner_owner two_element_mixed_pack;
    matching_mixed_pack_inner_owner matching_mixed_pack;
    using matching_owner_value_fold =
        NestedOwnerValueFold<3, 7>::Inner<3, 7>;
    using empty_owner_value_fold = NestedOwnerValueFold<>::Inner<>;
    matching_owner_value_fold matching_value_fold;
    empty_owner_value_fold empty_value_fold;
    using matching_template_template_fold =
        NestedOwnerTemplateTemplateFold<NestedOwnerFoldOneByte,
                                        NestedOwnerFoldTwoBytes>::Inner<1, 2>;
    using empty_template_template_fold =
        NestedOwnerTemplateTemplateFold<>::Inner<>;
    matching_template_template_fold template_template_fold;
    empty_template_template_fold empty_template_template_fold_value;
    NestedTypeDerived<int> integer(17);
    NestedTypeDerived<long long> wide(29);
    TypeParameterNestedType<NestedTypeCarrier> type_parameter(37);
    NestedTypeChain<NestedTypeChainOuter> chained_type;
    chained_type.value = 41;
    dmi_effect_count = 0;
    DmiEffects<int> effects;
    DmiEffects<long long> wide_effects;
    dmi_cleanup_count = 0;
    {
        DmiCleanup<int> cleanup;
        if (cleanup.member.value != 13) return 2;
    }
    return integer.read() == 17 && integer.read_base_type() == 17 &&
                   wide.read() == 29 && wide.read_base_type() == 29 &&
                   type_parameter.read() == 37 &&
                   chained_type.read() == 41 &&
                   read_nested_non_type_owner_chain<17, char, long long>() == 26 &&
                   read_direct_template_template_owner() == 4 &&
                   read_nested_template_template_owner_chain<long long>() == 8 &&
                   read_deep_nested_template_template_owner_chain<
                       long long>() == 8 &&
                   read_nested_owner_type_pack<>() == 1 &&
                   read_nested_owner_type_pack<int, char, long long>() == 4 &&
                   read_deep_nested_owner_type_pack<>() == 1 &&
                   read_deep_nested_owner_type_pack<int, char, long long>() ==
                       4 &&
                   read_nested_owner_value_pack<>() == 2 &&
                   read_nested_owner_value_pack<3, 5, 8>() == 5 &&
                   read_deep_nested_owner_value_pack<>() == 2 &&
                   read_deep_nested_owner_value_pack<3, 5, 8>() == 5 &&
                   read_nested_owner_transformed_value_pack<>() == 1 &&
                   read_nested_owner_transformed_value_pack<1, 2, 3>() == 9 &&
                   read_deep_nested_owner_transformed_value_pack<>() == 1 &&
                   read_deep_nested_owner_transformed_value_pack<1, 2, 3>() ==
                       9 &&
                    empty_mixed_pack.inner_payload_size() == 1 &&
                    two_element_mixed_pack.inner_payload_size() == 8 &&
                    matching_mixed_pack.packed_widths_match() &&
                    matching_value_fold.values_match() &&
                    matching_value_fold.values_match_via_call() &&
                    empty_value_fold.values_match() &&
                    template_template_fold.sizes_match() &&
                    empty_template_template_fold_value.sizes_match() &&
                    read_explicit_nested_owner_mixed_pack_layout() == 8 &&
                   function_template_nested_type_chain<
                       NestedTypeChainOuter>() == 43 &&
                   dmi_effect_count == 4 && effects.called == 7 &&
                   effects.side_effect == 8 && effects.values[0] == 7 &&
                   effects.values[1] == 11 && effects.matrix[0][0] == 1 &&
                   effects.matrix[0][2] == 5 && effects.matrix[1][0] == 7 &&
                   effects.matrix[1][2] == 11 && wide_effects.called == 7 &&
                   wide_effects.side_effect == 8 &&
                   wide_effects.values[0] == 7 &&
                   wide_effects.values[1] == 11 &&
                   wide_effects.matrix[0][0] == 1 &&
                   wide_effects.matrix[0][2] == 5 &&
                   wide_effects.matrix[1][0] == 7 &&
                   wide_effects.matrix[1][2] == 11 && dmi_cleanup_count == 13
               ? 0
               : 1;
}
