export module reachable_private;

struct ReachablePrivate {
    int value;
};

namespace reachable_detail {
struct ReachableScoped {
    int scoped_value;
};
}
namespace reachable_alias_detail = reachable_detail;
using reachable_detail::ReachableScoped;

template<class T>
struct ReachableTemplate {
    T value;
};

int reachable_private_value = 4;

int reachable_private_helper(int value) {
    return value + reachable_private_value;
}

export using ReachableAlias = ReachablePrivate;
export using ReachableScopedAlias = reachable_detail::ReachableScoped;
export using ReachableNamespaceAliasType =
    reachable_alias_detail::ReachableScoped;
export using ReachableUsingType = ReachableScoped;
export using ReachableTemplateAlias = ReachableTemplate<int>;
