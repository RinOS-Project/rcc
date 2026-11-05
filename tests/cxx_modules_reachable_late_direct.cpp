import reachable_bridge;
import reachable_private;

static_assert(sizeof(reachable_overload(1)) ==
              sizeof(ReachableOverloadResult));
static_assert(sizeof(reachable_public_detail::ReachablePublicNested) ==
              sizeof(int));

int main() {
    ReachableAlias value{};
    value.value = 19;
    reachable_public_detail::ReachablePublicNested nested{};
    nested.value = 27;
    ReachablePublicTemplate<int> public_template{};
    public_template.value = 31;
    return bridge_value(value) == 19 &&
            reachable_public_detail::public_namespace_function(5) == 75 &&
            nested.value == 27 && public_template.value == 31 ? 0 : 1;
}
