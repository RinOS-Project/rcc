template<typename... Types>
struct RepeatedPackExpansionBox {};

template<typename... Types>
int repeat_pack_expansion(
    RepeatedPackExpansionBox<Types..., int, Types...>,
    RepeatedPackExpansionBox<Types...>) {
    return static_cast<int>(sizeof...(Types));
}

int main() {
    if (repeat_pack_expansion(
            RepeatedPackExpansionBox<char, short, int, char, short>{},
            RepeatedPackExpansionBox<char, short>{}) != 2) {
        return 1;
    }
    if (repeat_pack_expansion(RepeatedPackExpansionBox<int>{},
                              RepeatedPackExpansionBox<>{}) != 0) {
        return 2;
    }
    return 0;
}
