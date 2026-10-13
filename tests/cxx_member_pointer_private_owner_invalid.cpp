struct PrivateMemberPointerOwnerBase {};

struct PrivateMemberPointerOwnerDerived : private PrivateMemberPointerOwnerBase {};

template<typename T>
int choose_private_member_pointer_owner(
    T, int PrivateMemberPointerOwnerDerived::*) {
    return 1;
}

int invalid_private_member_pointer_owner_template(
    int PrivateMemberPointerOwnerBase::*member) {
    return choose_private_member_pointer_owner(0, member);
}
