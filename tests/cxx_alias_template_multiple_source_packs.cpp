template<int Left, int Right>
struct AliasValuePair {};

template<typename... Values>
struct AliasValueList {};

template<int... OwnerValues>
struct AliasValuePackOwner {
    template<int... AliasValues>
    using Zip = AliasValueList<
        AliasValuePair<OwnerValues * 2, AliasValues + 1>...>;
};

using ZippedAliasValues =
    AliasValuePackOwner<1, 2, 3>::Zip<4, 5, 6>;
using ExpectedAliasValues = AliasValueList<
    AliasValuePair<2, 5>, AliasValuePair<4, 6>, AliasValuePair<6, 7>>;
using AliasRightShiftValue = AliasValuePair<(8 >> 1), 4>;

int check_zipped_alias_values(ExpectedAliasValues*) {
    return 0;
}

int check_alias_right_shift(AliasValuePair<4, 4>*) {
    return 0;
}

int main() {
    ZippedAliasValues values;
    AliasRightShiftValue shifted;
    if (check_zipped_alias_values(&values) != 0) return 1;
    return check_alias_right_shift(&shifted);
}
