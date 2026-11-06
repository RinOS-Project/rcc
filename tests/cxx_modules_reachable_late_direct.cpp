import reachable_bridge;
import reachable_private;

static_assert(sizeof(reachable_overload(1)) ==
              sizeof(ReachableOverloadResult));
static_assert(sizeof(reachable_public_detail::ReachablePublicNested) ==
              2 * sizeof(int));
static_assert(sizeof(ReachablePublicDefaultSize<>) == 4 * sizeof(int));
static_assert(sizeof(ReachablePublicQualifiedDefaultSize<>) ==
              5 * sizeof(int));

int main() {
    ReachableAlias value{};
    value.value = 19;
    reachable_public_detail::ReachablePublicNested nested{};
    nested.value = 27;
    ReachablePublicTemplate<int> public_template{};
    public_template.value = 31;
    ReachablePublicAliasTemplate<int> public_alias_template{};
    public_alias_template.value = 37;
    ReachableScoped promoted_using{};
    promoted_using.scoped_value = 39;
    ReachablePublicDefaultSize<> default_sized{};
    default_sized.values[3] = 43;
    ReachablePublicQualifiedDefaultSize<> qualified_default_sized{};
    qualified_default_sized.values[4] = 47;
    return bridge_value(value) == 19 &&
            reachable_public_detail::public_namespace_function(5) == 75 &&
            reachable_public_variable == 47 &&
            reachable_public_constrained(value) == 23 &&
            nested.value == 27 && public_template.value == 31 &&
            public_alias_template.value == 37 &&
            promoted_using.scoped_value == 39 &&
            default_sized.values[3] == 43 &&
            qualified_default_sized.values[4] == 47 &&
            reachable_public_identity(41) == 41 ? 0 : 1;
}
