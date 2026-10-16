struct NestedMemberPointerTarget {
    int first;
    int second;
};

struct NestedMemberPointerKey {
    int NestedMemberPointerTarget::*member;
};

template <NestedMemberPointerKey Value>
struct NestedMemberPointerOwner {
    template <typename Element>
    struct Inner {
        int get(NestedMemberPointerTarget& object) const {
            return object.*Value.member;
        }

    };
};

template <NestedMemberPointerKey Value>
struct NestedMemberPointerNullOwner {
    template <typename Element>
    struct Inner {
        bool is_null() const {
            return Value.member == nullptr;
        }
    };
};

int main() {
    NestedMemberPointerTarget object;
    object.first = 17;
    object.second = 42;
    NestedMemberPointerOwner<
        NestedMemberPointerKey{&NestedMemberPointerTarget::first}>
        ::Inner<int> first;
    NestedMemberPointerOwner<
        NestedMemberPointerKey{&NestedMemberPointerTarget::second}>
        ::Inner<int> second;
    NestedMemberPointerNullOwner<NestedMemberPointerKey{nullptr}>::Inner<int>
        null_member;
    return first.get(object) == 17 && second.get(object) == 42 &&
            null_member.is_null()
        ? 0 : 1;
}
