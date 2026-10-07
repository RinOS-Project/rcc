struct MemberPointerPrivateOwnerRejected {
private:
    int private_value;
};

int MemberPointerPrivateOwnerRejected::*invalid_private_member =
    &MemberPointerPrivateOwnerRejected::private_value;

struct MemberPointerAmbiguousBase {
    int value;
};

struct MemberPointerPublicRoute : MemberPointerAmbiguousBase {};
struct MemberPointerPrivateRoute : private MemberPointerAmbiguousBase {};
struct MemberPointerAmbiguousDerived : MemberPointerPublicRoute,
                                      MemberPointerPrivateRoute {};

int read_ambiguous_member_pointer_object(
    MemberPointerAmbiguousDerived* object,
    int MemberPointerAmbiguousBase::*member) {
    return object->*member;
}

int MemberPointerAmbiguousDerived::*invalid_ambiguous_owner_conversion =
    &MemberPointerAmbiguousBase::value;

struct MemberPointerConstOwner {
    int value;
};

const int MemberPointerConstOwner::*const_member_pointer =
    &MemberPointerConstOwner::value;
int MemberPointerConstOwner::*invalid_const_member_pointer_conversion =
    const_member_pointer;

int invalid_member_pointer_xvalue_assignment(
    MemberPointerConstOwner& object,
    int MemberPointerConstOwner::*member) {
    return (static_cast<MemberPointerConstOwner&&>(object).*member) = 9;
}
