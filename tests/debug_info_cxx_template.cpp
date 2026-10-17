struct DebugTemplatePayload {
    unsigned short value;
};

template <typename Element, int Count, int Bias, unsigned int Mask>
struct DebugTemplateBox {
    Element items[Count];
};

DebugTemplateBox<DebugTemplatePayload, 3, -7, 0xF0000000U> debug_template_box;

template <typename Head, typename... Tail>
struct DebugTemplatePack {
    Head head;
};

DebugTemplatePack<int> debug_template_empty_pack;
DebugTemplatePack<int, short, long> debug_template_multiple_pack;

template <typename Head, int... Values>
struct DebugTemplateSignedValuePack {
    Head head;
};

DebugTemplateSignedValuePack<int> debug_template_empty_value_pack;
DebugTemplateSignedValuePack<int, -2, 7> debug_template_signed_value_pack;

template <typename Head, unsigned int... Values>
struct DebugTemplateUnsignedValuePack {
    Head head;
};

DebugTemplateUnsignedValuePack<int, 0xF0000000U>
    debug_template_unsigned_value_pack;

template <typename ValueType, ValueType... Values>
struct DebugTemplateDependentValuePack {
    int marker;
};

struct DebugTemplateNestedValueOwner {
    using value_type = short;
};

struct DebugTemplateCompoundTypeOwner {
    using value_type = short;
    short member;
};

struct DebugTemplateCompoundPackShortOwner {
    using value_type = short;
    short member;
};

struct DebugTemplateCompoundPackLongOwner {
    using value_type = long;
    long member;
};

template <typename Owner>
typename Owner::value_type* debug_template_dependent_pointer_type(
    typename Owner::value_type* value) {
    return value;
}

template <typename Owner>
typename Owner::value_type& debug_template_dependent_reference_type(
    typename Owner::value_type& value) {
    return value;
}

template <typename Owner>
typename Owner::value_type Owner::*
debug_template_dependent_member_pointer_type(
    typename Owner::value_type Owner::* value) {
    return value;
}

int debug_template_dependent_compound_type_entry() {
    DebugTemplateCompoundTypeOwner owner{11};
    short value = 7;
    short* pointer = debug_template_dependent_pointer_type<
        DebugTemplateCompoundTypeOwner>(&value);
    short& reference = debug_template_dependent_reference_type<
        DebugTemplateCompoundTypeOwner>(value);
    short DebugTemplateCompoundTypeOwner::* member =
        debug_template_dependent_member_pointer_type<
            DebugTemplateCompoundTypeOwner>(
                &DebugTemplateCompoundTypeOwner::member);
    return pointer == &value && &reference == &value &&
           owner.*member == 11;
}

template <typename... Owners>
int debug_template_dependent_pointer_pack(
    typename Owners::value_type*... values) {
    return ((values != nullptr) + ... + 0);
}

template <typename... Owners>
int debug_template_dependent_reference_pack(
    typename Owners::value_type&... values) {
    return static_cast<int>(sizeof...(Owners));
}

template <typename... Owners>
int debug_template_dependent_member_pointer_pack(
    typename Owners::value_type Owners::*... values) {
    return ((values != nullptr) + ... + 0);
}

template <typename... Values>
int debug_template_pack_with_variadic_ellipsis(Values... values, ...) {
    return static_cast<int>(sizeof...(Values));
}

int debug_template_dependent_compound_pack_entry() {
    short short_value = 3;
    long long_value = 5;
    return debug_template_dependent_pointer_pack<
               DebugTemplateCompoundPackShortOwner,
               DebugTemplateCompoundPackLongOwner>(&short_value, &long_value) +
           debug_template_dependent_reference_pack<
               DebugTemplateCompoundPackShortOwner,
               DebugTemplateCompoundPackLongOwner>(short_value, long_value) +
           debug_template_dependent_member_pointer_pack<
               DebugTemplateCompoundPackShortOwner,
               DebugTemplateCompoundPackLongOwner>(
                   &DebugTemplateCompoundPackShortOwner::member,
                   &DebugTemplateCompoundPackLongOwner::member) +
           debug_template_pack_with_variadic_ellipsis<int>(7, 11);
}

struct DebugTemplateCompoundNestedValueLeaf {
    using nested_type = short;
};

