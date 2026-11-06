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

struct MemberFunctionPointerRankingRoot {
    void invoke() noexcept;
};

struct MemberFunctionPointerRankingMiddle
    : MemberFunctionPointerRankingRoot {};

struct MemberFunctionPointerRankingLeaf
    : MemberFunctionPointerRankingMiddle {};

struct MemberPointerRankingMiddleResult { char value; };
struct MemberPointerRankingLeafResult { char value[2]; };

MemberPointerRankingMiddleResult rank_data_member_owner(
    int MemberPointerRankingMiddle::*);
MemberPointerRankingLeafResult rank_data_member_owner(
    int MemberPointerRankingLeaf::*);

MemberPointerRankingMiddleResult rank_function_member_owner(
    void (MemberFunctionPointerRankingMiddle::*)() noexcept);
MemberPointerRankingLeafResult rank_function_member_owner(
    void (MemberFunctionPointerRankingLeaf::*)());

static_assert(sizeof(decltype(rank_data_member_owner(
                  &MemberPointerRankingRoot::root_value))) ==
              sizeof(MemberPointerRankingMiddleResult));
static_assert(sizeof(decltype(rank_function_member_owner(
                  &MemberFunctionPointerRankingRoot::invoke))) ==
              sizeof(MemberPointerRankingMiddleResult));

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
