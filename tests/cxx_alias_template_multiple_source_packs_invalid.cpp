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

using InvalidZippedAliasValues =
    AliasValuePackOwner<1, 2>::Zip<4, 5, 6>;

InvalidZippedAliasValues invalid_values;
