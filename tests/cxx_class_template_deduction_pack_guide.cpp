int pack_guide_constructions = 0;

template <typename... Ts>
struct PackGuideBox {
    PackGuideBox(Ts...) { ++pack_guide_constructions; }
};

template <typename... Ts>
PackGuideBox(Ts...) -> PackGuideBox<Ts...>;

template <typename First, typename... Rest>
struct HeadedGuideBox {
    HeadedGuideBox(First, Rest...) { ++pack_guide_constructions; }
};

template <typename First, typename... Rest>
HeadedGuideBox(First, Rest...) -> HeadedGuideBox<First, Rest...>;

template <typename Value = int>
struct DefaultedGuideBox {
    DefaultedGuideBox() { ++pack_guide_constructions; }
};

int main() {
    PackGuideBox value(1, 2.0, 'x');
    int before_default_initialization = pack_guide_constructions;
    PackGuideBox empty;
    PackGuideBox<int, double, char>* exact = &value;
    PackGuideBox<>* empty_exact = &empty;

    HeadedGuideBox headed(1, 2.0, 'x');
    HeadedGuideBox<int, double, char>* headed_exact = &headed;
    DefaultedGuideBox defaulted;
    DefaultedGuideBox<int>* defaulted_exact = &defaulted;
    return sizeof(*exact) == sizeof(value) &&
                   sizeof(*empty_exact) == sizeof(empty) &&
                   sizeof(*headed_exact) == sizeof(headed) &&
                   sizeof(*defaulted_exact) == sizeof(defaulted) &&
                   pack_guide_constructions ==
                       before_default_initialization + 3
        ? 0 : 1;
}
