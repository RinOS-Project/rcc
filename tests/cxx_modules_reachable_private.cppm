export module reachable_private;

struct ReachablePrivateBase {
    int inherited_value;
};

constexpr int reachable_private_concept_minimum_size = 4;

template<class T>
concept ReachablePrivateConcept =
    sizeof(T) >= reachable_private_concept_minimum_size;

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
constexpr int reachable_private_default_size = 5;
constexpr int reachable_private_default_size_from_function() {
    return 6;
}

struct ReachableScoped {
    int scoped_value;
};

int reachable_scoped_helper(int value) {
    return value + 6;
}
}
namespace reachable_alias_detail = reachable_detail;
export using reachable_detail::ReachableScoped;

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

export struct ReachablePublicDerived : ReachablePrivateBase {
    int own_value;
};

export template<class T>
    requires ReachablePrivateConcept<T>
int reachable_public_constrained(T value) {
    return reachable_private_helper(value.value);
}

export template<int N = reachable_private_concept_minimum_size>
struct ReachablePublicDefaultSize {
    int values[N];
};

export template<int N = reachable_detail::reachable_private_default_size>
struct ReachablePublicQualifiedDefaultSize {
    int values[N];
};

export template<
    int N = reachable_detail::reachable_private_default_size_from_function()>
struct ReachablePublicQualifiedFunctionDefaultSize {
    int values[N];
};

export int reachable_public_variable = 47;

export ReachableOverloadResult reachable_overload(double value) {
    ReachableOverloadResult result = {{'R', 'I', 'N'}};
    (void)value;
    return result;
}

export namespace reachable_public_detail {
struct ReachablePublicNested {
    int value;
    int initialized_by_private_helper = reachable_private_helper(2);

    int private_helper_value() const {
        return reachable_private_helper(value);
    }
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