struct DebugTemplateCompoundValueOwner {
    using value_type = DebugTemplateCompoundNestedValueLeaf;
};

template <typename Owner, typename Owner::value_type... Values>
struct DebugTemplateNestedValuePack {
    int marker;
};

DebugTemplateNestedValuePack<DebugTemplateNestedValueOwner, -4, 19>
    debug_template_nested_value_pack;

int debug_template_nested_value_pack_consumer(
    DebugTemplateNestedValuePack<DebugTemplateNestedValueOwner, -4, 19>* value) {
    return value ? value->marker : 0;
}

template <typename Owner, typename Owner::value_type::nested_type... Values>
struct DebugTemplateCompoundNestedValuePack {
    int marker;
};

DebugTemplateCompoundNestedValuePack<DebugTemplateCompoundValueOwner, -4, 19>
    debug_template_compound_nested_value_pack;

int debug_template_compound_nested_value_pack_consumer(
    DebugTemplateCompoundNestedValuePack<
        DebugTemplateCompoundValueOwner, -4, 19>* value) {
    return value ? value->marker : 0;
}

template <typename Owner, typename Owner::value_type Value>
struct DebugTemplateNestedScalarValue {
    int marker;
};

DebugTemplateNestedScalarValue<DebugTemplateNestedValueOwner, -6>
    debug_template_nested_scalar_value;

int debug_template_nested_scalar_value_consumer(
    DebugTemplateNestedScalarValue<DebugTemplateNestedValueOwner, -6>* value) {
    return value ? value->marker : 0;
}

template <typename Type>
struct DebugTemplateAliasTemplateArgument {};

struct DebugTemplateAliasTemplateValueOwner {
    template <typename Type, int Width = 4>
    using value_type_template = Type;

    template <typename Type, int Width>
    using explicit_value_type_template = Type;

    template <typename Type>
        requires (sizeof(Type) >= 2)
    using constrained_value_type_template = Type;

    template <template <typename> class Container>
    using template_value_type = int;

    template <template <typename> class Container =
                  DebugTemplateAliasTemplateArgument>
    using default_template_value_type = int;
};

template <typename Owner>
struct DebugTemplateNestedAliasChain {
    template <typename Type>
    using value_type = typename Owner::template value_type_template<Type>;
};

template <typename Owner>
struct DebugTemplateNestedAliasChain2 {
    template <typename Type>
    using value_type = typename Owner::template value_type<Type>;
};

template <template <typename> class Container>
using DebugTemplateStandaloneTemplateAliasValue = int;

DebugTemplateStandaloneTemplateAliasValue<DebugTemplateAliasTemplateArgument>
    debug_template_standalone_template_alias_value;

template <typename Owner,
          typename Owner::template value_type_template<short> Value>
struct DebugTemplateNestedAliasTemplateScalarValue {
    int marker;
};

DebugTemplateNestedAliasTemplateScalarValue<
    DebugTemplateAliasTemplateValueOwner, -9>
    debug_template_nested_alias_template_scalar_value;

int debug_template_nested_alias_template_scalar_value_consumer(
    DebugTemplateNestedAliasTemplateScalarValue<
        DebugTemplateAliasTemplateValueOwner, -9>* value) {
    return value ? value->marker : 0;
}

template <typename Owner,
          typename Owner::template value_type_template<int> Value>
int debug_template_function_nested_alias_template_scalar_value() {
    return static_cast<int>(Value);
}

int debug_template_function_nested_alias_template_scalar_value_entry() {
    return debug_template_function_nested_alias_template_scalar_value<
        DebugTemplateAliasTemplateValueOwner, -11>();
}

template <typename Owner,
          typename Owner::template explicit_value_type_template<short, 7>
              Value>
struct DebugTemplateNestedAliasTemplateNontypeScalarValue {
    int marker;
};

DebugTemplateNestedAliasTemplateNontypeScalarValue<
    DebugTemplateAliasTemplateValueOwner, -13>
    debug_template_nested_alias_template_nontype_scalar_value;

int debug_template_nested_alias_template_nontype_scalar_value_consumer(
    DebugTemplateNestedAliasTemplateNontypeScalarValue<
        DebugTemplateAliasTemplateValueOwner, -13>* value) {
    return value ? value->marker : 0;
}

template <typename Owner,
          typename Owner::template constrained_value_type_template<short>
              Value>
