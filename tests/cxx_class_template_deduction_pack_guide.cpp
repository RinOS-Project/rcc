template <typename... Ts>
struct PackGuideBox {
    PackGuideBox(Ts...) {}
};

template <typename... Ts>
PackGuideBox(Ts...) -> PackGuideBox<Ts...>;

template <typename First, typename... Rest>
struct HeadedGuideBox {
    HeadedGuideBox(First, Rest...) {}
};

template <typename First, typename... Rest>
HeadedGuideBox(First, Rest...) -> HeadedGuideBox<First, Rest...>;

int main() {
    PackGuideBox value(1, 2.0, 'x');
    PackGuideBox empty{};
    PackGuideBox<int, double, char>* exact = &value;
    PackGuideBox<>* empty_exact = &empty;

    HeadedGuideBox headed(1, 2.0, 'x');
    HeadedGuideBox<int, double, char>* headed_exact = &headed;
    return sizeof(*exact) == sizeof(value) &&
                   sizeof(*empty_exact) == sizeof(empty) &&
                   sizeof(*headed_exact) == sizeof(headed)
        ? 0 : 1;
}
