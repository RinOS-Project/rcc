template <typename Value>
struct GuideInner {
    Value value;
};

template <typename Value, typename Policy = void>
struct GuideDefaultInner {
    Value value;
};

template <typename First, typename Second>
struct GuidePair {
    First first;
    Second second;
};

template <typename Value, int Count>
struct GuideArray {
    Value values[Count];
};

struct GenericGuideChoice {
    static constexpr int value = 1;
};

struct ConstGuideChoice {
    static constexpr int value = 2;
};

template <typename Choice>
struct GuideOrderBox {
    int value;

    GuideOrderBox(const int*) : value(Choice::value) {}
};

template <typename T>
GuideOrderBox(T*) -> GuideOrderBox<GenericGuideChoice>;

template <typename T>
GuideOrderBox(const T*) -> GuideOrderBox<ConstGuideChoice>;

template <typename Value>
struct NonFinalGuidePackBox {
    int marker;
    Value value;
};

template <typename... Ignored, typename Value>
NonFinalGuidePackBox(int, Ignored..., Value)
    -> NonFinalGuidePackBox<Value>;

template <typename... Values>
struct GuidePackList {};

template <typename First, typename Second>
struct GuidePackBundle {};

template <typename First, typename Second>
struct GuidePackPair {};

template <typename Left, typename Right>
struct IndependentGuidePackBox {
    Left first;
    Right second;
};

template <typename... Left, typename... Right>
IndependentGuidePackBox(GuidePackList<Left...>, GuidePackList<Right...>)
    -> IndependentGuidePackBox<GuidePackList<Left...>,
                               GuidePackList<Right...>>;

template <typename Left, typename Right>
struct ExpandedIndependentGuidePackBox {
    Left first;
    Right second;

    ExpandedIndependentGuidePackBox(GuidePackPair<int, long>,
                                    GuidePackPair<char, short>) {}
};

template <typename... Left, typename... Right>
ExpandedIndependentGuidePackBox(GuidePackPair<Left, Right>...)
    -> ExpandedIndependentGuidePackBox<GuidePackList<Left...>,
                                       GuidePackList<Right...>>;

template <typename Left, typename Right>
struct NestedExpansionGuideBox {
    Left first;
    Right second;

    NestedExpansionGuideBox(
        GuidePackBundle<GuidePackList<int, char>, GuidePackList<long>>) {}
    NestedExpansionGuideBox(
        GuidePackList<GuidePackPair<int, long>,
                      GuidePackPair<char, short>>) {}
};

template <typename... Left, typename... Right>
NestedExpansionGuideBox(
    GuidePackBundle<GuidePackList<Left...>, GuidePackList<Right...>>)
    -> NestedExpansionGuideBox<GuidePackList<Left...>,
                               GuidePackList<Right...>>;

template <typename... Left, typename... Right>
NestedExpansionGuideBox(GuidePackList<GuidePackPair<Left, Right>...>)
    -> NestedExpansionGuideBox<GuidePackList<Left...>,
                               GuidePackList<Right...>>;

template <typename Left, typename Right>
struct NestedExpansionLengthBox {
    NestedExpansionLengthBox(
        GuidePackBundle<GuidePackList<int, char>, GuidePackList<long>>,
        GuidePackList<GuidePackPair<int, long>,
                      GuidePackPair<char, short>>) {}
};

template <typename... Left, typename... Right>
NestedExpansionLengthBox(
    GuidePackBundle<GuidePackList<Left...>, GuidePackList<Right...>>,
    GuidePackList<GuidePackPair<Left, Right>...>)
    -> NestedExpansionLengthBox<GuidePackList<Left...>,
                                GuidePackList<Right...>>;

template <typename Value, int Count>
struct GuideValuePair {};

template <int... Values>
struct GuideValueList {
    char values[sizeof...(Values) + 1];
};

template <int Value>
struct GuideValueTag {};

template <typename Pack>
struct ArithmeticGuidePackBox {
    Pack value;

    template <typename First, typename Second>
    ArithmeticGuidePackBox(GuideValueList<2, 4>, First, Second) {}

