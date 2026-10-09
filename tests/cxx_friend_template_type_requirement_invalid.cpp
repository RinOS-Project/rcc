template<class T>
requires requires { typename T::value_type; }
int constrained_type_requirement_friend(T& host);

class ConstrainedTypeRequirementFriendHost {
    int value;

    template<class U>
    requires requires { typename U::value_type; }
    friend int constrained_type_requirement_friend(U& host);
};

template<class V>
requires requires { typename V::value_type; }
int constrained_type_requirement_friend(V& host) {
    return host.value;
}

struct MissingTypeRequirement {
};

int main() {
    MissingTypeRequirement missing;
    return constrained_type_requirement_friend(missing);
}
