template<typename Left, typename Right>
struct AliasTypePair {};

template<typename... Values>
struct AliasTypeList {};

template<typename... OwnerTypes>
struct AliasTypePackOwner {
    template<typename... AliasTypes>
    using Zip = AliasTypeList<AliasTypePair<OwnerTypes, AliasTypes>...>;
};

using InvalidZippedAliasTypes =
    AliasTypePackOwner<int, char>::Zip<float, double, short>;

InvalidZippedAliasTypes invalid_types;
