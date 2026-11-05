export module reachable_bridge;

import reachable_private;

export namespace reachable_public_api {
int namespace_function(int value) {
    return value + 11;
}
}

export using BridgeAlias = ReachableAlias;
export using BridgeEnumAlias = ReachableEnumAlias;
export using BridgeScopedAlias = ReachableScopedAlias;
export using BridgeNamespaceAliasType = ReachableNamespaceAliasType;
export using BridgeUsingType = ReachableUsingType;
export using BridgeTemplateAlias = ReachableTemplateAlias;
export using BridgePublicNested =
    reachable_public_detail::ReachablePublicNested;

export template<class T = ReachablePrivate>
struct BridgePrivateDefaultTemplate {
    T value;

    int private_value_sum() const {
        return reachable_private_helper(value.value);
    }
};

export inline int bridge_private_helper(int value) {
    return reachable_private_helper(value);
}

export inline int bridge_private_hidden_friend(BridgeAlias value) {
    return reachable_private_hidden_friend(value);
}

export inline int bridge_private_namespace_helper(int value) {
    return reachable_detail::reachable_scoped_helper(value);
}

export inline int bridge_private_overload(int value) {
    return reachable_overload(value);
}

export inline int bridge_private_default_argument(
    int value = reachable_private_helper(31)) {
    return value;
}

export int bridge_private_declaration_default(
    int value = reachable_private_helper(39));

export template<class T>
inline int bridge_private_template_helper(T value) {
    return reachable_private_helper(value.value);
}

export inline int bridge_private_enum() {
    return reachable_private_enum_value;
}

export inline int bridge_private_adl(BridgeAlias value) {
    return reachable_private_adl(value);
}

export int bridge_value(BridgeAlias value);
