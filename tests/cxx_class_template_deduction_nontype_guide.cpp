template <typename Element, int Count>
struct ExtentGuideBox {
    ExtentGuideBox(Element (&source)[Count]) { (void)source; }
};

template <typename Element, int Count>
ExtentGuideBox(Element (&source)[Count]) -> ExtentGuideBox<Element, Count>;

template <int Count = 5>
struct DefaultCountGuideBox {
    DefaultCountGuideBox(int) {}
};

template <int Count = 5>
DefaultCountGuideBox(int) -> DefaultCountGuideBox<Count>;

template <int... Extents>
struct ExtentPackGuideBox {
    int marker;
    int (&short_source)[2];
    int (&long_source)[4];
};

template <int... Extents>
ExtentPackGuideBox(int, int (&...sources)[Extents])
    -> ExtentPackGuideBox<Extents...>;

template <int... Extents>
struct EmptyExtentPackGuideBox {
    int marker;
};

template <int... Extents>
EmptyExtentPackGuideBox(int, int (&...sources)[Extents])
    -> EmptyExtentPackGuideBox<Extents...>;

template <int Value>
struct IntegralGuideTag {};

template <int... Values>
struct IntegralTagPackGuideBox {
    int marker;
    IntegralGuideTag<2> first;
    IntegralGuideTag<4> second;
};

template <int... Values>
IntegralTagPackGuideBox(int, IntegralGuideTag<Values>...)
    -> IntegralTagPackGuideBox<Values...>;

int main() {
    int source[3] = {4, 5, 6};
    ExtentGuideBox value(source);
    ExtentGuideBox<int, 3>* exact = &value;
    DefaultCountGuideBox defaulted(1);
    DefaultCountGuideBox<5>* exact_default = &defaulted;
    int short_source[2] = {7, 8};
    int long_source[4] = {9, 10, 11, 12};
    ExtentPackGuideBox extent_pack{0, short_source, long_source};
    ExtentPackGuideBox<2, 4>* exact_extent_pack = &extent_pack;
    EmptyExtentPackGuideBox empty_extent_pack{1};
    EmptyExtentPackGuideBox<>* exact_empty_extent_pack = &empty_extent_pack;
    IntegralGuideTag<2> two_tag{};
    IntegralGuideTag<4> four_tag{};
    IntegralTagPackGuideBox tag_pack{1, two_tag, four_tag};
    IntegralTagPackGuideBox<2, 4>* exact_tag_pack = &tag_pack;
    return sizeof(*exact) > 0 && sizeof(*exact_default) > 0 &&
                   sizeof(*exact_extent_pack) > 0 &&
                   sizeof(*exact_empty_extent_pack) > 0 &&
                   sizeof(*exact_tag_pack) > 0
        ? 0 : 1;
}