struct DebugTemplateNestedConstrainedAliasTemplateScalarValue {
    int marker;
};

DebugTemplateNestedConstrainedAliasTemplateScalarValue<
    DebugTemplateAliasTemplateValueOwner, -17>
    debug_template_nested_constrained_alias_template_scalar_value;

int debug_template_nested_constrained_alias_template_scalar_value_consumer(
    DebugTemplateNestedConstrainedAliasTemplateScalarValue<
        DebugTemplateAliasTemplateValueOwner, -17>* value) {
    return value ? value->marker : 0;
}

template <typename Owner,
          typename Owner::template template_value_type<
              DebugTemplateAliasTemplateArgument> Value>
struct DebugTemplateNestedTemplateTemplateAliasScalarValue {
    int marker;
};

DebugTemplateNestedTemplateTemplateAliasScalarValue<
    DebugTemplateAliasTemplateValueOwner, -19>
    debug_template_nested_template_template_alias_scalar_value;

int debug_template_nested_template_template_alias_scalar_value_consumer(
    DebugTemplateNestedTemplateTemplateAliasScalarValue<
        DebugTemplateAliasTemplateValueOwner, -19>* value) {
    return value ? value->marker : 0;
}

template <typename Owner,
          typename Owner::template default_template_value_type<> Value>
struct DebugTemplateNestedDefaultTemplateTemplateAliasScalarValue {
    int marker;
};

DebugTemplateNestedDefaultTemplateTemplateAliasScalarValue<
    DebugTemplateAliasTemplateValueOwner, -23>
    debug_template_nested_default_template_template_alias_scalar_value;

int debug_template_nested_default_template_template_alias_scalar_value_consumer(
    DebugTemplateNestedDefaultTemplateTemplateAliasScalarValue<
        DebugTemplateAliasTemplateValueOwner, -23>* value) {
    return value ? value->marker : 0;
}

template <typename Owner, template <typename> class Container,
          typename Owner::template template_value_type<Container> Value>
struct DebugTemplateNestedOuterTemplateTemplateAliasScalarValue {
    int marker;
};

DebugTemplateNestedOuterTemplateTemplateAliasScalarValue<
    DebugTemplateAliasTemplateValueOwner,
    DebugTemplateAliasTemplateArgument, -29>
    debug_template_nested_outer_template_template_alias_scalar_value;

int debug_template_nested_outer_template_template_alias_scalar_value_consumer(
    DebugTemplateNestedOuterTemplateTemplateAliasScalarValue<
        DebugTemplateAliasTemplateValueOwner,
        DebugTemplateAliasTemplateArgument, -29>* value) {
    return value ? value->marker : 0;
}

template <typename Owner,
          typename DebugTemplateNestedAliasChain<Owner>::template value_type<
              short> Value>
struct DebugTemplateNestedAliasChainScalarValue {
    int marker;
};

DebugTemplateNestedAliasChainScalarValue<
    DebugTemplateAliasTemplateValueOwner, -31>
    debug_template_nested_alias_chain_scalar_value;

int debug_template_nested_alias_chain_scalar_value_consumer(
    DebugTemplateNestedAliasChainScalarValue<
        DebugTemplateAliasTemplateValueOwner, -31>* value) {
    return value ? value->marker : 0;
}

template <typename Owner,
          typename DebugTemplateNestedAliasChain2<
              DebugTemplateNestedAliasChain<Owner>>::template value_type<
                  short> Value>
struct DebugTemplateNestedAliasChain2ScalarValue {
    int marker;
};

DebugTemplateNestedAliasChain2ScalarValue<
    DebugTemplateAliasTemplateValueOwner, -37>
    debug_template_nested_alias_chain2_scalar_value;

int debug_template_nested_alias_chain2_scalar_value_consumer(
    DebugTemplateNestedAliasChain2ScalarValue<
        DebugTemplateAliasTemplateValueOwner, -37>* value) {
    return value ? value->marker : 0;
}

DebugTemplateDependentValuePack<short>
    debug_template_dependent_empty_value_pack;
DebugTemplateDependentValuePack<short, -2, 7>
    debug_template_dependent_signed_value_pack;
DebugTemplateDependentValuePack<unsigned short, 0xF000U>
    debug_template_dependent_unsigned_value_pack;
