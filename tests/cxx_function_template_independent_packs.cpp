template<typename... Types>
struct Pack {};

template<typename Left, typename Right>
struct Pair {};

template<typename First, typename Second>
struct Duo {};

template<typename Value>
struct Envelope {};

template<typename Value, int Tag>
struct Tagged {};

struct NotPair {};
struct NotTagged {};

template<typename... Left, typename... Right>
int choose_nested_independent_packs(
    Pair<Pack<Left...>, Pack<Right...>>) {
    return static_cast<int>(sizeof...(Left) * 10 + sizeof...(Right));
}

template<typename... Left, typename... Right>
int choose_separate_independent_packs(Pack<Left...>, Pack<Right...>) {
    return static_cast<int>(sizeof...(Left) * 10 + sizeof...(Right));
}

template<typename... Left, typename... Right>
int choose_zipped_independent_packs(Pair<Left, Right>...) {
    return static_cast<int>(sizeof...(Left) * 10 + sizeof...(Right));
}

template<typename... Left, typename... Right>
int choose_zipped_compound_packs(Pair<Left*, Right>...) {
    return static_cast<int>(sizeof...(Left) * 10 + sizeof...(Right));
}

template<typename... Left, typename... Right>
int choose_zipped_nested_packs(Envelope<Duo<Left, Right>>...) {
    return static_cast<int>(sizeof...(Left) * 10 + sizeof...(Right));
}

template<typename... Types, int... Values>
int choose_zipped_mixed_packs(Tagged<Types, Values>...) {
    return static_cast<int>(sizeof...(Types) * 10 + sizeof...(Values));
}

int main() {
#if defined(RCC_TEST_INVALID_ZIPPED_PACK)
    return choose_zipped_independent_packs(NotPair{}) +
           choose_zipped_compound_packs(Pair<int, char>{}) +
           choose_zipped_nested_packs(NotPair{}) +
           choose_zipped_mixed_packs(NotTagged{});
#else
    using EmptyPair = Pair<Pack<>, Pack<> >;
    using UnequalPair = Pair<Pack<char>, Pack<int, long> >;
    using WidePair = Pair<Pack<int, char>, Pack<double, long, short> >;
    return choose_nested_independent_packs(EmptyPair{}) == 0 &&
                   choose_nested_independent_packs(UnequalPair{}) == 12 &&
                   choose_nested_independent_packs(WidePair{}) == 23 &&
                   choose_separate_independent_packs(
                       Pack<int, char>{}, Pack<double, long, short>{}) == 23 &&
                   choose_zipped_independent_packs() == 0 &&
                   choose_zipped_independent_packs(
                       Pair<int, char>{}, Pair<long, double>{}) == 22 &&
                   choose_zipped_compound_packs() == 0 &&
                   choose_zipped_compound_packs(
                       Pair<int*, char>{}, Pair<long*, double>{}) == 22 &&
                   choose_zipped_nested_packs() == 0 &&
                   choose_zipped_nested_packs(
                       Envelope<Duo<int, char>>{},
                       Envelope<Duo<long, double>>{}) == 22 &&
                   choose_zipped_mixed_packs() == 0 &&
                   choose_zipped_mixed_packs(
                       Tagged<int, 1>{}, Tagged<long, 2>{}) == 22
        ? 0 : 1;
#endif
}
