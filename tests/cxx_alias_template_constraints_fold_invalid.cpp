template<int... Values>
struct AliasTransformedValues {
    unsigned char values[sizeof...(Values) + 1];
};

template<int OwnerValue>
struct AliasPackConstraintOwner {
    template<int... Values>
        requires (OwnerValue > 0 && ((Values > 0) && ...))
    using OwnerAdjustedAliasPack =
        AliasTransformedValues<(Values * 3 + OwnerValue)...>;
};

AliasPackConstraintOwner<0>::OwnerAdjustedAliasPack<1, 2>
    rejected_owner_alias_pack;

int main() {
    return 0;
}
