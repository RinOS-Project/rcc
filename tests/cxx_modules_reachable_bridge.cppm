export module reachable_bridge;

import reachable_private;

export using BridgeAlias = ReachableAlias;
export using BridgeScopedAlias = ReachableScopedAlias;
export using BridgeNamespaceAliasType = ReachableNamespaceAliasType;
export using BridgeUsingType = ReachableUsingType;
export using BridgeTemplateAlias = ReachableTemplateAlias;

export inline int bridge_private_helper(int value) {
    return reachable_private_helper(value);
}

export int bridge_value(BridgeAlias value);
