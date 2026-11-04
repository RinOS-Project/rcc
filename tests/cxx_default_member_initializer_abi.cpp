union DefaultInitializedUnionAbi {
    unsigned integer = 11u;
    float floating;
};

struct DefaultInitializedMixedAggregateAbi {
    unsigned integer = 13u;
    double floating = 2.5;
};

static_assert(__is_trivially_copyable(DefaultInitializedUnionAbi));
static_assert(!__is_trivial(DefaultInitializedUnionAbi));
static_assert(__is_trivially_copyable(DefaultInitializedMixedAggregateAbi));
static_assert(!__is_trivial(DefaultInitializedMixedAggregateAbi));

DefaultInitializedUnionAbi load_union_impl_abi(
    const volatile DefaultInitializedUnionAbi* object) noexcept {
    DefaultInitializedUnionAbi result;
    __atomic_load(object, &result, 5);
    return result;
}

DefaultInitializedUnionAbi relay_union_abi(
    DefaultInitializedUnionAbi value) noexcept {
    return value;
}

DefaultInitializedMixedAggregateAbi relay_mixed_aggregate_abi(
    DefaultInitializedMixedAggregateAbi value) noexcept {
    return value;
}

int main() {
    DefaultInitializedUnionAbi storage{};
    DefaultInitializedUnionAbi observed = load_union_impl_abi(&storage);
    if (observed.integer != 11u) return 1;

    DefaultInitializedUnionAbi desired{.integer = 19u};
    __atomic_store(&storage, &desired, 5);
    observed = load_union_impl_abi(&storage);
    if (observed.integer != 19u) return 2;
    DefaultInitializedUnionAbi relayed = relay_union_abi(observed);
    if (relayed.integer != 19u) return 3;

    DefaultInitializedMixedAggregateAbi mixed{};
    if (mixed.integer != 13u || mixed.floating != 2.5) return 4;
    mixed.integer = 23u;
    mixed.floating = 4.5;
    DefaultInitializedMixedAggregateAbi relayed_mixed =
        relay_mixed_aggregate_abi(mixed);
    if (relayed_mixed.integer != 23u || relayed_mixed.floating != 4.5)
        return 5;
    return 0;
}
