template <typename OwnerValue>
struct NestedMemberSpecializationRequiresExplicitOwnerHeader {
    template <typename Value>
    struct Inner {};
};

template <>
struct NestedMemberSpecializationRequiresExplicitOwnerHeader<int>::Inner<char> {};
