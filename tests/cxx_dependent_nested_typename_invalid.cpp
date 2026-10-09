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

template <typename T>
struct NestedTypeMember {
    typename T::value_type value;
};

NestedTypeMember<MissingNestedType> missing;
NestedTypeMember<PrivateNestedType> inaccessible;
NestedTypeMember<AmbiguousNestedType> ambiguous;

int main() {
    return 0;
}
