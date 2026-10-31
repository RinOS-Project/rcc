template<typename... Types>
struct Pack {};

template<typename Left, typename Right>
struct Pair {};

template<typename First, typename Second>
struct Duo {};

template<typename Value>
struct Envelope {};

struct NotPair {};

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

int main() {
#if defined(RCC_TEST_INVALID_ZIPPED_PACK)
    return choose_zipped_independent_packs(NotPair{}) +
           choose_zipped_compound_packs(Pair<int, char>{}) +
           choose_zipped_nested_packs(NotPair{});
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
                       Envelope<Duo<long, double>>{}) == 22
        ? 0 : 1;
#endif
}
