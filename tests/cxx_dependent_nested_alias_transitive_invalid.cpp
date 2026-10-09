struct NestedAliasAmbiguousLeftRoot {
    using value_type = int;
};

struct NestedAliasAmbiguousRightRoot {
    using value_type = long;
};

struct NestedAliasAmbiguousLeft : NestedAliasAmbiguousLeftRoot {};
struct NestedAliasAmbiguousRight : NestedAliasAmbiguousRightRoot {};
struct NestedAliasAmbiguousLeaf : NestedAliasAmbiguousLeft,
                                  NestedAliasAmbiguousRight {};

struct ProtectedNestedAliasRoot {
protected:
    using value_type = short;
};

template <typename T>
struct NestedAliasBox {
    typename T::value_type value;
};

NestedAliasBox<NestedAliasAmbiguousLeaf> ambiguous;
NestedAliasBox<ProtectedNestedAliasRoot> inaccessible;

int main() {
    return 0;
}
