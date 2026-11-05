import reachable_bridge;

int main() {
    BridgeAlias value{};
    value.value = 17;
    BridgeScopedAlias scoped{};
    scoped.scoped_value = 25;
    BridgeNamespaceAliasType aliased{};
    aliased.scoped_value = 27;
    BridgeUsingType introduced{};
    introduced.scoped_value = 29;
    BridgeTemplateAlias templated{};
    templated.value = 33;
    BridgePrivateDefaultTemplate<> defaulted{};
    defaulted.value.value = 37;
    return bridge_value(value) == 17 &&
            reachable_public_api::namespace_function(8) == 19 &&
            scoped.scoped_value == 25 &&
            aliased.scoped_value == 27 && introduced.scoped_value == 29 &&
            templated.value == 33 && defaulted.value.value == 37 &&
            defaulted.private_value_sum() == 41 &&
            bridge_private_hidden_friend(value) == 26 &&
            reachable_private_hidden_friend(value) == 26 &&
            bridge_private_default_argument() == 35 &&
            bridge_private_declaration_default() == 43 &&
            bridge_private_overload(8) == 108 &&
            bridge_private_template_helper(value) == 21 &&
            bridge_private_namespace_helper(8) == 14 &&
            bridge_private_helper(8) == 12 &&
            bridge_private_enum() == 41 && bridge_private_adl(value) == 22
        ? 0 : 1;
}
