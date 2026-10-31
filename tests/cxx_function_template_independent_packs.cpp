template<typename... Types>
struct Pack {};

template<typename Left, typename Right>
struct Pair {};

template<typename... Left, typename... Right>
int choose_nested_independent_packs(
    Pair<Pack<Left...>, Pack<Right...>>) {
    return static_cast<int>(sizeof...(Left) * 10 + sizeof...(Right));
}

template<typename... Left, typename... Right>
int choose_separate_independent_packs(Pack<Left...>, Pack<Right...>) {
    return static_cast<int>(sizeof...(Left) * 10 + sizeof...(Right));
}

int main() {
    using EmptyPair = Pair<Pack<>, Pack<> >;
    using UnequalPair = Pair<Pack<char>, Pack<int, long> >;
    using WidePair = Pair<Pack<int, char>, Pack<double, long, short> >;
    return choose_nested_independent_packs(EmptyPair{}) == 0 &&
                   choose_nested_independent_packs(UnequalPair{}) == 12 &&
                   choose_nested_independent_packs(WidePair{}) == 23 &&
                   choose_separate_independent_packs(
                       Pack<int, char>{}, Pack<double, long, short>{}) == 23
        ? 0 : 1;
}
