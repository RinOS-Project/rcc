union InactiveUnionMemberReference {
    int active;
    int inactive;
};

constexpr int read_inactive_union_member(
    InactiveUnionMemberReference& value) {
    return value.inactive;
}

constexpr int invalid_inactive_union_reference() {
    InactiveUnionMemberReference value{.active = 7};
    return read_inactive_union_member(value);
}

constexpr int invalid_inactive_union_value =
    invalid_inactive_union_reference();

static_assert(invalid_inactive_union_reference() == 7);
