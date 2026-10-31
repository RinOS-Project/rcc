template<int... Values>
struct ValuePack {};

template<typename Left, typename Right>
struct ValuePair {};

template<typename... Types>
struct TypePack {};

template<typename Left, typename Right>
struct MixedPair {};

template<int... Left, int... Right>
int choose_nested_nontype_packs(
    ValuePair<ValuePack<Left...>, ValuePack<Right...>>) {
    return static_cast<int>((0 + ... + Left) * 1000 +
                            sizeof...(Left) * 100 +
                            (0 + ... + Right) * 10 + sizeof...(Right));
}

template<int... Left, int... Right>
int choose_separate_nontype_packs(ValuePack<Left...>, ValuePack<Right...>) {
    return static_cast<int>((0 + ... + Left) * 1000 +
                            sizeof...(Left) * 100 +
                            (0 + ... + Right) * 10 + sizeof...(Right));
}

template<typename... Types, int... Values>
int choose_nested_mixed_packs(
    MixedPair<TypePack<Types...>, ValuePack<Values...>>) {
    return static_cast<int>(sizeof...(Types) * 100 + (0 + ... + Values));
}

int main() {
    using EmptyPair = ValuePair<ValuePack<>, ValuePack<> >;
    using LeftEmptyPair = ValuePair<ValuePack<>, ValuePack<7, 8> >;
    using RightEmptyPair = ValuePair<ValuePack<5>, ValuePack<> >;
    using WidePair = ValuePair<ValuePack<1, 2>, ValuePack<3, 4, 5> >;
    using EmptyMixedPair = MixedPair<TypePack<>, ValuePack<> >;
    using FilledMixedPair =
        MixedPair<TypePack<char, long>, ValuePack<4, 5, 6> >;
    return choose_nested_nontype_packs(EmptyPair{}) == 0 &&
                   choose_nested_nontype_packs(LeftEmptyPair{}) == 152 &&
                   choose_nested_nontype_packs(RightEmptyPair{}) == 5100 &&
                   choose_nested_nontype_packs(WidePair{}) == 3323 &&
                   choose_separate_nontype_packs(
                       ValuePack<6>{}, ValuePack<7, 8, 9>{}) == 6343 &&
                   choose_nested_mixed_packs(EmptyMixedPair{}) == 0 &&
                   choose_nested_mixed_packs(FilledMixedPair{}) == 215
        ? 0 : 1;
}
