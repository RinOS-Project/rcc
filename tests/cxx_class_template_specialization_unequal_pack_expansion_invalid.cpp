template<typename T>
struct UnequalExpansionBox {};

template<typename... Types>
struct UnequalExpansionPack {
    static constexpr int kind = 0;
};

template<typename Head, typename... Tail>
struct UnequalExpansionPack<Head, UnequalExpansionBox<Tail>...> {
    static constexpr int kind = 1;
};

template<typename Head, typename... Tail>
struct UnequalExpansionPack<UnequalExpansionBox<Head>, Tail...> {
    static constexpr int kind = 2;
};

UnequalExpansionPack<UnequalExpansionBox<int>> ambiguous_single;
UnequalExpansionPack<UnequalExpansionBox<int>, UnequalExpansionBox<char>>
    ambiguous_multiple;
