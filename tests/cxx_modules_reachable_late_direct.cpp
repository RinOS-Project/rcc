import reachable_bridge;
import reachable_private;

static_assert(sizeof(reachable_overload(1)) ==
              sizeof(ReachableOverloadResult));

int main() {
    ReachableAlias value{};
    value.value = 19;
    return bridge_value(value) == 19 ? 0 : 1;
}