DebugTemplateDependentValuePack<int, 7>
    debug_template_dependent_int_value_pack;

enum class DebugTemplateDependentValueEnum : short {
    negative = -3,
    positive = 12
};
DebugTemplateDependentValuePack<DebugTemplateDependentValueEnum,
                                DebugTemplateDependentValueEnum::negative,
                                DebugTemplateDependentValueEnum::positive>
    debug_template_dependent_enum_value_pack;

int debug_template_dependent_enum_value_pack_consumer(
    DebugTemplateDependentValuePack<
        DebugTemplateDependentValueEnum,
        DebugTemplateDependentValueEnum::negative,
        DebugTemplateDependentValueEnum::positive>* value) {
    return value ? value->marker : 0;
}

template <DebugTemplateDependentValueEnum Value>
struct DebugTemplateEnumValue {
    int marker;
};

DebugTemplateEnumValue<DebugTemplateDependentValueEnum::negative>
    debug_template_enum_value;

int debug_template_enum_value_consumer(
    DebugTemplateEnumValue<DebugTemplateDependentValueEnum::negative>* value) {
    return value ? value->marker : 0;
}

template <int... Values>
struct DebugTemplateEmptyNonTypePack {};

DebugTemplateEmptyNonTypePack<> debug_template_zero_parameter_pack;

template <typename Type>
struct DebugTemplateTemplateLeaf {
    Type value;
};

namespace debug_template_namespace {
template <typename Type>
struct Leaf {
    Type value;
};
}

template <template <typename> class Template>
struct DebugTemplateTemplateHolder {
    Template<int> member;
};

DebugTemplateTemplateHolder<DebugTemplateTemplateLeaf>
    debug_template_template_holder;
DebugTemplateTemplateHolder<debug_template_namespace::Leaf>
    debug_template_namespace_holder;

template <template <typename> class... Template>
struct DebugTemplateTemplatePackHolder {
    static constexpr int count = sizeof...(Template);
};

DebugTemplateTemplatePackHolder<DebugTemplateTemplateLeaf,
                               debug_template_namespace::Leaf>
    debug_template_template_pack_holder;
DebugTemplateTemplatePackHolder<DebugTemplateTemplateLeaf>
    debug_template_template_single_pack_holder;
DebugTemplateTemplatePackHolder<>
    debug_template_template_empty_pack_holder;
DebugTemplateTemplatePackHolder<DebugTemplateTemplateLeaf,
                               debug_template_namespace::Leaf>
    debug_template_template_pack_holder_again;
static_assert(DebugTemplateTemplatePackHolder<
                  DebugTemplateTemplateLeaf,
                  debug_template_namespace::Leaf>::count == 2);
static_assert(DebugTemplateTemplatePackHolder<
                  DebugTemplateTemplateLeaf>::count == 1);
static_assert(DebugTemplateTemplatePackHolder<>::count == 0);

int debug_template_template_entry() {
    return debug_template_template_holder.member.value;
}

int debug_template_template_pack_entry(
    DebugTemplateTemplatePackHolder<DebugTemplateTemplateLeaf,
                                    debug_template_namespace::Leaf>* value) {
    return value != nullptr;
}

int debug_template_template_fixed_entry(
    DebugTemplateTemplateHolder<debug_template_namespace::Leaf>* value) {
    return value != nullptr;
}

int debug_template_template_pack_count_entry() {
    return DebugTemplateTemplatePackHolder<DebugTemplateTemplateLeaf,
                                           debug_template_namespace::Leaf>::count == 2 &&
           DebugTemplateTemplatePackHolder<DebugTemplateTemplateLeaf>::count == 1 &&
           DebugTemplateTemplatePackHolder<>::count == 0;
}

DebugTemplateTemplateLeaf<int> debug_template_function_template_leaf;
debug_template_namespace::Leaf<int>
    debug_template_namespace_function_template_leaf;

template <template <typename> class Template, typename Value>
int debug_template_template_function(Template<Value>* value) {
    return value != nullptr;
}

int debug_template_template_function_entry() {
    return debug_template_template_function<DebugTemplateTemplateLeaf, int>(
               &debug_template_function_template_leaf) +
           debug_template_template_function<debug_template_namespace::Leaf,
                                            int>(
               &debug_template_namespace_function_template_leaf) +
           debug_template_template_function(
               &debug_template_function_template_leaf);
}

