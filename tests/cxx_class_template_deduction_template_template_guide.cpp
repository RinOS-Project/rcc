template <typename Value>
struct GuideInner {
    Value value;
};

template <template <typename> class Container>
struct TemplateTemplateGuideBox {
    Container<int> value;
};

template <template <typename> class Container>
TemplateTemplateGuideBox(Container<int>) ->
    TemplateTemplateGuideBox<Container>;

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
    TemplateTemplateGuideBox value{source};
    TemplateTemplateGuideBox<GuideInner>* exact = &value;
    TemplateTemplatePackGuideBox pack{1, source, source};
    TemplateTemplatePackGuideBox<GuideInner, GuideInner>* exact_pack = &pack;
    return exact->value.value == 17 && exact_pack->first.value == 17 &&
                   exact_pack->second.value == 17
        ? 0 : 1;
}
