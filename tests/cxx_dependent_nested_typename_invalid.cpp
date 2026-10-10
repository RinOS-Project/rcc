struct MissingNestedType {};

struct PrivateNestedType {
private:
    using value_type = int;
};

struct FirstNestedType {
    using value_type = int;
};

struct SecondNestedType {
    using value_type = long;
};

struct AmbiguousNestedType : FirstNestedType, SecondNestedType {};

struct NestedAliasPrivateLeaf {
private:
    using nested_type = int;
};

struct NestedAliasPrivateOuter {
    using value_type = NestedAliasPrivateLeaf;
};

struct NestedAliasMissingLeaf {};

struct NestedAliasMissingOuter {
    using value_type = NestedAliasMissingLeaf;
};

template <typename T>
struct NestedTypeMember {
    typename T::value_type value;
};

template <typename T>
struct NestedTypeChainMember {
    typename T::value_type::nested_type value;
};

NestedTypeMember<MissingNestedType> missing;
NestedTypeMember<PrivateNestedType> inaccessible;
NestedTypeMember<AmbiguousNestedType> ambiguous;
NestedTypeChainMember<NestedAliasPrivateOuter> nested_inaccessible;
NestedTypeChainMember<NestedAliasMissingOuter> nested_missing;

int main() {
    return 0;
}
