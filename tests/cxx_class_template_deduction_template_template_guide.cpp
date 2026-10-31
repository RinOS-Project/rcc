template <typename Value>
struct GuideInner {
    Value value;
};

template <typename First, typename Second>
struct GuidePair {
    First first;
    Second second;
};

template <template <typename> class Container>
struct TemplateTemplateGuideBox {
    Container<int> value;
};

template <template <typename> class Container>
TemplateTemplateGuideBox(Container<int>) ->
    TemplateTemplateGuideBox<Container>;

template <template <typename, typename> class Container, typename First,
          typename Second>
struct TemplateTemplateMultiGuideBox {
    Container<First, Second> value;
};

template <template <typename, typename> class Container, typename First,
          typename Second>
TemplateTemplateMultiGuideBox(Container<First, Second>)
    -> TemplateTemplateMultiGuideBox<Container, First, Second>;

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
    GuidePair<long, int> pair{23, 41};
    TemplateTemplateGuideBox value{source};
    TemplateTemplateGuideBox<GuideInner>* exact = &value;
    TemplateTemplateMultiGuideBox multi{pair};
    TemplateTemplateMultiGuideBox<GuidePair, long, int>* exact_multi = &multi;
    TemplateTemplatePackGuideBox pack{1, source, source};
    TemplateTemplatePackGuideBox<GuideInner, GuideInner>* exact_pack = &pack;
    return exact->value.value == 17 &&
                   exact_multi->value.first == 23 &&
                   exact_multi->value.second == 41 &&
                   exact_pack->first.value == 17 &&
                   exact_pack->second.value == 17
        ? 0 : 1;
}
