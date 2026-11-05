export module reachable_private;

struct ReachablePrivate {
    int value;

    friend int reachable_private_hidden_friend(ReachablePrivate value) {
        return value.value + 9;
    }
};

enum ReachablePrivateEnum {
    reachable_private_enum_value = 41
};

namespace reachable_detail {
struct ReachableScoped {
    int scoped_value;
};

int reachable_scoped_helper(int value) {
    return value + 6;
}
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

int reachable_overload(int value) {
    return value + 100;
}

int reachable_private_adl(ReachablePrivate value) {
    return value.value + 5;
}

export struct ReachableOverloadResult {
    char bytes[3];
};

export ReachableOverloadResult reachable_overload(double value) {
    ReachableOverloadResult result = {{'R', 'I', 'N'}};
    (void)value;
    return result;
}

export namespace reachable_public_detail {
struct ReachablePublicNested {
    int value;
};

int public_namespace_function(int value) {
    return value + 70;
}
}

export template<class T>
struct ReachablePublicTemplate {
    T value;
};

export template<class T>
using ReachablePublicAliasTemplate = ReachablePublicTemplate<T>;

export template<class T>
int reachable_public_identity(T value) {
    return value;
}

export using ReachableAlias = ReachablePrivate;
export using ReachableEnumAlias = ReachablePrivateEnum;
export using ReachableScopedAlias = reachable_detail::ReachableScoped;
export using ReachableNamespaceAliasType =
    reachable_alias_detail::ReachableScoped;
export using ReachableUsingType = ReachableScoped;
export using ReachableTemplateAlias = ReachableTemplate<int>;
