struct AmbiguousMemberPointerOwnerBase {
    int adjust(int value) { return value; }
};

struct AmbiguousMemberPointerOwnerLeft
    : AmbiguousMemberPointerOwnerBase {};
struct AmbiguousMemberPointerOwnerRight
    : AmbiguousMemberPointerOwnerBase {};
struct AmbiguousMemberPointerOwnerDerived
    : AmbiguousMemberPointerOwnerLeft, AmbiguousMemberPointerOwnerRight {};

int (AmbiguousMemberPointerOwnerBase::*base_method)(int) =
    &AmbiguousMemberPointerOwnerBase::adjust;

void form_ambiguous_owner_conversion() {
    int (AmbiguousMemberPointerOwnerDerived::*ambiguous_owner_method)(int) =
        base_method;
}
