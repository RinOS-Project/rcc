template<class T>
requires requires(T candidate) {
    candidate.accepts(0);
}
int reveal_member_call_requirement_friend(T& host);

class MemberCallRequirementFriendHost {
public:
    int accepts(int value) const { return value + 1; }

private:
    int value = 0;

    template<class U>
    requires requires(U probe) {
        probe.accepts(0);
    }
    friend int reveal_member_call_requirement_friend(U& host);
};

static_assert(requires(MemberCallRequirementFriendHost candidate) {
    candidate.accepts(0);
});

template<class T>
requires requires(T candidate) {
    candidate.accepts(0);
}
int call_member_with_requirement(T& host) {
    return host.accepts(0);
}

template<class V>
requires requires(V item) {
    item.accepts(0);
}
int reveal_member_call_requirement_friend(V& host) {
    return host.value;
}

int main() {
    MemberCallRequirementFriendHost host;
    return reveal_member_call_requirement_friend(host) == 0 &&
                   call_member_with_requirement(host) == 1
        ? 0
        : 1;
}
