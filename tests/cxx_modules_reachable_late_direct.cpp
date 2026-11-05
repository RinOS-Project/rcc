import reachable_bridge;
import reachable_private;

int main() {
    ReachableAlias value{};
    value.value = 19;
    return bridge_value(value) == 19 ? 0 : 1;
}
