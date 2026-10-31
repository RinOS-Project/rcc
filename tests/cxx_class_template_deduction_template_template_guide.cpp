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

int main() {
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
    TemplateTemplateMultiGuideBox multi{pair};
    TemplateTemplateMultiGuideBox<GuidePair, long, int>* exact_multi = &multi;
    TemplateTemplateNontypeGuideBox nontype{array};
    TemplateTemplateNontypeGuideBox<GuideArray, int, 3>* exact_nontype =
        &nontype;
    TemplateTemplatePackGuideBox pack{1, source, source};
    TemplateTemplatePackGuideBox<GuideInner, GuideInner>* exact_pack = &pack;
    return exact->value.value == 17 &&
                   exact_defaulted->value.value == 19 &&
                   exact_pack_signature->value.value == 17 &&
                   exact_order->value == 2 &&
                   exact_multi->value.first == 23 &&
                   exact_multi->value.second == 41 &&
                   exact_nontype->value.values[0] == 2 &&
                   exact_nontype->value.values[2] == 5 &&
                   exact_pack->first.value == 17 &&
                   exact_pack->second.value == 17
        ? 0 : 1;
}
