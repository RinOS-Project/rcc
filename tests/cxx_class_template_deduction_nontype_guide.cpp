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

int main() {
    int source[3] = {4, 5, 6};
    ExtentGuideBox value(source);
    ExtentGuideBox<int, 3>* exact = &value;
    DefaultCountGuideBox defaulted(1);
    DefaultCountGuideBox<5>* exact_default = &defaulted;
    return sizeof(*exact) > 0 && sizeof(*exact_default) > 0 ? 0 : 1;
}
