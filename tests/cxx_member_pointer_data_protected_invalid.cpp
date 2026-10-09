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
