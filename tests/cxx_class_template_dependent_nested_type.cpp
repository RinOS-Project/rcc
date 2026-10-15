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
