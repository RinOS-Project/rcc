struct MemberPointerUsingPrivateBase {
    int value;
};

struct MemberPointerUsingPrivateDerived : MemberPointerUsingPrivateBase {
private:
    using MemberPointerUsingPrivateBase::value;
};

int MemberPointerUsingPrivateBase::*private_member =
    &MemberPointerUsingPrivateDerived::value;

struct MemberPointerPrivateInheritanceBase {
    int inherited;
};

struct MemberPointerPrivateInheritanceDerived
    : private MemberPointerPrivateInheritanceBase {};

int MemberPointerPrivateInheritanceBase::*private_inherited_member =
    &MemberPointerPrivateInheritanceDerived::inherited;

struct MemberPointerProtectedInheritanceBase {
    int inherited;
};

struct MemberPointerProtectedInheritanceDerived
    : protected MemberPointerProtectedInheritanceBase {};

int MemberPointerProtectedInheritanceBase::*protected_inherited_member =
    &MemberPointerProtectedInheritanceDerived::inherited;