    template <typename First>
    ArithmeticGuidePackBox(GuideValueList<2, 4>, First) {}
};

template <int... Values>
ArithmeticGuidePackBox(GuideValueList<Values...>,
                       GuideValueTag<(Values + 1)>...)
    -> ArithmeticGuidePackBox<GuideValueList<Values...>>;

template <typename Types, typename Counts>
struct MixedIndependentGuidePackBox {
    Types first;
    Counts second;

    MixedIndependentGuidePackBox(GuideValuePair<int, 3>,
                                 GuideValuePair<char, 5>) {}
};

template <typename... Types, int... Counts>
MixedIndependentGuidePackBox(GuideValuePair<Types, Counts>...)
    -> MixedIndependentGuidePackBox<GuidePackList<Types...>,
                                    GuideValueList<Counts...>>;

template <template <typename> class Container>
struct TemplateTemplateGuideBox {
    Container<int> value;
};

template <template <typename> class Container>
TemplateTemplateGuideBox(Container<int>) ->
    TemplateTemplateGuideBox<Container>;

template <template <typename...> class Container>
struct TemplateTemplatePackSignatureGuideBox {
    Container<int> value;
};

template <template <typename...> class Container>
TemplateTemplatePackSignatureGuideBox(Container<int>)
    -> TemplateTemplatePackSignatureGuideBox<Container>;

template <template <typename, typename> class Container, typename First,
          typename Second>
struct TemplateTemplateMultiGuideBox {
    Container<First, Second> value;
};

template <template <typename, typename> class Container, typename First,
          typename Second>
TemplateTemplateMultiGuideBox(Container<First, Second>)
    -> TemplateTemplateMultiGuideBox<Container, First, Second>;

template <template <typename, int> class Container, typename Value,
          int Count>
struct TemplateTemplateNontypeGuideBox {
    Container<Value, Count> value;
};

template <template <typename, int> class Container, typename Value,
          int Count>
TemplateTemplateNontypeGuideBox(Container<Value, Count>)
    -> TemplateTemplateNontypeGuideBox<Container, Value, Count>;

template <template <typename> class... Containers>
struct TemplateTemplatePackGuideBox {
    int marker;
    GuideInner<int> first;
    GuideInner<int> second;
};

template <template <typename> class... Containers>
TemplateTemplatePackGuideBox(int, Containers<int>...)
    -> TemplateTemplatePackGuideBox<Containers...>;

