struct InactiveUnionAggregate {
    int first;
    int second;
};

union InactiveUnionMemberReference {
    int active;
    int inactive;
    InactiveUnionAggregate aggregate;
};

constexpr int read_inactive_union_member(
    InactiveUnionMemberReference& value) {
    return value.inactive;
}

constexpr int invalid_inactive_union_reference() {
    InactiveUnionMemberReference value{.active = 7};
    return read_inactive_union_member(value);
}

constexpr int read_inactive_union_aggregate(
    InactiveUnionMemberReference& value) {
    return value.aggregate.first;
}

constexpr int invalid_inactive_union_aggregate_reference() {
    InactiveUnionMemberReference value{.active = 11};
    return read_inactive_union_aggregate(value);
}

constexpr int invalid_inactive_union_value =
    invalid_inactive_union_reference();
constexpr int invalid_inactive_union_aggregate_value =
    invalid_inactive_union_aggregate_reference();

static_assert(invalid_inactive_union_reference() == 7);
static_assert(invalid_inactive_union_aggregate_reference() == 11);