template <template <typename> class Template, typename Value>
int debug_template_template_repeated_function(Template<Value>* first,
                                              Template<Value>* second) {
    return first != nullptr && second != nullptr;
}

int debug_template_template_repeated_function_entry() {
    return debug_template_template_repeated_function<
        DebugTemplateTemplateLeaf, int>(&debug_template_function_template_leaf,
                                        &debug_template_function_template_leaf);
}

template <int Value>
struct DebugTemplateValueTemplateLeaf {
    int value;
};

template <template <int> class Template, int Value>
int debug_template_value_template_function(Template<Value>* value) {
    return value != nullptr;
}

DebugTemplateValueTemplateLeaf<7> debug_template_value_template_leaf;

int debug_template_value_template_function_entry() {
    return debug_template_value_template_function<
               DebugTemplateValueTemplateLeaf, 7>(
               &debug_template_value_template_leaf) +
           debug_template_value_template_function(
               &debug_template_value_template_leaf);
}

template <typename Value, int Bias>
int debug_template_function(Value value) {
    return (int)value + Bias;
}

int debug_template_entry() {
    return debug_template_function<unsigned short, -7>(
        debug_template_box.items[0].value);
}

template <typename Head, typename... Tail>
int debug_template_function_type_pack(Head head, Tail... values) {
    return static_cast<int>(head) + static_cast<int>(sizeof...(Tail));
}

int debug_template_function_type_pack_empty_entry() {
    return debug_template_function_type_pack<int>(1);
}

int debug_template_function_type_pack_multiple_entry() {
    return debug_template_function_type_pack<int, short, long>(1, 2, 3);
}

template <int Seed, int... Values>
int debug_template_function_signed_pack() {
    return Seed + static_cast<int>(sizeof...(Values));
}

int debug_template_function_signed_pack_empty_entry() {
    return debug_template_function_signed_pack<3>();
}

int debug_template_function_signed_pack_multiple_entry() {
    return debug_template_function_signed_pack<3, -2, 7>();
}

template <unsigned int... Values>
int debug_template_function_unsigned_pack() {
    return static_cast<int>(sizeof...(Values));
}

int debug_template_function_unsigned_pack_empty_entry() {
    return debug_template_function_unsigned_pack<>();
}

int debug_template_function_unsigned_pack_entry() {
    return debug_template_function_unsigned_pack<0xF0000000U>();
}

template <typename ValueType, ValueType... Values>
int debug_template_function_dependent_value_pack() {
    return static_cast<int>(sizeof...(Values));
}

int debug_template_function_dependent_value_pack_entry() {
    return debug_template_function_dependent_value_pack<short>() +
           debug_template_function_dependent_value_pack<short, -2, 7>() +
           debug_template_function_dependent_value_pack<unsigned short,
                                                        0xF000U>() +
           debug_template_function_dependent_value_pack<int, 7>() +
           debug_template_function_dependent_value_pack<
               DebugTemplateDependentValueEnum,
               DebugTemplateDependentValueEnum::negative,
               DebugTemplateDependentValueEnum::positive>();
}

template <typename Owner, typename Owner::value_type... Values>
int debug_template_function_nested_value_pack() {
    return static_cast<int>(sizeof...(Values));
}

int debug_template_function_nested_value_pack_entry() {
    return debug_template_function_nested_value_pack<
        DebugTemplateNestedValueOwner, -4, 19>();
}

template <typename Owner, typename Owner::value_type::nested_type... Values>
int debug_template_function_compound_nested_value_pack() {
    return static_cast<int>(sizeof...(Values));
}

int debug_template_function_compound_nested_value_pack_entry() {
    return debug_template_function_compound_nested_value_pack<
        DebugTemplateCompoundValueOwner, -4, 19>();
}

template <typename Owner, typename Owner::value_type Value>
int debug_template_function_nested_scalar_value() {
    return Value;
}

int debug_template_function_nested_scalar_value_entry() {
    return debug_template_function_nested_scalar_value<
        DebugTemplateNestedValueOwner, -6>();
}

template <DebugTemplateDependentValueEnum Value>
int debug_template_function_enum_value() {
    return static_cast<int>(Value);
}

int debug_template_function_enum_value_entry() {
    return debug_template_function_enum_value<
        DebugTemplateDependentValueEnum::positive>();
}
