import reachable_bridge;
import reachable_private;

int private_namespace_function_must_remain_hidden() {
    return reachable_detail::reachable_scoped_helper(1);
}
