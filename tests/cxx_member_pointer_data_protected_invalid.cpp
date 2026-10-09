struct MemberPointerProtectedDesignatorBase {
protected:
    int value;
};

struct MemberPointerProtectedDesignatorDerived
    : MemberPointerProtectedDesignatorBase {
    int invalid_protected_designator() {
        auto member = &MemberPointerProtectedDesignatorBase::value;
        return this->*member;
    }
};

struct MemberPointerFreeProtectedDesignatorBase {
protected:
    int value;
};

struct MemberPointerFreeProtectedDesignatorDerived
    : MemberPointerFreeProtectedDesignatorBase {
    friend int invalid_free_friend_protected_designator(
        MemberPointerFreeProtectedDesignatorDerived& object);
};

int invalid_free_friend_protected_designator(
    MemberPointerFreeProtectedDesignatorDerived& object) {
    auto member = &MemberPointerFreeProtectedDesignatorBase::value;
    return object.*member;
}
