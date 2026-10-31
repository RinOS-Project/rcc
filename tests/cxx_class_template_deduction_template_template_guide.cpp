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

int main() {
    GuideInner<int> source{17};
    TemplateTemplateGuideBox value{source};
    TemplateTemplateGuideBox<GuideInner>* exact = &value;
    return exact->value.value == 17 ? 0 : 1;
}
