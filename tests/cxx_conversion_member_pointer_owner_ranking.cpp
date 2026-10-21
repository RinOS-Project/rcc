struct MemberPointerRankingRoot {
    int root_value;
};

struct MemberPointerRankingMiddlePrefix {
    int middle_prefix;
};

struct MemberPointerRankingMiddle : MemberPointerRankingMiddlePrefix,
                                   MemberPointerRankingRoot {
    int middle_value;
};

struct MemberPointerRankingLeafPrefix {
    int leaf_prefix;
};

struct MemberPointerRankingLeaf : MemberPointerRankingLeafPrefix,
                                 MemberPointerRankingMiddle {
    int leaf_value;
};

class MemberPointerRankingSource {
public:
    operator int MemberPointerRankingRoot::*() {
        return &MemberPointerRankingRoot::root_value;
    }

    operator int MemberPointerRankingMiddle::*() {
        return &MemberPointerRankingMiddle::middle_value;
    }
};

int read_member_pointer_value(
    const MemberPointerRankingLeaf& object,
    int MemberPointerRankingLeaf::*member) {
    return object.*member;
}

int main() {
    MemberPointerRankingSource source;
    MemberPointerRankingLeaf object;
    object.root_value = 17;
    object.middle_value = 23;

    int MemberPointerRankingLeaf::*member =
        static_cast<int MemberPointerRankingLeaf::*>(source);
    if (object.*member != 23) return 1;
    int argument_value = read_member_pointer_value(object, source);
    if (argument_value != 23) return argument_value == 17 ? 2 : 3;
    return 0;
}

#if defined(TEST_UNRELATED_MEMBER_POINTER_OWNERS_AMBIGUOUS)
struct UnrelatedMemberPointerLeft {
    int left_value;
};

struct UnrelatedMemberPointerRight {
    int right_value;
};

struct UnrelatedMemberPointerLeaf : UnrelatedMemberPointerLeft,
                                    UnrelatedMemberPointerRight {};

class UnrelatedMemberPointerSource {
public:
    operator int UnrelatedMemberPointerLeft::*() {
        return &UnrelatedMemberPointerLeft::left_value;
    }

    operator int UnrelatedMemberPointerRight::*() {
        return &UnrelatedMemberPointerRight::right_value;
    }
};

int reject_unrelated_member_pointer_owner_ranking() {
    UnrelatedMemberPointerSource source;
    int UnrelatedMemberPointerLeaf::*member =
        static_cast<int UnrelatedMemberPointerLeaf::*>(source);
    return member == nullptr;
}
#endif