#if defined(RCC_ARITHMETIC_GUIDE_BAD_VALUE)
int main() {
    ArithmeticGuidePackBox invalid{
        GuideValueList<2, 4>{}, GuideValueTag<3>{}, GuideValueTag<6>{}};
    return sizeof(invalid.value.values);
}
#elif defined(RCC_ARITHMETIC_GUIDE_BAD_LENGTH)
int main() {
    ArithmeticGuidePackBox invalid{GuideValueList<2, 4>{}, GuideValueTag<3>{}};
    return sizeof(invalid.value.values);
}
#else
int main() {
#ifdef RCC_NESTED_GUIDE_BAD_ZIPPED_LENGTH
    NestedExpansionLengthBox invalid_zipped_expansion{
        GuidePackBundle<GuidePackList<int, char>, GuidePackList<long>>{},
        GuidePackList<GuidePackPair<int, long>,
                      GuidePackPair<char, short>>{}};
    (void)invalid_zipped_expansion;
#endif
    GuideInner<int> source{17};
    GuideDefaultInner<int> default_source{19};
    GuidePair<long, int> pair{23, 41};
    GuideArray<int, 3> array;
    array.values[0] = 2;
    array.values[1] = 3;
    array.values[2] = 5;
    TemplateTemplateGuideBox value{source};
    TemplateTemplateGuideBox<GuideInner>* exact = &value;
    TemplateTemplateGuideBox defaulted{default_source};
    TemplateTemplateGuideBox<GuideDefaultInner>* exact_defaulted =
        &defaulted;
    TemplateTemplatePackSignatureGuideBox pack_signature{source};
    TemplateTemplatePackSignatureGuideBox<GuideInner>* exact_pack_signature =
        &pack_signature;
    const int* order_source = nullptr;
    GuideOrderBox selected_order{order_source};
    GuideOrderBox<ConstGuideChoice>* exact_order = &selected_order;
    NonFinalGuidePackBox selected_nonfinal_pack{1, 43};
    NonFinalGuidePackBox<int>* exact_nonfinal_pack =
        &selected_nonfinal_pack;
    IndependentGuidePackBox selected_independent_packs{
        GuidePackList<int, char>{}, GuidePackList<long>{}};
    IndependentGuidePackBox<GuidePackList<int, char>, GuidePackList<long>>*
        exact_independent_packs = &selected_independent_packs;
    ExpandedIndependentGuidePackBox selected_expanded_packs{
        GuidePackPair<int, long>{}, GuidePackPair<char, short>{}};
    ExpandedIndependentGuidePackBox<GuidePackList<int, char>,
                                    GuidePackList<long, short>>*
        exact_expanded_packs = &selected_expanded_packs;
    NestedExpansionGuideBox selected_nested_expansions{
        GuidePackBundle<GuidePackList<int, char>, GuidePackList<long>>{}};
    NestedExpansionGuideBox<GuidePackList<int, char>, GuidePackList<long>>*
        exact_nested_expansions = &selected_nested_expansions;
    NestedExpansionGuideBox selected_zipped_expansion{
        GuidePackList<GuidePackPair<int, long>,
                      GuidePackPair<char, short>>{}};
    NestedExpansionGuideBox<GuidePackList<int, char>,
                            GuidePackList<long, short>>*
        exact_zipped_expansion = &selected_zipped_expansion;
    MixedIndependentGuidePackBox selected_mixed_packs{
        GuideValuePair<int, 3>{}, GuideValuePair<char, 5>{}};
    ArithmeticGuidePackBox selected_arithmetic_pack{
        GuideValueList<2, 4>{}, GuideValueTag<3>{}, GuideValueTag<5>{}};
    TemplateTemplateMultiGuideBox multi{pair};
    TemplateTemplateMultiGuideBox<GuidePair, long, int>* exact_multi = &multi;
    TemplateTemplateNontypeGuideBox nontype{array};
    TemplateTemplateNontypeGuideBox<GuideArray, int, 3>* exact_nontype =
        &nontype;
    TemplateTemplatePackGuideBox pack{1, source, source};
    TemplateTemplatePackGuideBox<GuideInner, GuideInner>* exact_pack = &pack;
    if (sizeof(selected_arithmetic_pack.value.values) != 3) return 3;
    return exact->value.value == 17 &&
                   exact_defaulted->value.value == 19 &&
                   exact_pack_signature->value.value == 17 &&
                   exact_order->value == 2 &&
                   exact_nonfinal_pack->marker == 1 &&
                   exact_nonfinal_pack->value == 43 &&
                   sizeof(exact_independent_packs->first) ==
                       sizeof(GuidePackList<int, char>) &&
                   sizeof(exact_expanded_packs->second) ==
                       sizeof(GuidePackList<long, short>) &&
                   sizeof(exact_nested_expansions->first) ==
                       sizeof(GuidePackList<int, char>) &&
                   sizeof(exact_nested_expansions->second) ==
                       sizeof(GuidePackList<long>) &&
                   sizeof(exact_zipped_expansion->first) ==
                       sizeof(GuidePackList<int, char>) &&
                   sizeof(exact_zipped_expansion->second) ==
                       sizeof(GuidePackList<long, short>) &&
                   sizeof(selected_mixed_packs.second) == 3 &&
                   exact_multi->value.first == 23 &&
                   exact_multi->value.second == 41 &&
                   exact_nontype->value.values[0] == 2 &&
                   exact_nontype->value.values[2] == 5 &&
                   exact_pack->first.value == 17 &&
                   exact_pack->second.value == 17
        ? 0 : 1;
}
#endif
