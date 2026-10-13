struct PrivateMemberPointerGlobalBase {
    int value;
};

struct PrivateMemberPointerGlobalDerived : private PrivateMemberPointerGlobalBase {};

int PrivateMemberPointerGlobalDerived::*invalid_private_owner_conversion =
    &PrivateMemberPointerGlobalBase::value;
