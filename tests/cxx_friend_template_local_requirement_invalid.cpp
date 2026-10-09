template<class T>
requires requires(T candidate) { candidate.marker; sizeof(candidate); }
int constrained_local_requirement_friend(T& host);

class ConstrainedLocalRequirementFriendHost {
public:
    int marker;

private:
    int value;

    template<class U>
    requires requires(U probe) { probe.marker; sizeof(probe); }
    friend int constrained_local_requirement_friend(U& host);
};

template<class V>
requires requires(V item) { item.marker; sizeof(item); }
int constrained_local_requirement_friend(V& host) {
    return host.value;
}

struct MissingLocalRequirementMember {
};

int main() {
    MissingLocalRequirementMember missing;
    return constrained_local_requirement_friend(missing);
}
