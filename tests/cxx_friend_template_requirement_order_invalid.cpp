template<class T>
requires requires(T candidate) {
    candidate.marker;
    typename T::value_type;
}
int ordered_requirement_friend(T& host);

class OrderedRequirementFriendHost {
public:
    using value_type = int;
    int marker;

private:
    int value;

    template<class U>
    requires requires(U probe) {
        typename U::value_type;
        probe.marker;
    }
    friend int ordered_requirement_friend(U& host);
};

template<class V>
requires requires(V item) {
    item.marker;
    typename V::value_type;
}
int ordered_requirement_friend(V& host) {
    return host.value;
}

int main() {
    OrderedRequirementFriendHost host;
    return ordered_requirement_friend(host);
}
