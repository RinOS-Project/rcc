template<class T>
requires requires(T candidate) {
    candidate.accepts(0);
}
int reveal_member_call_requirement_mismatch(T& host);

class MemberCallRequirementMismatchHost {
public:
    int accepts(int value) const { return value + 1; }

private:
    int value = 0;

    template<class U>
    requires requires(U probe) {
        probe.accepts(0);
    }
    friend int reveal_member_call_requirement_mismatch(U& host);
};

template<class V>
requires requires(V item) {
    item.accepts(1);
}
int reveal_member_call_requirement_mismatch(V& host) {
    return host.value;
}

int main() {
    MemberCallRequirementMismatchHost host;
    return reveal_member_call_requirement_mismatch(host);
}
