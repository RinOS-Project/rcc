struct MemberPointerReferencePadding {
    int padding;
};

struct MemberPointerReferenceBase {
    int value;
};

struct MemberPointerReferenceDerived : MemberPointerReferencePadding,
                                       MemberPointerReferenceBase {};

struct MemberPointerReferenceSource {
    int MemberPointerReferenceBase::*member;

    operator int MemberPointerReferenceBase::*() const {
        return member;
    }
};

void receive_mutable_member_pointer_reference(
    int MemberPointerReferenceDerived::*& member);

int main() {
    MemberPointerReferenceSource source{&MemberPointerReferenceBase::value};
    receive_mutable_member_pointer_reference(source);
    return 0;
}
