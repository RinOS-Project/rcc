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
